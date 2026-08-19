#pragma once

#include "Config.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

class AzDeckPacketQueue {
public:
    AzDeckPacketQueue() {
        reset();
    }

    void reset() {
        head_ = 0;
        tail_ = 0;
        count_ = 0;
        overflow_ = false;
        disconnect_ = false;
    }

    void push(const uint8_t* data, size_t length, uint8_t clientId = 0) {
        if (data == nullptr || length == 0) {
            return;
        }

        if (length >= AZDECK_RX_BUFFER_SIZE) {
            // Oversized packet: discard every pending snapshot, then failsafe.
            head_ = 0;
            tail_ = 0;
            count_ = 0;
            overflow_ = true;
            return;
        }

        if (count_ >= AZDECK_PACKET_QUEUE_DEPTH) {
            // Queue full: drop oldest snapshot, keep newest. Not failsafe.
            tail_ = static_cast<uint8_t>((tail_ + 1) % AZDECK_PACKET_QUEUE_DEPTH);
            --count_;
        }

        memcpy(slots_[head_].data, data, length);
        slots_[head_].data[length] = '\0';
        slots_[head_].len = length;
        slots_[head_].clientId = clientId;
        head_ = static_cast<uint8_t>((head_ + 1) % AZDECK_PACKET_QUEUE_DEPTH);
        ++count_;
    }

    bool take(
        char* buffer,
        size_t capacity,
        size_t* outLength,
        uint8_t* clientId,
        bool* overflow
    ) {
        if (overflow_) {
            overflow_ = false;
            if (overflow != nullptr) {
                *overflow = true;
            }
            return false;
        }
        if (overflow != nullptr) {
            *overflow = false;
        }
        if (count_ == 0 || buffer == nullptr || capacity == 0) {
            return false;
        }

        size_t n = slots_[tail_].len;
        if (n >= capacity) {
            n = capacity - 1;
        }
        memcpy(buffer, slots_[tail_].data, n);
        buffer[n] = '\0';
        if (outLength != nullptr) {
            *outLength = n;
        }
        if (clientId != nullptr) {
            *clientId = slots_[tail_].clientId;
        }
        tail_ = static_cast<uint8_t>((tail_ + 1) % AZDECK_PACKET_QUEUE_DEPTH);
        --count_;
        return true;
    }

    void markDisconnect() {
        disconnect_ = true;
    }

    bool takeDisconnect() {
        if (!disconnect_) {
            return false;
        }
        disconnect_ = false;
        return true;
    }

    uint8_t count() const {
        return count_;
    }

    bool overflowPending() const {
        return overflow_;
    }

private:
    struct Slot {
        char data[AZDECK_RX_BUFFER_SIZE];
        size_t len;
        uint8_t clientId;
    };

    Slot slots_[AZDECK_PACKET_QUEUE_DEPTH];
    uint8_t head_;
    uint8_t tail_;
    uint8_t count_;
    bool overflow_;
    bool disconnect_;
};
