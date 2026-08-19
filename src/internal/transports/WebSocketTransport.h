#pragma once

#include <stddef.h>
#include <stdint.h>

bool azdeckWsBegin(uint16_t port);
void azdeckWsUpdate();
bool azdeckWsTake(
    char* buffer,
    size_t capacity,
    size_t* outLength,
    uint8_t* clientId,
    bool* overflow
);
bool azdeckWsTakeDisconnect();
void azdeckWsSend(uint8_t clientId, const char* data, size_t length);
