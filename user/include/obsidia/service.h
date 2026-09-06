#pragma once

#include <stdint.h>

/*
 * Obsidia Service API
 *
 * Services are long-running userspace processes that expose an IPC
 * endpoint under a human-readable name.
 *
 * Example:
 *
 *     os_service_register("display", endpoint);
 *
 * A client can later request access to that service:
 *
 *     int64_t display = os_service_connect("display");
 *
 * The returned value is an IPC handle owned by the calling process.
 */

#define OS_SERVICE_NAME_MAX 32

/*
 * Register an IPC endpoint as a named system service.
 *
 * name:
 *     Null-terminated service name.
 *
 * endpoint:
 *     IPC endpoint handle belonging to the caller.
 *
 * Returns:
 *      0 on success
 *     -1 on failure
 */
int os_service_register(
    const char* name,
    uint64_t endpoint
);

/*
 * Connect to a named service.
 *
 * Returns:
 *     IPC endpoint handle on success
 *     negative value on failure
 */
int64_t os_service_connect(
    const char* name
);
