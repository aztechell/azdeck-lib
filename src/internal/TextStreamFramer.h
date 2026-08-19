#pragma once

#include "Config.h"

#include <stddef.h>
#include <stdint.h>

class AzDeckTextStreamFramer {
public:
    typedef void (*OnLine)(void* context, const char* data, size_t length);
    typedef void (*OnFail)(void* context);

    AzDeckTextStreamFramer();

    void reset();
    bool append(const uint8_t* data, size_t length);
    void processAfterBurst(OnLine onLine, OnFail onFail, void* context);

private:
    void emitRange(
        size_t start,
        size_t end,
        OnLine onLine,
        void* context
    );

    char buffer_[AZDECK_RX_BUFFER_SIZE];
    size_t length_;
    bool seenLf_;
};
