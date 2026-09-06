#pragma once

#include <stdint.h>

/* Userspace policy boundary for the growing Obsidia Windows ABI subsystem. */
typedef enum {
    WINABI_OK = 0,
    WINABI_UNSUPPORTED_ARCH = -1,
    WINABI_MALFORMED_IMAGE = -2,
    WINABI_IMPORT_MODULE_MISSING = -3,
    WINABI_IMPORT_SYMBOL_MISSING = -4,
    WINABI_RELOCATION_UNSUPPORTED = -5
} winabi_status_t;

typedef struct {
    const char* module_name;
    const char* symbol_name;
    uint16_t ordinal;
    uint8_t by_ordinal;
} winabi_import_t;

/* Future DLL providers implement this without adding Windows policy to the kernel. */
typedef uint64_t (*winabi_resolve_import_fn)(const winabi_import_t* import,void* context);
