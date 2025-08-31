#pragma once
#include <stdbool.h>
#include "esp_err.h"
#include "role_manager.h"  // provides device_role_t

esp_err_t worker_role_start(void);
void worker_role_set_debug(bool enabled);