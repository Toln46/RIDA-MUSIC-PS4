#include "audius.h"

#include <orbis/Http.h>
#include <orbis/Ssl.h>
#include <orbis/Net.h>
#include <orbis/Sysmodule.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>

#define AUDIUS_NET_POOL (64 * 1024)
#define AUDIUS_HTTP_POOL (256 * 1024)
#define AUDIUS_MAX_JSON (512 * 1024)

static int g_netPool = -1;
static int g_ssl = -1;
static int g_http = -1;
static bool g_ready = false;
static char g_error[160] = "NOT INITIALIZED";

static void set_error(const char* value) {
    snprintf(g_error, sizeof(g_error), "%s", value ? value : "UNKNOWN ERROR");
}

const char* audius_last_error() { return g_error; }

bool audius_init() {
    if (g_ready) return true;

    int rc = sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET);
    if (rc < 0) { set_error("NET MODULE LOAD FAILED"); return false; }
    rc = sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SSL);
    if (rc < 0) { set_error("SSL MODULE LOAD FAILED"); return false; }
    rc = sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_HTTP);
    if (rc < 0) { set_error("HTTP MODULE LOAD FAILED"); return false; }

    rc = sceNetInit();
    if (rc < 0) { set_error("NETWORK INIT FAILED"); return false; }

    g_netPool = sceNetPoolCreate("RidaAudiusNet", AUDIUS_NET_POOL, 0);
    if (g_netPool < 0) { set_error("NETWORK POOL FAILED"); return false; }

    g_ssl = sceSslInit(SSL_POOLSIZE);
    if (g_ssl < 0) { set_error("SSL INIT FAILED"); return false; }

    g_http = sceHttpInit(g_netPool, g_ssl, AUDIUS_HTTP_POOL);
    if (g_http < 0) { set_error("HTTP INIT FAILED"); return false; }

    g_ready = true;
    set_error("READY");
    return true;
}

static bool http_get(const char* url, std::string& body) {
    if (!audius_init()) return false;

    int tpl = sceHttpCreateTemplate(g_http, "RGS-MUSIC-PS4/0.04", ORBIS_HTTP_VERSION_1_1, 1);
    if (tpl < 0) { set_error("HTTP TEMPLATE FAILED"); return false; }
    sceHttpSetConnectTimeOut(tpl, 12000000);
    sceHttpSetSendTimeOut(tpl, 12000000);
    sceHttpSetResolveTimeOut(tpl, 12000000);

    int conn = sceHttpCreateConnectionWithURL(tpl, url, 1);
    if (conn < 0) {
        sceHttpDeleteTemplate(tpl);
        set_error("HTTPS CONNECTION FAILED");
        return false;
    }

    int req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_GET, url, 0);
    if (req < 0) {
        sceHttpDeleteConnection(conn);
        sceHttpDeleteTemplate(tpl);
        set_error("HTTP REQUEST FAILED");
        return false;
    }

    int ok = false;
    int rc = sceHttpSendRequest(req, NULL, 0);
    int status = 0;
    if (rc < 0 || sceHttpGetStatusCode(req, &status) < 0) {
        set_error("HTTP SEND FAILED");
        goto cleanup;
    }
    if (status != 200) {
        snprintf(g_error, sizeof(g_error), "AUDIUS HTTP STATUS %d", status);
        goto cleanup;
    }

    body.clear();
    char chunk[8192];
    for (;;) {
        int got = sceHttpReadData(req, chunk, sizeof(chunk));
        if (got < 0) { set_error("HTTP READ FAILED"); goto cleanup; }
        if (got == 0) break;
        if (body.size() + (size_t)got > AUDIUS_MAX_JSON) {
            set_error("AUDIUS RESPONSE TOO LARGE");
            goto cleanup;
        }
        body.append(chunk, (size_t)got);
    }

    ok = !body.empty();
    if (ok) set_error("READY");

cleanup:
    sceHttpDeleteRequest(req);
    sceHttpDeleteConnection(conn);
    sceHttpDeleteTemplate(tpl);
    return ok;
}

