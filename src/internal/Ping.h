#pragma once

#include <stddef.h>

bool azdeckIsPing(const char* data, size_t length);
bool azdeckBuildPong(
    const char* pingData,
    size_t pingLength,
    char* out,
    size_t outCapacity,
    size_t* outLength
);
