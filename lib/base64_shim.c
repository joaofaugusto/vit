// base64_shim.c — Base64 encode/decode for Vit
// Self-contained, no external dependencies.

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static const char B64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char* vit_base64_encode(const char* data) {
    size_t len = strlen(data);
    size_t out_len = 4 * ((len + 2) / 3) + 1;
    char* out = malloc(out_len);
    size_t i, j = 0;
    for (i = 0; i + 2 < len; i += 3) {
        out[j++] = B64[(uint8_t)data[i] >> 2];
        out[j++] = B64[((uint8_t)data[i] & 3) << 4 | (uint8_t)data[i+1] >> 4];
        out[j++] = B64[((uint8_t)data[i+1] & 0xf) << 2 | (uint8_t)data[i+2] >> 6];
        out[j++] = B64[(uint8_t)data[i+2] & 0x3f];
    }
    if (i < len) {
        out[j++] = B64[(uint8_t)data[i] >> 2];
        if (i + 1 < len) {
            out[j++] = B64[((uint8_t)data[i] & 3) << 4 | (uint8_t)data[i+1] >> 4];
            out[j++] = B64[((uint8_t)data[i+1] & 0xf) << 2];
        } else {
            out[j++] = B64[((uint8_t)data[i] & 3) << 4];
            out[j++] = '=';
        }
        out[j++] = '=';
    }
    out[j] = '\0';
    return out;
}

static int b64val(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

char* vit_base64_decode(const char* data) {
    size_t len = strlen(data);
    char* out = malloc(len + 1);
    size_t j = 0;
    for (size_t i = 0; i + 3 < len; i += 4) {
        int a = b64val(data[i]),   b = b64val(data[i+1]);
        int c = b64val(data[i+2]), d = b64val(data[i+3]);
        if (a < 0 || b < 0) break;
        out[j++] = (char)((a << 2) | (b >> 4));
        if (c >= 0) out[j++] = (char)((b << 4) | (c >> 2));
        if (d >= 0) out[j++] = (char)((c << 6) | d);
    }
    out[j] = '\0';
    return out;
}

void vit_base64_free(char* s) {
    if (s) free(s);
}
