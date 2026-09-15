#include "AzDeckTypes.h"
#include "internal/ChannelStore.h"
#include "internal/Config.h"
#include "internal/ControlCore.h"
#include "internal/JsonParser.h"
#include "internal/PacketQueue.h"
#include "internal/Ping.h"
#include "internal/platform/SettingsHttpProtocol.h"

#include <ArduinoJson.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>

static int gFailures = 0;

static void expect(bool ok, const char* name) {
    if (!ok) {
        std::printf("FAIL %s\n", name);
        ++gFailures;
    } else {
        std::printf("ok   %s\n", name);
    }
}

static AzDeckJsonParseResult parseJsonCopy(
    AzDeckChannelStore& store,
    const char* json
) {
    char buffer[AZDECK_RX_BUFFER_SIZE];
    const size_t n = std::strlen(json);
    if (n >= sizeof(buffer)) {
        return AZDECK_JSON_MALFORMED;
    }
    std::memcpy(buffer, json, n);
    buffer[n] = '\0';
    return azdeckParseJson(buffer, n, store);
}

static void testChannelStore() {
    AzDeckChannelStore store;
    expect(store.get("missing") == 0.0f, "unknown channel is 0");

    expect(store.set("move_x", 0.72f), "set move_x");
    expect(store.get("move_x") == 0.72f, "get move_x");
    expect(store.set("speed", 180.0f), "set speed");
    expect(store.get("speed") == 180.0f, "slider preserves value");

    store.zeroValues();
    expect(store.get("move_x") == 0.0f, "failsafe zeros values");
    expect(store.get("speed") == 0.0f, "failsafe zeros speed");

    char longName[AZDECK_MAX_CHANNEL_NAME_LENGTH + 2];
    for (int i = 0; i < AZDECK_MAX_CHANNEL_NAME_LENGTH + 1; ++i) {
        longName[i] = 'a';
    }
    longName[AZDECK_MAX_CHANNEL_NAME_LENGTH + 1] = '\0';
    expect(!store.set(longName, 1.0f), "oversize name rejected");
    expect(store.get(longName) == 0.0f, "oversize name not stored");

    char maxName[AZDECK_MAX_CHANNEL_NAME_LENGTH + 1];
    for (int i = 0; i < AZDECK_MAX_CHANNEL_NAME_LENGTH; ++i) {
        maxName[i] = 'b';
    }
    maxName[AZDECK_MAX_CHANNEL_NAME_LENGTH] = '\0';
    expect(store.set(maxName, 3.0f), "max-length name accepted");
    expect(store.get(maxName) == 3.0f, "max-length name stored");
}

