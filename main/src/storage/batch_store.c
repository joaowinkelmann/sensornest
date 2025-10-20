#include "storage/batch_store.h"
#include <string.h>
#include <time.h>
#include "cJSON.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_mac.h"

static const char *TAG_BS = "BATCH_STORE";

RTC_DATA_ATTR static sensor_reading_t s_readings[BATCH_STORE_MAX_READINGS];
RTC_DATA_ATTR static int s_count = 0;

esp_err_t batch_store_init(bool force_clear) {
    if (force_clear) {
        s_count = 0;
        memset(s_readings, 0, sizeof(s_readings));
    }
    return ESP_OK;
}

int batch_store_capacity(void) { return BATCH_STORE_MAX_READINGS; }
int batch_store_count(void)    { return s_count; }
bool batch_store_is_full(void) { return s_count >= BATCH_STORE_MAX_READINGS; }

const sensor_reading_t *batch_store_get(int idx) {
    if (idx < 0 || idx >= s_count) return NULL;
    return &s_readings[idx];
}

esp_err_t batch_store_add(const sensor_reading_t *reading) {
    if (!reading) return ESP_ERR_INVALID_ARG;
    if (batch_store_is_full()) return ESP_ERR_NO_MEM;
    s_readings[s_count++] = *reading;
    return ESP_OK;
}

void batch_store_clear(void) {
    s_count = 0;
}

esp_err_t batch_store_build_json(char **out_json) {
    if (!out_json) return ESP_ERR_INVALID_ARG;
    *out_json = NULL;

    cJSON *root = cJSON_CreateObject();
    if (!root) return ESP_ERR_NO_MEM;

    cJSON *arr = cJSON_CreateArray();
    if (!arr) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }
    // API requires key "data"
    cJSON_AddItemToObject(root, "data", arr);

    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    for (int i = 0; i < s_count; ++i) {
        const sensor_reading_t *r = &s_readings[i];
        cJSON *obj = cJSON_CreateObject();
        if (!obj) continue;

        // Required fields
        cJSON_AddStringToObject(obj, "mac", mac_str);
        cJSON_AddNumberToObject(obj, "temp", r->temperature);
        cJSON_AddNumberToObject(obj, "hum", r->humidity);

        // Optional readingTime (server uses processing time if omitted)
        // Format: YYYY-MM-DDTHH:MM:SS.000Z
        char iso[32];
        struct tm tm_info;
        gmtime_r(&r->timestamp, &tm_info);
        // seconds
        strftime(iso, sizeof(iso), "%Y-%m-%dT%H:%M:%S", &tm_info);
        // append milliseconds + Z
        strlcat(iso, ".000Z", sizeof(iso));
        cJSON_AddStringToObject(obj, "readingTime", iso);

        cJSON_AddItemToArray(arr, obj);
    }

    char *printed = cJSON_PrintUnformatted(root);
    if (!printed) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }
    *out_json = printed;
    cJSON_Delete(root);
    ESP_LOGI(TAG_BS, "Built JSON payload (%d readings).", s_count);
    return ESP_OK;
}