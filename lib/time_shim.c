// time_shim.c — time functions for Vit
// Self-contained: uses only POSIX libc (time.h, unistd.h).

// Required so glibc exposes strptime/timegm prototypes.
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Unix timestamp in seconds.
int64_t vit_time_now(void) {
    return (int64_t)time(NULL);
}

// Unix timestamp in milliseconds.
int64_t vit_time_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// Formats a Unix timestamp using strftime patterns.
// Common patterns:
//   "%Y-%m-%d"           → "2025-03-23"
//   "%Y-%m-%dT%H:%M:%SZ" → "2025-03-23T14:05:00Z"
//   "%d/%m/%Y %H:%M"     → "23/03/2025 14:05"
// Returns heap-allocated string; caller must free with vit_time_free().
char* vit_time_format(int64_t ts, const char* fmt) {
    time_t t = (time_t)ts;
    struct tm* tm_info = gmtime(&t);
    char* buf = malloc(256);
    strftime(buf, 256, fmt, tm_info);
    return buf;
}

// Same as vit_time_format but uses local timezone instead of UTC.
char* vit_time_format_local(int64_t ts, const char* fmt) {
    time_t t = (time_t)ts;
    struct tm* tm_info = localtime(&t);
    char* buf = malloc(256);
    strftime(buf, 256, fmt, tm_info);
    return buf;
}

// Parses an ISO-8601 date string ("YYYY-MM-DD" or "YYYY-MM-DDTHH:MM:SSZ")
// and returns a Unix timestamp in seconds. Returns -1 on failure.
int64_t vit_time_parse(const char* s) {
    struct tm tm = {0};
    // Try full datetime first, then date-only
    if (strptime(s, "%Y-%m-%dT%H:%M:%SZ", &tm) ||
        strptime(s, "%Y-%m-%dT%H:%M:%S",  &tm) ||
        strptime(s, "%Y-%m-%d",            &tm)) {
        return (int64_t)timegm(&tm);
    }
    return -1;
}

// Sleep for ms milliseconds.
void vit_time_sleep(int ms) {
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

void vit_time_free(char* s) {
    if (s) free(s);
}