static void testJsonParser() {
    AzDeckChannelStore store;
    expect(parseJsonCopy(store, "{\"move_x\":0.5}") == AZDECK_JSON_CONTROL, "json positive");
    expect(store.get("move_x") == 0.5f, "json positive value");

    expect(parseJsonCopy(store, "{\"move_y\":-0.25}") == AZDECK_JSON_CONTROL, "json negative");
    expect(store.get("move_y") == -0.25f, "json negative value");

    expect(parseJsonCopy(store, "{\"speed\":180}") == AZDECK_JSON_CONTROL, "json integer");
    expect(store.get("speed") == 180.0f, "json integer value");

    expect(parseJsonCopy(store, "{\"fire\":0}") == AZDECK_JSON_CONTROL, "json zero");
    expect(store.get("fire") == 0.0f, "json zero value");

    AzDeckChannelStore multi;
    expect(
        parseJsonCopy(multi, "{\"move_x\":0.5,\"move_y\":-0.2,\"l2\":1}") == AZDECK_JSON_CONTROL,
        "json multiple channels"
    );
    expect(multi.get("move_x") == 0.5f, "json multi x");
    expect(multi.get("move_y") == -0.2f, "json multi y");
    expect(multi.get("l2") == 1.0f, "json multi l2");

    AzDeckChannelStore stale;
    stale.set("move_x", 1.0f);
    expect(parseJsonCopy(stale, "{\"move_x\":\"oops\"}") == AZDECK_JSON_MALFORMED, "json string malformed");
    expect(stale.get("move_x") == 1.0f, "json string does not mutate");

    expect(parseJsonCopy(stale, "{\"move_x\":true}") == AZDECK_JSON_MALFORMED, "json bool malformed");
    expect(stale.get("move_x") == 1.0f, "json bool does not mutate");

    expect(parseJsonCopy(stale, "{\"move_x\":null}") == AZDECK_JSON_MALFORMED, "json null malformed");
    expect(parseJsonCopy(stale, "{\"move_x\":[1]}") == AZDECK_JSON_MALFORMED, "json array malformed");
    expect(parseJsonCopy(stale, "{\"move_x\":{\"a\":1}}") == AZDECK_JSON_MALFORMED, "json nested malformed");
    expect(stale.get("move_x") == 1.0f, "json nested does not mutate");

    AzDeckChannelStore atomic;
    atomic.set("x", 9.0f);
    atomic.set("y", 8.0f);
    expect(parseJsonCopy(atomic, "{\"x\":1,\"y\":\"oops\"}") == AZDECK_JSON_MALFORMED, "json later key malformed");
    expect(atomic.get("x") == 9.0f, "json no partial x");
    expect(atomic.get("y") == 8.0f, "json no partial y");

    AzDeckChannelStore service;
    service.set("x", 4.0f);
    expect(
        parseJsonCopy(service, "{\"type\":\"telemetry\",\"x\":1}") == AZDECK_JSON_SERVICE,
        "json service"
    );
    expect(service.get("x") == 4.0f, "json service no mutation");

    AzDeckChannelStore empty;
    empty.set("x", 5.0f);
    expect(parseJsonCopy(empty, "{}") == AZDECK_JSON_EMPTY, "json empty object");
    expect(empty.get("x") == 5.0f, "json empty does not mutate");

    AzDeckChannelStore skip;
    std::string longKey(AZDECK_MAX_CHANNEL_NAME_LENGTH + 1, 'k');
    std::string packet = "{\"";
    packet += longKey;
    packet += "\":1,\"short\":2}";
    expect(parseJsonCopy(skip, packet.c_str()) == AZDECK_JSON_CONTROL, "json oversized skipped");
    expect(skip.get("short") == 2.0f, "json short key kept");

    AzDeckChannelStore many;
    std::string packed = "{";
    for (int i = 0; i < AZDECK_MAX_CHANNELS; ++i) {
        char item[32];
        std::snprintf(item, sizeof(item), "%s\"c%02d\":%d", i == 0 ? "" : ",", i, i);
        packed += item;
    }
    packed += "}";
    expect(parseJsonCopy(many, packed.c_str()) == AZDECK_JSON_CONTROL, "json 32 short channels");
    expect(many.get("c00") == 0.0f, "json c00");
    expect(many.get("c31") == 31.0f, "json c31");
}

static void testPing() {
    const char* ping = "AZDECK_PING:1723456789012";
    expect(azdeckIsPing(ping, std::strlen(ping)), "detect ping");
    expect(!azdeckIsPing("{\"a\":1}", 7), "json is not ping");

    char pong[64];
    size_t pongLength = 0;
    expect(
        azdeckBuildPong(ping, std::strlen(ping), pong, sizeof(pong), &pongLength),
        "build pong"
    );
    expect(std::strcmp(pong, "AZDECK_PONG:1723456789012") == 0, "pong token");

    const char* shortPing = "AZDECK_PING:a";
    expect(azdeckIsPing(shortPing, std::strlen(shortPing)), "detect short ping");
    expect(
        azdeckBuildPong(shortPing, std::strlen(shortPing), pong, sizeof(pong), &pongLength),
        "build short pong"
    );
    expect(std::strcmp(pong, "AZDECK_PONG:a") == 0, "short pong token");
}

static void handleCopy(AzDeckControlCore& core, const char* payload, uint32_t nowMs) {
    char buffer[AZDECK_RX_BUFFER_SIZE];
    const size_t n = std::strlen(payload);
    std::memcpy(buffer, payload, n);
    buffer[n] = '\0';
    core.handlePayload(buffer, n, nowMs);
}

