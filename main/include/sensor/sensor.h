#pragma once
#include <stdbool.h>
#include <time.h>
#include "esp_err.h"

/* Unified sensor reading structure */
typedef struct {
    float temperature;
    float humidity;
    time_t timestamp;
} sensor_reading_t;

/* Initialize sensor hardware (idempotent) */
esp_err_t sensor_init(void);

/* Obtain one reading (simulate -> generate random values) */
esp_err_t sensor_read_once(bool simulate, sensor_reading_t *out);

/* Availability / self-test placeholder */
bool sensor_is_available(void);