#pragma once

#include "Config.h"

#include <stddef.h>

class AzDeckTelemetryStore {
public:
    AzDeckTelemetryStore();

    bool queue(const char* name, float value);
    bool queue(const char* name, const char* text);
    bool take(char* buffer, size_t capacity, size_t* outLength);

private:
    enum Kind {
        KindNumber,
        KindText
    };

    struct Slot {
        char name[AZDECK_MAX_CHANNEL_NAME_LENGTH + 1];
        char text[AZDECK_MAX_TELEMETRY_TEXT_LENGTH + 1];
        float value;
        Kind kind;
        bool used;
        bool dirty;
    };

    int find(const char* name) const;
    bool prepareSlot(const char* name, int* index);

    Slot slots_[AZDECK_MAX_TELEMETRY_CHANNELS];
    int count_;
};
