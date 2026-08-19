#include "internal/ChannelStore.h"
#include "internal/Config.h"
#include "internal/JsonParser.h"
#include "internal/JsonStreamFramer.h"
#include "internal/Ping.h"
#include "internal/TextParser.h"
#include "internal/TextStreamFramer.h"

#include <cmath>
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

    AzDeckChannelStore one;
    const char* justOne = "x:1";
    expect(azdeckParseText(justOne, std::strlen(justOne), one) == AZDECK_TEXT_CONTROL, "text x:1");
    expect(one.get("x") == 1.0f, "text x:1 value");

    AzDeckChannelStore neg;
    const char* justNeg = "x:-1";
    expect(azdeckParseText(justNeg, std::strlen(justNeg), neg) == AZDECK_TEXT_CONTROL, "text x:-1");
    expect(neg.get("x") == -1.0f, "text x:-1 value");

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
        "text malformed tokens"
    );

    AzDeckChannelStore badNum;
    badNum.set("x", 9.0f);
    const char* abc = "x:1abc";
    expect(azdeckParseText(abc, std::strlen(abc), badNum) == AZDECK_TEXT_MALFORMED, "text 1abc malformed");
    expect(badNum.get("x") == 9.0f, "text 1abc no mutate");

    const char* letters = "x:abc";
    expect(azdeckParseText(letters, std::strlen(letters), badNum) == AZDECK_TEXT_MALFORMED, "text abc malformed");
    const char* nanText = "x:NaN";
    expect(azdeckParseText(nanText, std::strlen(nanText), badNum) == AZDECK_TEXT_MALFORMED, "text NaN malformed");
    const char* infText = "x:Inf";
    expect(azdeckParseText(infText, std::strlen(infText), badNum) == AZDECK_TEXT_MALFORMED, "text Inf malformed");

    AzDeckChannelStore partial;
    partial.set("x", 9.0f);
    partial.set("y", 8.0f);
    const char* mixed = "x:1 y:oops";
    expect(azdeckParseText(mixed, std::strlen(mixed), partial) == AZDECK_TEXT_MALFORMED, "text mixed malformed");
    expect(partial.get("x") == 9.0f, "text no partial x");
    expect(partial.get("y") == 8.0f, "text no partial y");

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
    const char* spaced = "{\"x\":1}   {\"y\":2}";
    framer.feed(
        reinterpret_cast<const uint8_t*>(spaced),
        std::strlen(spaced),
        onObject,
        onFail,
        &capture
    );
    expect(capture.objects.size() == 2, "whitespace between objects");

    capture.objects.clear();
    capture.fails = 0;
    const char* recovered = "xx{\"z\":3}";
    framer.reset();
    framer.feed(
        reinterpret_cast<const uint8_t*>(recovered),
        std::strlen(recovered),
        onObject,
        onFail,
        &capture
    );
    expect(capture.fails > 0, "malformed prefix fail");
    expect(capture.objects.size() == 1, "malformed prefix recovery");

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
    const char* exact = "{\"n\":42}";
    const size_t exactLen = std::strlen(exact);
    framer.reset();
    for (size_t i = 0; i < exactLen; ++i) {
        framer.feed(
            reinterpret_cast<const uint8_t*>(exact + i),
            1,
            onObject,
            onFail,
            &capture
        );
    }
    expect(capture.objects.size() == 1, "split at every byte");
    expect(capture.fails == 0, "byte split no fail");

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

static void testTextFramer() {
    AzDeckTextStreamFramer framer;
    FramerCapture capture;
    capture.fails = 0;

    std::string longMsg = "speed:180 move_x:0.5 move_y:-0.25 l2:1 extra:1";
    while (longMsg.size() < 80) {
        longMsg += " k:1";
    }
    expect(longMsg.size() > 64, "text message longer than 64");

    for (size_t i = 0; i < longMsg.size(); i += 64) {
        const size_t n = (i + 64 < longMsg.size()) ? 64 : (longMsg.size() - i);
        expect(
            framer.append(reinterpret_cast<const uint8_t*>(longMsg.data() + i), n),
            "text append chunk"
        );
    }
    framer.processAfterBurst(onObject, onFail, &capture);
    expect(capture.objects.size() == 1, "text drained available is one message");
    expect(capture.objects[0] == longMsg, "text full message preserved");

    capture.objects.clear();
    framer.reset();
    const char* first = "move_x:1";
    const char* second = " move_y:2\n";
    expect(framer.append(reinterpret_cast<const uint8_t*>(first), std::strlen(first)), "text partial line");
    framer.processAfterBurst(onObject, onFail, &capture);
    expect(capture.objects.size() == 1, "text no-lf burst emits");
    capture.objects.clear();
    framer.reset();
    expect(framer.append(reinterpret_cast<const uint8_t*>(first), std::strlen(first)), "text hold start");
    expect(framer.append(reinterpret_cast<const uint8_t*>(second), std::strlen(second)), "text lf arrives");
    framer.processAfterBurst(onObject, onFail, &capture);
    expect(capture.objects.size() == 1, "text lf completes line");
    expect(capture.objects[0] == "move_x:1 move_y:2", "text lf payload");

    capture.objects.clear();
    framer.reset();
    const char* crlf = "a:1\r\nb:2\r\n";
    expect(framer.append(reinterpret_cast<const uint8_t*>(crlf), std::strlen(crlf)), "text crlf append");
    framer.processAfterBurst(onObject, onFail, &capture);
    expect(capture.objects.size() == 2, "text crlf two lines");

    capture.objects.clear();
    framer.reset();
    const char* many = "a:1\nb:2\nc:3\n";
    expect(framer.append(reinterpret_cast<const uint8_t*>(many), std::strlen(many)), "text multi append");
    framer.processAfterBurst(onObject, onFail, &capture);
    expect(capture.objects.size() == 3, "text multiple lines");

    capture.objects.clear();
    framer.reset();
    const char* primed = "a:1\n";
    expect(framer.append(reinterpret_cast<const uint8_t*>(primed), std::strlen(primed)), "text prime lf");
    framer.processAfterBurst(onObject, onFail, &capture);
    expect(capture.objects.size() == 1, "text prime line");
    capture.objects.clear();
    const char* held = "speed:1";
    expect(framer.append(reinterpret_cast<const uint8_t*>(held), std::strlen(held)), "text hold after lf");
    framer.processAfterBurst(onObject, onFail, &capture);
    expect(capture.objects.empty(), "text partial line kept across burst");
    const char* rest = "80\n";
    expect(framer.append(reinterpret_cast<const uint8_t*>(rest), std::strlen(rest)), "text rest");
    framer.processAfterBurst(onObject, onFail, &capture);
    expect(capture.objects.size() == 1, "text split across bursts with lf");
    expect(capture.objects[0] == "speed:180", "text split payload");

    framer.reset();
    std::string huge(AZDECK_RX_BUFFER_SIZE, 'x');
    expect(!framer.append(reinterpret_cast<const uint8_t*>(huge.data()), huge.size()), "text overflow");
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
    testJsonParser();
    testTextParser();
    testJsonFramer();
    testTextFramer();
    testPing();

    if (gFailures > 0) {
        std::printf("%d test(s) failed\n", gFailures);
        return 1;
    }
    std::printf("all tests passed\n");
    return 0;
}
