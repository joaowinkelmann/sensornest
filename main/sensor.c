#include "sensor.h"
#include "dht.h"
#include "esp_random.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include <time.h>                 // <--- ADDED
#include "freertos/FreeRTOS.h"    // <--- ADDED
#include "freertos/task.h"        // <--- ADDED
#include <string.h>

#define SENSOR_TAG        "SENSOR"
#define DHT_TYPE          DHT_TYPE_DHT11
#define DHT_GPIO          4
#define DHT_PULLUP_MS     1500
#define DHT_MAX_RETRIES   3

static bool s_inited = false;

esp_err_t sensor_init(void) {
    if (s_inited) return ESP_OK;
    gpio_set_direction(DHT_GPIO, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_set_pull_mode(DHT_GPIO, GPIO_PULLUP_ONLY);
    s_inited = true;
    return ESP_OK;
}

bool sensor_is_available(void) {
    return s_inited; /* Later: could add self‑test */
}

static void fill_simulated(sensor_reading_t *out) {
    out->temperature = 20.0f + (float)(esp_random() % 100) / 10.0f;
    out->humidity    = 40.0f + (float)(esp_random() % 200) / 10.0f;
    time(&out->timestamp);
}

esp_err_t sensor_read_once(bool simulate, sensor_reading_t *out) {
    if (!out) return ESP_ERR_INVALID_ARG;

    ESP_ERROR_CHECK(sensor_init());

    if (simulate) {
        fill_simulated(out);
        ESP_LOGI(SENSOR_TAG, "Simulated reading: T=%.1fC H=%.1f%%", out->temperature, out->humidity);
        return ESP_OK;
    }

    int16_t raw_h = 0, raw_t = 0;
    esp_err_t last = ESP_FAIL;

    for (int attempt = 0; attempt < DHT_MAX_RETRIES; ++attempt) {
        vTaskDelay(pdMS_TO_TICKS(DHT_PULLUP_MS));
        last = dht_read_data(DHT_TYPE, DHT_GPIO, &raw_h, &raw_t);
        if (last == ESP_OK) break;
        ESP_LOGW(SENSOR_TAG, "DHT read fail (%d/%d): %s", attempt + 1, DHT_MAX_RETRIES, esp_err_to_name(last));
    }

    if (last != ESP_OK) {
        ESP_LOGE(SENSOR_TAG, "DHT final failure: %s", esp_err_to_name(last));
        return last;
    }

    out->temperature = (float)raw_t / 10.0f;
    out->humidity    = (float)raw_h / 10.0f;
    time(&out->timestamp);
    ESP_LOGI(SENSOR_TAG, "Reading: T=%.1fC H=%.1f%%", out->temperature, out->humidity);
    return ESP_OK;
}