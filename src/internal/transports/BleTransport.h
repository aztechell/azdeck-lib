#pragma once

#include <stddef.h>
#include <stdint.h>

bool azdeckBleBegin(const char* deviceName);
void azdeckBleUpdate();
bool azdeckBleTake(
    char* buffer,
    size_t capacity,
    size_t* outLength,
    bool* overflow
);
bool azdeckBleTakeDisconnect();
bool azdeckBleConnected();
void azdeckBleSend(const char* data, size_t length);
