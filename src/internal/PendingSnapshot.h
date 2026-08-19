#pragma once

#include "ChannelStore.h"
#include "Config.h"

#include <math.h>
#include <string.h>

struct AzDeckPendingSnapshot {
    struct Entry {
        char name[AZDECK_MAX_CHANNEL_NAME_LENGTH + 1];
        float value;
    };

    Entry entries[AZDECK_MAX_CHANNELS];
    int count;

    void clear() {
        count = 0;
    }

    bool add(const char* name, float value) {
        if (name == nullptr || name[0] == '\0') {
            return true;
        }
        const size_t n = strlen(name);
        if (n > AZDECK_MAX_CHANNEL_NAME_LENGTH) {
            return true;
        }
        if (!isfinite(static_cast<double>(value))) {
            return false;
        }
        if (count >= AZDECK_MAX_CHANNELS) {
            return true;
        }
        memcpy(entries[count].name, name, n);
        entries[count].name[n] = '\0';
        entries[count].value = value;
        ++count;
        return true;
    }
};

inline void azdeckCommitSnapshot(
    AzDeckChannelStore& store,
    const AzDeckPendingSnapshot& snapshot
) {
    for (int i = 0; i < snapshot.count; ++i) {
        store.set(snapshot.entries[i].name, snapshot.entries[i].value);
    }
}
