#pragma once
#include <stdbool.h>
#include "esp_err.h"
#include "sensor.h"

/* Fixed maximum (RTC friendly) */
#define BATCH_STORE_MAX_READINGS 50

/* Initialize (idempotent). Clears only if force_clear=true. */
esp_err_t batch_store_init(bool force_clear);

/* Add a reading (copies). Returns ESP_ERR_NO_MEM if full. */
esp_err_t batch_store_add(const sensor_reading_t *reading);

/* Current counters */
int  batch_store_count(void);
int  batch_store_capacity(void);
bool batch_store_is_full(void);

/* Accessor (NULL if idx out of range) */
const sensor_reading_t *batch_store_get(int idx);

/* Clear all readings */
void batch_store_clear(void);

/* Build JSON payload (same format used before: {"data":[...]})
 * Allocates a cJSON_Print() string into *out_json. Caller must free(*out_json).
 */
esp_err_t batch_store_build_json(char **out_json);