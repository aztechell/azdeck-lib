#include "TextParser.h"

#include "ChannelStore.h"
#include "Config.h"

#include <stdlib.h>
#include <string.h>

namespace {

bool isSeparator(char c) {
    return c == ' ' || c == ';' || c == ',' || c == '\t' || c == '\r' || c == '\n';
}

bool parseNumber(const char* text, size_t length, float& out) {
    if (text == nullptr || length == 0 || length >= 32) {
        return false;
    }

    char buffer[32];
    memcpy(buffer, text, length);
    buffer[length] = '\0';

    char* end = nullptr;
    const float value = strtof(buffer, &end);
    if (end == buffer) {
        return false;
    }
    out = value;
    return true;
}

}  // namespace

AzDeckTextParseResult azdeckParseText(
    const char* data,
    size_t length,
    AzDeckChannelStore& store
) {
    if (data == nullptr || length == 0) {
        return AZDECK_TEXT_MALFORMED;
    }

    int applied = 0;
    size_t i = 0;
    while (i < length) {
        while (i < length && isSeparator(data[i])) {
            ++i;
        }
        if (i >= length) {
            break;
        }

        const size_t tokenStart = i;
        while (i < length && !isSeparator(data[i])) {
            ++i;
        }

        const size_t tokenLength = i - tokenStart;
        if (tokenLength == 0) {
            continue;
        }

        size_t sep = static_cast<size_t>(-1);
        for (size_t j = 0; j < tokenLength; ++j) {
            const char c = data[tokenStart + j];
            if (c == ':' || c == '=') {
                sep = j;
                break;
            }
        }

        if (sep == static_cast<size_t>(-1) || sep == 0 || sep + 1 >= tokenLength) {
            continue;
        }

        const char* keyPtr = data + tokenStart;
        const size_t keyLen = sep;
        const char* valuePtr = data + tokenStart + sep + 1;
        const size_t valueLen = tokenLength - sep - 1;

        if (keyLen > AZDECK_MAX_CHANNEL_NAME_LENGTH) {
            continue;
        }

        char key[AZDECK_MAX_CHANNEL_NAME_LENGTH + 1];
        memcpy(key, keyPtr, keyLen);
        key[keyLen] = '\0';

        float value = 0.0f;
        if (!parseNumber(valuePtr, valueLen, value)) {
            continue;
        }

        store.set(key, value);
        ++applied;
    }

    return applied > 0 ? AZDECK_TEXT_CONTROL : AZDECK_TEXT_MALFORMED;
}
