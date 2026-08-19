#pragma once

#include <stddef.h>

class AzDeckChannelStore;

enum AzDeckTextParseResult {
    AZDECK_TEXT_CONTROL,
    AZDECK_TEXT_MALFORMED
};

AzDeckTextParseResult azdeckParseText(
    const char* data,
    size_t length,
    AzDeckChannelStore& store
);
