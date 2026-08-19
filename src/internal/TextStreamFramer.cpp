#include "TextStreamFramer.h"

#include "Config.h"

#include <string.h>

AzDeckTextStreamFramer::AzDeckTextStreamFramer() {
    reset();
}

void AzDeckTextStreamFramer::reset() {
    length_ = 0;
    seenLf_ = false;
    buffer_[0] = '\0';
}

bool AzDeckTextStreamFramer::append(const uint8_t* data, size_t length) {
    if (data == nullptr || length == 0) {
        return true;
    }
    if (length_ + length >= AZDECK_RX_BUFFER_SIZE) {
        return false;
    }
    memcpy(buffer_ + length_, data, length);
    length_ += length;
    buffer_[length_] = '\0';
    return true;
}

void AzDeckTextStreamFramer::emitRange(
    size_t start,
    size_t end,
    OnLine onLine,
    void* context
) {
    if (onLine == nullptr || end <= start) {
        return;
    }
    onLine(context, buffer_ + start, end - start);
}

void AzDeckTextStreamFramer::processAfterBurst(
    OnLine onLine,
    OnFail onFail,
    void* context
) {
    (void)onFail;
    if (length_ == 0) {
        return;
    }

    bool sawLineFeed = false;
    size_t start = 0;
    for (size_t i = 0; i < length_; ++i) {
        if (buffer_[i] != '\n') {
            continue;
        }
        sawLineFeed = true;
        size_t end = i;
        if (end > start && buffer_[end - 1] == '\r') {
            --end;
        }
        emitRange(start, end, onLine, context);
        start = i + 1;
    }

    if (sawLineFeed) {
        seenLf_ = true;
        const size_t remain = length_ - start;
        if (remain > 0 && start > 0) {
            memmove(buffer_, buffer_ + start, remain);
        }
        length_ = remain;
        buffer_[length_] = '\0';
        return;
    }

    if (seenLf_) {
        return;
    }

    emitRange(0, length_, onLine, context);
    length_ = 0;
    buffer_[0] = '\0';
}
