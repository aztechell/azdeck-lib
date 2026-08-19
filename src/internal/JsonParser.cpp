#include "JsonParser.h"

#include "ChannelStore.h"
#include "Config.h"

#include <Arduino.h>
#include <ArduinoJson.h>

AzDeckJsonParseResult azdeckParseJson(
    const char* data,
    size_t length,
    AzDeckChannelStore& store
) {
    if (data == nullptr || length == 0) {
        return AZDECK_JSON_MALFORMED;
    }

    StaticJsonDocument<AZDECK_JSON_DOC_SIZE> document;
    const DeserializationError error = deserializeJson(document, data, length);
    if (error != DeserializationError::Ok || !document.is<JsonObject>()) {
        return AZDECK_JSON_MALFORMED;
    }

    JsonObject object = document.as<JsonObject>();
    if (object.containsKey("type")) {
        return AZDECK_JSON_SERVICE;
    }

    for (JsonPair kv : object) {
        const char* key = kv.key().c_str();
        if (key == nullptr || key[0] == '\0') {
            continue;
        }

        const JsonVariant value = kv.value();
        if (value.is<JsonObject>() || value.is<JsonArray>() || value.isNull()) {
            continue;
        }
        if (value.is<bool>() || value.is<const char*>() || value.is<char*>()) {
            continue;
        }
        if (!(value.is<float>() || value.is<double>() || value.is<int>() ||
              value.is<long>() || value.is<unsigned int>() || value.is<unsigned long>())) {
            continue;
        }

        store.set(key, value.as<float>());
    }

    return AZDECK_JSON_CONTROL;
}
