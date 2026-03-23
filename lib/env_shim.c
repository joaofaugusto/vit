// env_shim.c — null-safe wrappers for environment variable access
// getenv() returns NULL when the variable is not set; Vit expects str (never null).

#include <stdlib.h>

const char* vit_getenv(const char* name) {
    const char* val = getenv(name);
    return val ? val : "";
}
