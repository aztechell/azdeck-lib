#pragma once

#include <stddef.h>
#include <stdint.h>

bool azdeckSppBegin(const char* deviceName);
void azdeckSppUpdate();
bool azdeckSppTake(
    char* buffer,
    size_t capacity,
    size_t* outLength,
    bool* overflow
);
bool azdeckSppTakeDisconnect();
void azdeckSppSend(const char* data, size_t length);