static bool takePacket(
    AzDeckPacketQueue& queue,
    char* buffer,
    size_t capacity,
    bool* overflow
) {
    size_t n = 0;
    return queue.take(buffer, capacity, &n, nullptr, overflow);
}

static void testPacketQueue() {
    AzDeckPacketQueue queue;
    const uint8_t a[] = {'A'};
    const uint8_t b[] = {'B'};
    queue.push(a, 1);
    queue.push(b, 1);

    uint8_t oversized[AZDECK_RX_BUFFER_SIZE];
    std::memset(oversized, 'x', sizeof(oversized));
    queue.push(oversized, sizeof(oversized));
    expect(queue.count() == 0, "overflow clears queue");
    expect(queue.overflowPending(), "overflow pending");

    char buf[8];
    bool overflow = false;
    expect(!takePacket(queue, buf, sizeof(buf), &overflow), "overflow take empty");
    expect(overflow, "overflow flagged");
    overflow = false;
    expect(!takePacket(queue, buf, sizeof(buf), &overflow), "no stale A/B after overflow");
    expect(!overflow, "overflow consumed");
    expect(queue.count() == 0, "queue stays empty");

    AzDeckPacketQueue full;
    for (uint8_t i = 0; i < AZDECK_PACKET_QUEUE_DEPTH; ++i) {
        uint8_t item[] = {static_cast<uint8_t>('0' + i)};
        full.push(item, 1);
    }
    const uint8_t newest[] = {'N'};
    full.push(newest, 1);
    expect(full.count() == AZDECK_PACKET_QUEUE_DEPTH, "full queue keeps depth");
    expect(!full.overflowPending(), "queue full is not overflow");

    overflow = true;
    expect(takePacket(full, buf, sizeof(buf), &overflow), "take after drop oldest");
    expect(!overflow, "queue full does not failsafe");
    expect(buf[0] == '1', "oldest snapshot dropped");
    expect(takePacket(full, buf, sizeof(buf), &overflow), "take 2");
    expect(buf[0] == '2', "retained 2");
    expect(takePacket(full, buf, sizeof(buf), &overflow), "take 3");
    expect(buf[0] == '3', "retained 3");
    expect(takePacket(full, buf, sizeof(buf), &overflow), "take newest");
    expect(buf[0] == 'N', "newest retained");
    expect(!takePacket(full, buf, sizeof(buf), &overflow), "queue drained");
}

