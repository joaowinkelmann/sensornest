#pragma once
#include "esp_err.h"
#include <stdbool.h>

/* Starts controller role (listener + uploader loop, future expansion). */
esp_err_t controller_role_start(void);
void controller_role_set_debug(bool enabled);