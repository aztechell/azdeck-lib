#pragma once

#include <stddef.h>

enum AzDeckSettingsHttpKind {
    AZDECK_HTTP_INCOMPLETE,
    AZDECK_HTTP_SETTINGS_HEAD,
    AZDECK_HTTP_SETTINGS_GET,
    AZDECK_HTTP_OTHER
};

AzDeckSettingsHttpKind azdeckSettingsHttpClassify(
    const char* request,
    size_t length
);

size_t azdeckSettingsHttpFormatOk(
    char* out,
    size_t capacity,
    size_t bodyLength
);

size_t azdeckSettingsHttpFormatNotFound(char* out, size_t capacity);

size_t azdeckSettingsHttpQuery(
    const char* request,
    size_t length,
    char* out,
    size_t capacity
);