static void testControlCore() {
    AzDeckControlCore jsonCore;
    jsonCore.configure(BLE, 350);
    handleCopy(jsonCore, "{\"x\":1}", 0);
    expect(jsonCore.value("x") == 1.0f, "json control x");
    handleCopy(jsonCore, "{\"x\":\"oops\"}", 10);
    expect(jsonCore.value("x") == 0.0f, "malformed json zeros x");
    expect(jsonCore.hasCommand(), "malformed json keeps timer");
    jsonCore.checkTimeout(351);
    expect(!jsonCore.hasCommand(), "malformed json does not refresh timeout");

    AzDeckControlCore service;
    service.configure(BLE, 350);
    handleCopy(service, "{\"x\":1}", 0);
    handleCopy(service, "{\"type\":\"telemetry\",\"battery\":80}", 10);
    expect(service.value("x") == 1.0f, "service json keeps x");
    handleCopy(service, "{\"type\":\"settings_get\"}", 20);
    expect(service.value("x") == 1.0f, "settings_get keeps x");
    service.checkTimeout(351);
    expect(service.value("x") == 0.0f, "service json does not refresh timeout");

    AzDeckControlCore empty;
    empty.configure(BLE, 350);
    handleCopy(empty, "{\"x\":1}", 0);
    handleCopy(empty, "{}", 10);
    expect(empty.value("x") == 1.0f, "empty json keeps x");
    empty.checkTimeout(351);
    expect(empty.value("x") == 0.0f, "empty json does not refresh timeout");

    AzDeckControlCore ws;
    ws.configure(WEBSOCKET, 350);
    handleCopy(ws, "{\"x\":1}", 0);
    handleCopy(ws, "AZDECK_PING:1723456789012", 10);
    char pong[64];
    size_t pongLength = 0;
    expect(ws.takePong(pong, sizeof(pong), &pongLength), "ws ping builds pong");
    expect(std::strcmp(pong, "AZDECK_PONG:1723456789012") == 0, "ws pong token");
    expect(ws.value("x") == 1.0f, "ws ping keeps controls");
    ws.checkTimeout(351);
    expect(ws.value("x") == 0.0f, "ws ping does not refresh timeout");

    const AzDeckTransport others[] = {BLE, SPP};
    const char* names[] = {"ble", "spp"};
    for (int i = 0; i < 2; ++i) {
        AzDeckControlCore core;
        core.configure(others[i], 350);
        handleCopy(core, "{\"x\":1}", 0);
        handleCopy(core, "AZDECK_PING:1723456789012", 10);
        pongLength = 0;
        char built[40];
        char token[40];
        char keeps[40];
        char noRefresh[48];
        std::snprintf(built, sizeof(built), "%s ping builds pong", names[i]);
        std::snprintf(token, sizeof(token), "%s pong token", names[i]);
        std::snprintf(keeps, sizeof(keeps), "%s ping keeps controls", names[i]);
        std::snprintf(noRefresh, sizeof(noRefresh), "%s ping does not refresh timeout", names[i]);
        expect(core.takePong(pong, sizeof(pong), &pongLength), built);
        expect(std::strcmp(pong, "AZDECK_PONG:1723456789012") == 0, token);
        expect(core.value("x") == 1.0f, keeps);
        core.checkTimeout(351);
        expect(core.value("x") == 0.0f, noRefresh);
    }

    AzDeckControlCore timeoutZero;
    timeoutZero.configure(BLE, 0);
    expect(timeoutZero.timeoutMs() == AZDECK_DEFAULT_TIMEOUT_MS, "timeout 0 uses 350");
    handleCopy(timeoutZero, "{\"x\":1}", 0);
    timeoutZero.checkTimeout(351);
    expect(timeoutZero.value("x") == 0.0f, "timeout 0 still failsafes");
}

static bool parseTelemetry(
    const char* json,
    size_t length,
    StaticJsonDocument<AZDECK_JSON_DOC_SIZE>& document
) {
    if (json == nullptr || length == 0) {
        return false;
    }
    const DeserializationError error = deserializeJson(document, json, length);
    return error == DeserializationError::Ok && document.is<JsonObject>();
}

