#include "internal/ChannelStore.h"
#include "internal/Config.h"
#include "internal/JsonStreamFramer.h"
#include "internal/Ping.h"
#include "internal/TextParser.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int gFailures = 0;

static void expect(bool ok, const char* name) {
    if (!ok) {
        std::printf("FAIL %s\n", name);
        ++gFailures;
    } else {
        std::printf("ok   %s\n", name);
    }
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

static void testTextParser() {
    AzDeckChannelStore store;
    const char* text = "move_x:0.5 move_y:-0.3 l2:1";
    expect(
        azdeckParseText(text, std::strlen(text), store) == AZDECK_TEXT_CONTROL,
        "text control"
    );
    expect(store.get("move_x") == 0.5f, "text positive");
    expect(store.get("move_y") == -0.3f, "text negative");
    expect(store.get("l2") != 0.0f, "text button true");

    AzDeckChannelStore store2;
    const char* alt = "speed=180;fire=0,up=1";
    expect(
        azdeckParseText(alt, std::strlen(alt), store2) == AZDECK_TEXT_CONTROL,
        "text separators"
    );
    expect(store2.get("speed") == 180.0f, "text integer");
    expect(store2.get("fire") == 0.0f, "button zero");
    expect(store2.get("up") == 1.0f, "dpad one");

    AzDeckChannelStore store3;
    const char* garbage = "hello world";
    expect(
        azdeckParseText(garbage, std::strlen(garbage), store3) == AZDECK_TEXT_MALFORMED,
        "text malformed"
    );

    AzDeckChannelStore store4;
    std::string longKey(AZDECK_MAX_CHANNEL_NAME_LENGTH + 1, 'k');
    longKey += ":1 short:2";
    expect(
        azdeckParseText(longKey.c_str(), longKey.size(), store4) == AZDECK_TEXT_CONTROL,
        "text skips oversize key"
    );
    expect(store4.get("short") == 2.0f, "text keeps valid key");
}

struct FramerCapture {
    std::vector<std::string> objects;
    int fails;
};

static void onObject(void* context, const char* data, size_t length) {
    FramerCapture* capture = static_cast<FramerCapture*>(context);
    capture->objects.push_back(std::string(data, length));
}

static void onFail(void* context) {
    FramerCapture* capture = static_cast<FramerCapture*>(context);
    ++capture->fails;
}

static void testJsonFramer() {
    AzDeckJsonStreamFramer framer;
    FramerCapture capture;
    capture.fails = 0;

    const char* first = "{\"a\":1}";
    framer.feed(
        reinterpret_cast<const uint8_t*>(first),
        std::strlen(first),
        onObject,
        onFail,
        &capture
    );
    expect(capture.objects.size() == 1, "one json object");
    expect(capture.fails == 0, "no fail on complete object");

    capture.objects.clear();
    const char* split = "{\"b\":";
    framer.feed(
        reinterpret_cast<const uint8_t*>(split),
        std::strlen(split),
        onObject,
        onFail,
        &capture
    );
    expect(capture.objects.empty(), "incomplete json held");
    const char* rest = "2}";
    framer.feed(
        reinterpret_cast<const uint8_t*>(rest),
        std::strlen(rest),
        onObject,
        onFail,
        &capture
    );
    expect(capture.objects.size() == 1, "json completed across feeds");

    capture.objects.clear();
    const char* multi = "{\"x\":1}{\"y\":2}";
    framer.feed(
        reinterpret_cast<const uint8_t*>(multi),
        std::strlen(multi),
        onObject,
        onFail,
        &capture
    );
    expect(capture.objects.size() == 2, "multiple json objects");

    capture.objects.clear();
    capture.fails = 0;
    const char* quoted = "{\"k\":\"}\"}";
    framer.feed(
        reinterpret_cast<const uint8_t*>(quoted),
        std::strlen(quoted),
        onObject,
        onFail,
        &capture
    );
    expect(capture.objects.size() == 1, "braces inside strings ignored");
    expect(capture.fails == 0, "quoted brace not fail");

    capture.objects.clear();
    capture.fails = 0;
    const char* escaped = "{\"k\":\"\\\"}\"}";
    framer.feed(
        reinterpret_cast<const uint8_t*>(escaped),
        std::strlen(escaped),
        onObject,
        onFail,
        &capture
    );
    expect(capture.objects.size() == 1, "escaped quotes handled");

    capture.objects.clear();
    capture.fails = 0;
    std::string huge(AZDECK_RX_BUFFER_SIZE + 8, 'x');
    huge[0] = '{';
    framer.reset();
    framer.feed(
        reinterpret_cast<const uint8_t*>(huge.data()),
        huge.size(),
        onObject,
        onFail,
        &capture
    );
    expect(capture.objects.empty(), "overflow emits nothing");
    expect(capture.fails > 0, "overflow failsafe");
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
}

int main() {
    testChannelStore();
    testTextParser();
    testJsonFramer();
    testPing();

    if (gFailures > 0) {
        std::printf("%d test(s) failed\n", gFailures);
        return 1;
    }
    std::printf("all tests passed\n");
    return 0;
}
