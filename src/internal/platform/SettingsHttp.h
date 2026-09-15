#pragma once

bool azdeckSettingsHttpBegin(const char* html, void (*onQuery)(const char* query));
void azdeckSettingsHttpUpdate();
