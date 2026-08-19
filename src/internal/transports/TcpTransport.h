#pragma once

#include <stddef.h>
#include <stdint.h>

bool azdeckTcpBegin(uint16_t port, bool jsonMode);
void azdeckTcpUpdate(
    void (*onPayload)(void* context, const char* data, size_t length),
    void (*onFail)(void* context),
    void (*onDisconnect)(void* context),
    void* context
);
void azdeckTcpSend(const char* data, size_t length);
