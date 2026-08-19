#include "Ping.h"

#include <string.h>

namespace {
const char kPingPrefix[] = "AZDECK_PING:";
const char kPongPrefix[] = "AZDECK_PONG:";
}  // namespace

bool azdeckIsPing(const char* data, size_t length) {
    const size_t prefixLength = sizeof(kPingPrefix) - 1;
    if (data == nullptr || length < prefixLength) {
        return false;
    }
    return memcmp(data, kPingPrefix, prefixLength) == 0;
}

bool azdeckBuildPong(
    const char* pingData,
    size_t pingLength,
    char* out,
    size_t outCapacity,
    size_t* outLength
) {
    const size_t pingPrefixLength = sizeof(kPingPrefix) - 1;
    const size_t pongPrefixLength = sizeof(kPongPrefix) - 1;
    if (!azdeckIsPing(pingData, pingLength) || out == nullptr || outCapacity == 0) {
        return false;
    }

    const size_t tokenLength = pingLength - pingPrefixLength;
    const size_t needed = pongPrefixLength + tokenLength;
    if (needed >= outCapacity) {
        return false;
    }

    memcpy(out, kPongPrefix, pongPrefixLength);
    if (tokenLength > 0) {
        memcpy(out + pongPrefixLength, pingData + pingPrefixLength, tokenLength);
    }
    out[needed] = '\0';
    if (outLength != nullptr) {
        *outLength = needed;
    }
    return true;
}
