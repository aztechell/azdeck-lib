#pragma once

#include <stddef.h>
#include <stdint.h>

bool azdeckUnoR4BleStart(const char* deviceName);
void azdeckUnoR4BlePoll();
bool azdeckUnoR4BleTake(
    char* buffer,
    size_t capacity,
    size_t* outLength,
    bool* overflow
);
bool azdeckUnoR4BleTakeDisconnect();
bool azdeckUnoR4BleConnected();
void azdeckUnoR4BleSend(const char* data, size_t length);
