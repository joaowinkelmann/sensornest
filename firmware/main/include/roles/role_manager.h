#pragma once
#include "esp_err.h"

typedef enum {
    DEVICE_ROLE_UNKNOWN = 0,
    DEVICE_ROLE_WORKER,
    DEVICE_ROLE_CONTROLLER
} device_role_t;

device_role_t role_manager_get_cached(void);

/* Reads NVS if needed, then starts appropriate role.
 * Returns only for WORKER in debug mode (loop simulates wake cycles),
 * or never returns for production deep sleep flows.
 */
esp_err_t role_manager_start(void);