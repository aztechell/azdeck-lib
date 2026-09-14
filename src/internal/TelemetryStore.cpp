#include "TelemetryStore.h"

#include <ArduinoJson.h>
#include <math.h>
#include <string.h>

AzDeckTelemetryStore::AzDeckTelemetryStore() : count_(0) {
    for (int i = 0; i < AZDECK_MAX_TELEMETRY_CHANNELS; ++i) {
        slots_[i].name[0] = '\0';
        slots_[i].text[0] = '\0';
        slots_[i].value = 0.0f;
        slots_[i].kind = KindNumber;
        slots_[i].used = false;
        slots_[i].dirty = false;
    }
}

int AzDeckTelemetryStore::find(const char* name) const {
    if (name == nullptr || name[0] == '\0') {
        return -1;
    }
    for (int i = 0; i < count_; ++i) {
        if (slots_[i].used && strcmp(slots_[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

bool AzDeckTelemetryStore::prepareSlot(const char* name, int* index) {
    if (name == nullptr || name[0] == '\0' || index == nullptr) {
        return false;
    }

    const size_t n = strlen(name);
    if (n > AZDECK_MAX_CHANNEL_NAME_LENGTH) {
        return false;
    }

    const int existing = find(name);
    if (existing >= 0) {
        *index = existing;
        return true;
    }

    if (count_ >= AZDECK_MAX_TELEMETRY_CHANNELS) {
        return false;
    }

    memcpy(slots_[count_].name, name, n);
    slots_[count_].name[n] = '\0';
    slots_[count_].used = true;
    *index = count_;
    ++count_;
    return true;
}

bool AzDeckTelemetryStore::queue(const char* name, float value) {
    if (!isfinite(static_cast<double>(value))) {
        return false;
    }

    int index = -1;
    if (!prepareSlot(name, &index)) {
        return false;
    }

    slots_[index].kind = KindNumber;
    slots_[index].value = value;
    slots_[index].text[0] = '\0';
    slots_[index].dirty = true;
    return true;
}

bool AzDeckTelemetryStore::queue(const char* name, const char* text) {
    if (text == nullptr) {
        return false;
    }

    const size_t n = strlen(text);
    if (n > AZDECK_MAX_TELEMETRY_TEXT_LENGTH) {
        return false;
    }

    int index = -1;
    if (!prepareSlot(name, &index)) {
        return false;
    }

    memcpy(slots_[index].text, text, n);
    slots_[index].text[n] = '\0';
    slots_[index].kind = KindText;
    slots_[index].value = 0.0f;
    slots_[index].dirty = true;
    return true;
}

bool AzDeckTelemetryStore::take(char* buffer, size_t capacity, size_t* outLength) {
    if (buffer == nullptr || capacity < 2) {
        return false;
    }

    bool anyDirty = false;
    for (int i = 0; i < count_; ++i) {
        if (slots_[i].used && slots_[i].dirty) {
            anyDirty = true;
            break;
        }
    }
    if (!anyDirty) {
        return false;
    }

#if defined(ESP8266)
    static StaticJsonDocument<AZDECK_JSON_DOC_SIZE> document;
    document.clear();
#else
    StaticJsonDocument<AZDECK_JSON_DOC_SIZE> document;
#endif
    document["type"] = "telemetry";

    int included[AZDECK_MAX_TELEMETRY_CHANNELS];
    int includedCount = 0;

    for (int i = 0; i < count_; ++i) {
        if (!slots_[i].used || !slots_[i].dirty) {
            continue;
        }

        if (slots_[i].kind == KindText) {
            document[slots_[i].name] = slots_[i].text;
        } else {
            document[slots_[i].name] = slots_[i].value;
        }
        const size_t needed = measureJson(document) + 1;
        if (needed > capacity) {
            document.remove(slots_[i].name);
            continue;
        }
        included[includedCount++] = i;
    }

    if (includedCount == 0) {
        return false;
    }

    const size_t written = serializeJson(document, buffer, capacity);
    if (written == 0 || written >= capacity) {
        return false;
    }
    buffer[written] = '\0';
    if (outLength != nullptr) {
        *outLength = written;
    }
    for (int i = 0; i < includedCount; ++i) {
        slots_[included[i]].dirty = false;
    }
    return true;
}