static void testTelemetry() {
    AzDeckControlCore empty;
    empty.configure(BLE, 350);
    char buffer[AZDECK_RX_BUFFER_SIZE];
    size_t length = 0;
    expect(!empty.takeTelemetry(buffer, sizeof(buffer), &length), "empty queue");

    AzDeckControlCore one;
    one.configure(BLE, 350);
    expect(one.queueTelemetry("battery", 7.4f), "queue battery");
    expect(one.takeTelemetry(buffer, sizeof(buffer), &length), "take one key");
    StaticJsonDocument<AZDECK_JSON_DOC_SIZE> doc;
    expect(parseTelemetry(buffer, length, doc), "one key json");
    expect(std::strcmp(doc["type"] | "", "telemetry") == 0, "one key type");
    expect(std::fabs(doc["battery"].as<float>() - 7.4f) < 0.001f, "one key value");
    expect(!doc.containsKey("voltage"), "one key only battery");
    expect(!one.takeTelemetry(buffer, sizeof(buffer), &length), "one key drained");

    AzDeckControlCore two;
    two.configure(BLE, 350);
    expect(two.queueTelemetry("battery", 7.4f), "queue two battery");
    expect(two.queueTelemetry("voltage", 12.5f), "queue two voltage");
    expect(two.takeTelemetry(buffer, sizeof(buffer), &length), "take two keys");
    doc.clear();
    expect(parseTelemetry(buffer, length, doc), "two key json");
    expect(std::strcmp(doc["type"] | "", "telemetry") == 0, "two key type");
    expect(std::fabs(doc["battery"].as<float>() - 7.4f) < 0.001f, "two key battery");
    expect(std::fabs(doc["voltage"].as<float>() - 12.5f) < 0.001f, "two key voltage");

    AzDeckControlCore failsafeCore;
    failsafeCore.configure(BLE, 350);
    handleCopy(failsafeCore, "{\"x\":1}", 0);
    expect(failsafeCore.queueTelemetry("battery", 8.1f), "queue before failsafe");
    failsafeCore.failsafe();
    expect(failsafeCore.value("x") == 0.0f, "failsafe zeros controls");
    expect(failsafeCore.takeTelemetry(buffer, sizeof(buffer), &length), "failsafe keeps outbound");
    doc.clear();
    expect(parseTelemetry(buffer, length, doc), "failsafe telemetry json");
    expect(std::fabs(doc["battery"].as<float>() - 8.1f) < 0.001f, "failsafe telemetry value");

    AzDeckControlCore inbound;
    inbound.configure(BLE, 350);
    handleCopy(inbound, "{\"type\":\"telemetry\",\"x\":1}", 0);
    expect(inbound.value("x") == 0.0f, "inbound telemetry does not set x");
    expect(!inbound.hasCommand(), "inbound telemetry does not refresh timeout");

    AzDeckControlCore rejected;
    rejected.configure(BLE, 350);
    char longName[AZDECK_MAX_CHANNEL_NAME_LENGTH + 2];
    for (int i = 0; i < AZDECK_MAX_CHANNEL_NAME_LENGTH + 1; ++i) {
        longName[i] = 'a';
    }
    longName[AZDECK_MAX_CHANNEL_NAME_LENGTH + 1] = '\0';
    expect(!rejected.queueTelemetry(longName, 1.0f), "oversize name ignored");
    expect(
        !rejected.queueTelemetry("nan", std::numeric_limits<float>::quiet_NaN()),
        "nan ignored"
    );
    expect(
        !rejected.queueTelemetry("inf", std::numeric_limits<float>::infinity()),
        "inf ignored"
    );
    expect(!rejected.takeTelemetry(buffer, sizeof(buffer), &length), "rejected stays empty");

    AzDeckControlCore full;
    full.configure(BLE, 350);
    for (int i = 0; i < AZDECK_MAX_TELEMETRY_CHANNELS; ++i) {
        char name[8];
        std::snprintf(name, sizeof(name), "k%02d", i);
        expect(full.queueTelemetry(name, static_cast<float>(i)), name);
    }
    expect(!full.queueTelemetry("extra", 1.0f), "extra telemetry key ignored");

    AzDeckControlCore overflow;
    overflow.configure(BLE, 350);
    expect(overflow.queueTelemetry("battery", 7.4f), "overflow measure queue");
    char oneKey[AZDECK_RX_BUFFER_SIZE];
    size_t oneLen = 0;
    expect(overflow.takeTelemetry(oneKey, sizeof(oneKey), &oneLen), "overflow measure take");
    expect(overflow.queueTelemetry("battery", 7.4f), "overflow requeue battery");
    expect(overflow.queueTelemetry("voltage", 12.5f), "overflow queue voltage");
    char part[AZDECK_RX_BUFFER_SIZE];
    size_t partLen = 0;
    expect(overflow.takeTelemetry(part, oneLen + 1, &partLen), "oversized first flush");
    doc.clear();
    expect(parseTelemetry(part, partLen, doc), "partial json");
    expect(doc.containsKey("battery"), "partial has first key");
    expect(!doc.containsKey("voltage"), "partial leaves remainder");
    char rest[AZDECK_RX_BUFFER_SIZE];
    size_t restLen = 0;
    expect(overflow.takeTelemetry(rest, sizeof(rest), &restLen), "oversized remainder");
    doc.clear();
    expect(parseTelemetry(rest, restLen, doc), "remainder json");
    expect(doc.containsKey("voltage"), "remainder has second key");
    expect(!doc.containsKey("battery"), "remainder does not resend first");

    AzDeckControlCore text;
    text.configure(WEBSOCKET, 350);
    expect(text.queueTelemetry("serial", "hello"), "queue text");
    expect(text.takeTelemetry(buffer, sizeof(buffer), &length), "take text");
    doc.clear();
    expect(parseTelemetry(buffer, length, doc), "text json");
    expect(std::strcmp(doc["type"] | "", "telemetry") == 0, "text type");
    expect(std::strcmp(doc["serial"] | "", "hello") == 0, "text value");

    char tooLong[AZDECK_MAX_TELEMETRY_TEXT_LENGTH + 2];
    for (int i = 0; i < AZDECK_MAX_TELEMETRY_TEXT_LENGTH + 1; ++i) {
        tooLong[i] = 'x';
    }
    tooLong[AZDECK_MAX_TELEMETRY_TEXT_LENGTH + 1] = '\0';
    expect(!text.queueTelemetry("serial", tooLong), "oversize text ignored");
    expect(!text.queueTelemetry("serial", static_cast<const char*>(nullptr)), "null text ignored");
}

