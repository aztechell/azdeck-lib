#include "ChannelStore.h"

#include <string.h>

AzDeckChannelStore::AzDeckChannelStore() : count_(0) {
    clear();
}

void AzDeckChannelStore::clear() {
    count_ = 0;
    for (int i = 0; i < AZDECK_MAX_CHANNELS; ++i) {
        slots_[i].name[0] = '\0';
        slots_[i].value = 0.0f;
        slots_[i].used = false;
    }
}

int AzDeckChannelStore::count() const {
    return count_;
}

int AzDeckChannelStore::find(const char* name) const {
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

bool AzDeckChannelStore::set(const char* name, float value) {
    if (name == nullptr || name[0] == '\0') {
        return false;
    }

    const size_t n = strlen(name);
    if (n > AZDECK_MAX_CHANNEL_NAME_LENGTH) {
        return false;
    }

    const int existing = find(name);
    if (existing >= 0) {
        slots_[existing].value = value;
        return true;
    }

    if (count_ >= AZDECK_MAX_CHANNELS) {
        return false;
    }

    memcpy(slots_[count_].name, name, n);
    slots_[count_].name[n] = '\0';
    slots_[count_].value = value;
    slots_[count_].used = true;
    ++count_;
    return true;
}

float AzDeckChannelStore::get(const char* name) const {
    const int idx = find(name);
    if (idx < 0) {
        return 0.0f;
    }
    return slots_[idx].value;
}

void AzDeckChannelStore::zeroValues() {
    for (int i = 0; i < count_; ++i) {
        slots_[i].value = 0.0f;
    }
}
