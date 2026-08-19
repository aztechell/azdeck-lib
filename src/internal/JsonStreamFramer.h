#pragma once

#include "Config.h"

#include <stddef.h>
#include <stdint.h>

class AzDeckJsonStreamFramer {
public:
    typedef void (*OnObject)(void* context, const char* data, size_t length);
    typedef void (*OnFail)(void* context);

    AzDeckJsonStreamFramer();

    void reset();
    void feed(
        const uint8_t* data,
        size_t length,
        OnObject onObject,
        OnFail onFail,
        void* context
    );

private:
    enum Action {
        kContinue,
        kEmit,
        kFail
    };

    Action consume(char c);
    static bool isSpace(char c);

    char buffer_[AZDECK_RX_BUFFER_SIZE];
    size_t length_;
    int depth_;
    bool inString_;
    bool escape_;
    bool started_;
};
