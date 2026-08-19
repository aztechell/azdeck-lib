#pragma once

#include <stddef.h>
#include <stdint.h>

bool azdeckEsp32BleStart(const char* deviceName);
void azdeckEsp32BlePoll();
bool azdeckEsp32BleTake(
    char* buffer,
    size_t capacity,
    size_t* outLength,
    bool* overflow
);
bool azdeckEsp32BleTakeDisconnect();
bool azdeckEsp32BleConnected();
void azdeckEsp32BleSend(const char* data, size_t length);
