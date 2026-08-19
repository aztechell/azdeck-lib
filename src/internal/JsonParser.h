#pragma once

#include <stddef.h>

class AzDeckChannelStore;

enum AzDeckJsonParseResult {
    AZDECK_JSON_CONTROL,
    AZDECK_JSON_SERVICE,
    AZDECK_JSON_EMPTY,
    AZDECK_JSON_MALFORMED
};

AzDeckJsonParseResult azdeckParseJson(
    char* data,
    size_t length,
    AzDeckChannelStore& store
);
