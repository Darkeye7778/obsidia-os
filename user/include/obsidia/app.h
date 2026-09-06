#pragma once

#include <stdint.h>

typedef enum {
    OBS_EXEC_UNKNOWN = 0,
    OBS_EXEC_ELF64 = 1,
    OBS_EXEC_NATIVE = 2,
    OBS_EXEC_PE32 = 3,
    OBS_EXEC_PE32_PLUS = 4
} obs_exec_format_t;

/* Packaging convention only; the dispatcher always trusts magic, never suffix. */
#define OBS_NATIVE_EXEC_EXTENSION ".obsx"

/* Detects from validated file contents. File suffixes have no semantic role. */
obs_exec_format_t obs_exec_detect(const char* path);

/* Unified application launch. The executable dispatcher selects the runtime. */
int64_t obs_app_launch(const char* path);
