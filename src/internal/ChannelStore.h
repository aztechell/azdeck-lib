#pragma once

#include "Config.h"

class AzDeckChannelStore {
public:
    AzDeckChannelStore();

    bool set(const char* name, float value);
    float get(const char* name) const;
    void zeroValues();
    void clear();
    int count() const;

private:
    struct Slot {
        char name[AZDECK_MAX_CHANNEL_NAME_LENGTH + 1];
        float value;
        bool used;
    };

    int find(const char* name) const;

    Slot slots_[AZDECK_MAX_CHANNELS];
    int count_;
};
