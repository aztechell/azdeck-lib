#include "SettingsHttpProtocol.h"

#include <stdio.h>
#include <string.h>

namespace {

bool headersComplete(const char* request, size_t length) {
    if (request == nullptr || length < 4) {
        return false;
    }
    for (size_t i = 3; i < length; ++i) {
        if (request[i - 3] == '\r' && request[i - 2] == '\n' &&
            request[i - 1] == '\r' && request[i] == '\n') {
            return true;
        }
    }
    return false;
}

char upperAscii(char c) {
    if (c >= 'a' && c <= 'z') {
        return static_cast<char>(c - 'a' + 'A');
    }
    return c;
}

bool methodEquals(const char* line, const char* method) {
    size_t i = 0;
    while (method[i] != '\0') {
        if (upperAscii(line[i]) != method[i]) {
            return false;
        }
        ++i;
    }
    return line[i] == ' ';
}

bool pathIsSettings(const char* path) {
    const char* prefix = "/settings";
    size_t i = 0;
    while (prefix[i] != '\0') {
        if (path[i] != prefix[i]) {
            return false;
        }
        ++i;
    }
    const char c = path[i];
    return c == ' ' || c == '?' || c == '\r' || c == '\n' || c == '\0';
}

}  // namespace

AzDeckSettingsHttpKind azdeckSettingsHttpClassify(
    const char* request,
    size_t length
) {
    if (request == nullptr || length == 0) {
        return AZDECK_HTTP_INCOMPLETE;
    }
    if (!headersComplete(request, length)) {
        return AZDECK_HTTP_INCOMPLETE;
    }

    const char* path = request;
    while (*path != '\0' && *path != ' ' && *path != '\r' && *path != '\n') {
        ++path;
    }
    if (*path != ' ') {
        return AZDECK_HTTP_OTHER;
    }
    ++path;
    if (*path != '/') {
        return AZDECK_HTTP_OTHER;
    }

    if (!pathIsSettings(path)) {
        return AZDECK_HTTP_OTHER;
    }

    if (methodEquals(request, "HEAD")) {
        return AZDECK_HTTP_SETTINGS_HEAD;
    }
    if (methodEquals(request, "GET")) {
        return AZDECK_HTTP_SETTINGS_GET;
    }
    return AZDECK_HTTP_OTHER;
}

size_t azdeckSettingsHttpFormatOk(
    char* out,
    size_t capacity,
    size_t bodyLength
) {
    if (out == nullptr || capacity == 0) {
        return 0;
    }
    const int n = snprintf(
        out,
        capacity,
        "HTTP/1.1 200 OK\r\n"
        "X-AzDeck-Settings: 1\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Cache-Control: no-store\r\n"
        "Content-Length: %u\r\n"
        "Connection: close\r\n"
        "\r\n",
        static_cast<unsigned>(bodyLength)
    );
    if (n < 0 || static_cast<size_t>(n) >= capacity) {
        out[0] = '\0';
        return 0;
    }
    return static_cast<size_t>(n);
}

size_t azdeckSettingsHttpFormatNotFound(char* out, size_t capacity) {
    if (out == nullptr || capacity == 0) {
        return 0;
    }
    const char* text =
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "Cache-Control: no-store\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n"
        "\r\n";
    const size_t n = strlen(text);
    if (n >= capacity) {
        out[0] = '\0';
        return 0;
    }
    memcpy(out, text, n + 1);
    return n;
}

size_t azdeckSettingsHttpQuery(
    const char* request,
    size_t length,
    char* out,
    size_t capacity
) {
    if (out == nullptr || capacity == 0) {
        return 0;
    }
    out[0] = '\0';
    if (request == nullptr || length == 0) {
        return 0;
    }

    const char* path = request;
    const char* end = request + length;
    while (path < end && *path != '\0' && *path != ' ' && *path != '\r' && *path != '\n') {
        ++path;
    }
    if (path >= end || *path != ' ') {
        return 0;
    }
    ++path;

    const char* prefix = "/settings";
    size_t i = 0;
    while (prefix[i] != '\0') {
        if (path >= end || *path == '\0' || *path != prefix[i]) {
            return 0;
        }
        ++path;
        ++i;
    }
    if (path >= end || *path != '?') {
        return 0;
    }
    ++path;

    size_t n = 0;
    while (path < end && *path != '\0' && *path != ' ' && *path != '\r' && *path != '\n') {
        if (n + 1 >= capacity) {
            out[0] = '\0';
            return 0;
        }
        out[n++] = *path++;
    }
    out[n] = '\0';
    return n;
}