static void testSettingsHttp() {
    const char* incomplete = "HEAD /settings HTTP/1.1\r\nHost: 192.168.4.1\r\n";
    expect(
        azdeckSettingsHttpClassify(incomplete, std::strlen(incomplete)) ==
            AZDECK_HTTP_INCOMPLETE,
        "incomplete without header end"
    );

    const char* head = "HEAD /settings HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n";
    expect(
        azdeckSettingsHttpClassify(head, std::strlen(head)) == AZDECK_HTTP_SETTINGS_HEAD,
        "head settings"
    );

    const char* get = "GET /settings HTTP/1.1\r\n\r\n";
    expect(
        azdeckSettingsHttpClassify(get, std::strlen(get)) == AZDECK_HTTP_SETTINGS_GET,
        "get settings"
    );

    const char* query = "GET /settings?x=1 HTTP/1.1\r\n\r\n";
    expect(
        azdeckSettingsHttpClassify(query, std::strlen(query)) == AZDECK_HTTP_SETTINGS_GET,
        "get settings query"
    );

    const char* root = "GET / HTTP/1.1\r\n\r\n";
    expect(
        azdeckSettingsHttpClassify(root, std::strlen(root)) == AZDECK_HTTP_OTHER,
        "get root is other"
    );

    const char* post = "POST /settings HTTP/1.1\r\n\r\n";
    expect(
        azdeckSettingsHttpClassify(post, std::strlen(post)) == AZDECK_HTTP_OTHER,
        "post settings is other"
    );

    char ok[256];
    const size_t okLen = azdeckSettingsHttpFormatOk(ok, sizeof(ok), 12);
    expect(okLen > 0, "ok headers length");
    expect(std::strstr(ok, "X-AzDeck-Settings: 1") != nullptr, "ok marker");
    expect(std::strstr(ok, "Content-Type: text/html; charset=utf-8") != nullptr, "ok type");
    expect(std::strstr(ok, "Cache-Control: no-store") != nullptr, "ok cache");
    expect(std::strstr(ok, "Content-Length: 12") != nullptr, "ok content length");
    expect(std::strstr(ok, "\r\n\r\n") != nullptr, "ok header end");

    char missing[256];
    const size_t missingLen = azdeckSettingsHttpFormatNotFound(missing, sizeof(missing));
    expect(missingLen > 0, "404 headers length");
    expect(std::strstr(missing, "X-AzDeck-Settings") == nullptr, "404 has no marker");
    expect(std::strstr(missing, "404") != nullptr, "404 status");

    char q[32];
    expect(azdeckSettingsHttpQuery(get, std::strlen(get), q, sizeof(q)) == 0, "get no query");
    expect(
        azdeckSettingsHttpQuery(query, std::strlen(query), q, sizeof(q)) > 0,
        "query extracted"
    );
    expect(std::strcmp(q, "x=1") == 0, "query value");
    const char* led = "GET /settings?led=1 HTTP/1.1\r\n\r\n";
    expect(
        azdeckSettingsHttpQuery(led, std::strlen(led), q, sizeof(q)) > 0,
        "led query extracted"
    );
    expect(std::strcmp(q, "led=1") == 0, "led query value");
}

int main() {
    testChannelStore();
    testJsonParser();
    testPing();
    testPacketQueue();
    testControlCore();
    testTelemetry();
    testSettingsHttp();

    if (gFailures > 0) {
        std::printf("%d test(s) failed\n", gFailures);
        return 1;
    }
    std::printf("all tests passed\n");
    return 0;
}
