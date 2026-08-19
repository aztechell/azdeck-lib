#include "JsonStreamFramer.h"

AzDeckJsonStreamFramer::AzDeckJsonStreamFramer() {
    reset();
}

void AzDeckJsonStreamFramer::reset() {
    length_ = 0;
    depth_ = 0;
    inString_ = false;
    escape_ = false;
    started_ = false;
}

bool AzDeckJsonStreamFramer::isSpace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

AzDeckJsonStreamFramer::Action AzDeckJsonStreamFramer::consume(char c) {
    if (!started_) {
        if (isSpace(c)) {
            return kContinue;
        }
        if (c != '{') {
            return kFail;
        }
        started_ = true;
        depth_ = 0;
        inString_ = false;
        escape_ = false;
        length_ = 0;
    }

    if (length_ >= AZDECK_RX_BUFFER_SIZE) {
        return kFail;
    }
    buffer_[length_++] = c;

    if (escape_) {
        escape_ = false;
        return kContinue;
    }

    if (inString_) {
        if (c == '\\') {
            escape_ = true;
            return kContinue;
        }
        if (c == '"') {
            inString_ = false;
        }
        return kContinue;
    }

    if (c == '"') {
        inString_ = true;
        return kContinue;
    }
    if (c == '{') {
        ++depth_;
        return kContinue;
    }
    if (c == '}') {
        --depth_;
        if (depth_ < 0) {
            return kFail;
        }
        if (depth_ == 0) {
            return kEmit;
        }
        return kContinue;
    }

    return kContinue;
}

void AzDeckJsonStreamFramer::feed(
    const uint8_t* data,
    size_t length,
    OnObject onObject,
    OnFail onFail,
    void* context
) {
    if (data == nullptr || length == 0) {
        return;
    }

    for (size_t i = 0; i < length; ++i) {
        const Action action = consume(static_cast<char>(data[i]));
        if (action == kFail) {
            if (onFail != nullptr) {
                onFail(context);
            }
            reset();
            continue;
        }
        if (action == kEmit) {
            if (onObject != nullptr && length_ > 0) {
                onObject(context, buffer_, length_);
            }
            reset();
        }
    }
}
