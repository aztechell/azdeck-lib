#include "JsonParser.h"

#include "ChannelStore.h"
#include "Config.h"
#include "PendingSnapshot.h"

#include <ArduinoJson.h>
#include <math.h>

AzDeckJsonParseResult azdeckParseJson(
    char* data,
    size_t length,
    AzDeckChannelStore& store
) {
    if (data == nullptr || length == 0) {
        return AZDECK_JSON_MALFORMED;
    }

    StaticJsonDocument<AZDECK_JSON_DOC_SIZE> document;
    const DeserializationError error = deserializeJson(document, data, length);
    if (error != DeserializationError::Ok || !document.is<JsonObject>()) {
        return AZDECK_JSON_MALFORMED;
    }

    JsonObject object = document.as<JsonObject>();
    if (object.containsKey("type")) {
        return AZDECK_JSON_SERVICE;
    }
    if (object.size() == 0) {
        return AZDECK_JSON_EMPTY;
    }

    AzDeckPendingSnapshot snapshot;
    snapshot.clear();

    for (JsonPair kv : object) {
        const char* key = kv.key().c_str();
        if (key == nullptr || key[0] == '\0') {
            return AZDECK_JSON_MALFORMED;
        }

        const JsonVariant value = kv.value();
        if (value.is<JsonObject>() || value.is<JsonArray>() || value.isNull() ||
            value.is<bool>() || value.is<const char*>()) {
            return AZDECK_JSON_MALFORMED;
        }
        if (!(value.is<float>() || value.is<double>() || value.is<int>() ||
              value.is<long>() || value.is<unsigned int>() || value.is<unsigned long>())) {
            return AZDECK_JSON_MALFORMED;
        }

        const float number = value.as<float>();
        if (!isfinite(static_cast<double>(number))) {
            return AZDECK_JSON_MALFORMED;
        }
        if (!snapshot.add(key, number)) {
            return AZDECK_JSON_MALFORMED;
        }
    }

    if (snapshot.count == 0) {
        return AZDECK_JSON_EMPTY;
    }

    azdeckCommitSnapshot(store, snapshot);
    return AZDECK_JSON_CONTROL;
}
