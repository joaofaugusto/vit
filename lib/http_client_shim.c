// http_client_shim.c — libcurl wrappers for Vit
//
// All responses are heap-allocated (malloc). Vit does not manage this memory,
// so for long-running servers call http_client_free() after each request.
//
// Requires: libcurl4-openssl-dev
//   sudo apt install libcurl4-openssl-dev

#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ── response buffer ───────────────────────────────────────────────────────────

typedef struct {
    char*  data;
    size_t len;
} CurlBuf;

static size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    CurlBuf* buf = (CurlBuf*)userdata;
    size_t n = size * nmemb;
    char* tmp = realloc(buf->data, buf->len + n + 1);
    if (!tmp) return 0;
    buf->data = tmp;
    memcpy(buf->data + buf->len, ptr, n);
    buf->len += n;
    buf->data[buf->len] = '\0';
    return n;
}

// ── last status code (global, updated on every request) ───────────────────────

static int vit_last_status = 0;

int vit_http_client_status(void) {
    return vit_last_status;
}

// ── free response body ────────────────────────────────────────────────────────

void vit_http_client_free(char* ptr) {
    if (ptr) free(ptr);
}

// ── internal: run a request ───────────────────────────────────────────────────

static char* do_request(
    const char* url,
    const char* method,      // NULL = GET
    const char* body,        // NULL = no body
    const char* content_type // NULL = skip Content-Type header
) {
    CURL* curl = curl_easy_init();
    if (!curl) return strdup("");

    CurlBuf buf = { NULL, 0 };

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    struct curl_slist* headers = NULL;
    if (content_type) {
        char hdr[256];
        snprintf(hdr, sizeof(hdr), "Content-Type: %s", content_type);
        headers = curl_slist_append(headers, hdr);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    if (method && strcmp(method, "POST") == 0) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body ? body : "");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)(body ? strlen(body) : 0));
    } else if (method && strcmp(method, "PUT") == 0) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body ? body : "");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)(body ? strlen(body) : 0));
    } else if (method && strcmp(method, "DELETE") == 0) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    } else if (method && strcmp(method, "PATCH") == 0) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body ? body : "");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)(body ? strlen(body) : 0));
    }

    curl_easy_perform(curl);

    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    vit_last_status = (int)status;

    if (headers) curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (!buf.data) return strdup("");
    return buf.data;
}

// ── public API ────────────────────────────────────────────────────────────────

char* vit_http_get(const char* url) {
    return do_request(url, NULL, NULL, NULL);
}

char* vit_http_post(const char* url, const char* body) {
    return do_request(url, "POST", body, "application/x-www-form-urlencoded");
}

char* vit_http_post_json(const char* url, const char* body) {
    return do_request(url, "POST", body, "application/json");
}

char* vit_http_put_json(const char* url, const char* body) {
    return do_request(url, "PUT", body, "application/json");
}

char* vit_http_patch_json(const char* url, const char* body) {
    return do_request(url, "PATCH", body, "application/json");
}

char* vit_http_delete(const char* url) {
    return do_request(url, "DELETE", NULL, NULL);
}