static void url_encode(const char* input, char* output, size_t cap) {
    static const char hex[] = "0123456789ABCDEF";
    size_t p = 0;
    for (size_t i = 0; input && input[i] && p + 4 < cap; ++i) {
        unsigned char c = (unsigned char)input[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            output[p++] = (char)c;
        } else if (c == ' ') {
            output[p++] = '+';
        } else {
            output[p++] = '%';
            output[p++] = hex[c >> 4];
            output[p++] = hex[c & 15];
        }
    }
    output[p] = 0;
}

static bool read_json_string(const char* object, size_t length, const char* key,
                             char* output, size_t capacity) {
    char needle[48];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char* end = object + length;
    const char* p = object;
    size_t needleLen = strlen(needle);
    while (p + needleLen < end) {
        const char* found = strstr(p, needle);
        if (!found || found >= end) return false;
        p = found + needleLen;
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;
        if (p >= end || *p++ != ':') continue;
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;
        if (p >= end || *p != '"') return false;
        ++p;
        size_t out = 0;
        while (p < end && *p != '"') {
            char c = *p++;
            if (c == '\\' && p < end) {
                char escaped = *p++;
                if (escaped == 'n' || escaped == 'r' || escaped == 't') c = ' ';
                else c = escaped;
            }
            if (out + 1 < capacity) output[out++] = c;
        }
        if (capacity) output[out] = 0;
        return true;
    }
    return false;
}

static int read_json_int(const char* object, size_t length, const char* key) {
    char needle[48];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char* end = object + length;
    const char* p = object;
    while (p + strlen(needle) < end) {
        const char* found = strstr(p, needle);
        if (!found || found >= end) return 0;
        p = found + strlen(needle);
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;
        if (p >= end || *p++ != ':') continue;
        while (p < end && (*p == ' ' || *p == '\t')) ++p;
        return p < end ? atoi(p) : 0;
    }
    return 0;
}

static int parse_tracks(const std::string& json, AudiusTrack* tracks, int capacity) {
    const char* data = strstr(json.c_str(), "\"data\"");
    if (!data) return 0;
    const char* p = strchr(data, '[');
    if (!p) return 0;
    ++p;
    int count = 0;

    while (*p && count < capacity) {
        while (*p && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t' || *p == ',')) ++p;
        if (*p == ']') break;
        if (*p != '{') { ++p; continue; }

        const char* start = p;
        int depth = 0;
        bool inString = false;
        bool escaped = false;
        do {
            char c = *p++;
            if (inString) {
                if (escaped) escaped = false;
                else if (c == '\\') escaped = true;
                else if (c == '"') inString = false;
            } else {
                if (c == '"') inString = true;
                else if (c == '{') ++depth;
                else if (c == '}') --depth;
            }
        } while (*p && depth > 0);

        if (depth != 0) break;
        size_t objectLen = (size_t)(p - start);
        AudiusTrack& track = tracks[count];
        memset(&track, 0, sizeof(track));
        read_json_string(start, objectLen, "id", track.id, sizeof(track.id));
        read_json_string(start, objectLen, "title", track.title, sizeof(track.title));
        track.duration = read_json_int(start, objectLen, "duration");

        const char* user = strstr(start, "\"user\"");
        if (user && user < p) {
            const char* userStart = strchr(user, '{');
            if (userStart && userStart < p)
                read_json_string(userStart, (size_t)(p - userStart), "name",
                                 track.artist, sizeof(track.artist));
        }

        if (track.id[0] && track.title[0]) ++count;
    }
    return count;
}

int audius_search(const char* query, AudiusTrack* tracks, int capacity) {
    if (!query || !tracks || capacity < 1) {
        set_error("INVALID SEARCH");
        return -1;
    }

    char encoded[256];
    char url[512];
    url_encode(query, encoded, sizeof(encoded));
    snprintf(url, sizeof(url),
             "https://api.audius.co/v1/tracks/search?query=%s&limit=%d",
             encoded, capacity);

    std::string response;
    if (!http_get(url, response)) return -1;

    int count = parse_tracks(response, tracks, capacity);
    if (count == 0) set_error("NO TRACKS FOUND");
    else set_error("SEARCH COMPLETE");
    return count;
}
