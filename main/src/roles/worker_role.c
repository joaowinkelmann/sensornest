#include "roles/worker_role.h"
#include "esp_log.h"
#include "sensor/sensor.h"
#include "net/espnow_manager.h"
#include "storage/batch_store.h"
#include "net/rest_client.h"
#include "esp_sntp.h"
#include "nvs.h"
#include "esp_wifi.h"
#include "esp_sleep.h"
#include "roles/role_manager.h"
#include "net/wifi_manager.h"
#include "indicator/led_indicator.h"

// Forward declare (temporarily reusing code from tempmesh.c until fully moved)
extern bool send_batched_data(void);
extern void perform_sensor_reading(void);
extern void simulate_or_sleep(uint64_t secs);

static const char *TAG_WR = "WORKER_ROLE";

static bool s_debug = false;
extern RTC_DATA_ATTR bool time_is_set;
extern RTC_DATA_ATTR bool device_config_initialized;
extern RTC_DATA_ATTR int device_readingsPerBatch;
extern RTC_DATA_ATTR int device_wakeUpInterval;

/* Hooks implemented in tempmesh.c we still reuse */
extern bool send_batched_data(void);          // will be internalized later
extern void perform_sensor_reading(void);     // same
extern void simulate_or_sleep(uint64_t secs); // same

void worker_role_set_debug(bool enabled) { s_debug = enabled; }

static void ensure_time(void) {
    if (time_is_set) return;
    ESP_LOGI(TAG_WR, "Syncing SNTP time...");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
    int retry = 0;
    while (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < 15) {
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    if (retry < 15) { time_is_set = true; ESP_LOGI(TAG_WR, "Time synced."); }
    else ESP_LOGW(TAG_WR, "Time sync failed.");
}

static void fetch_remote_config(void) {
    if (device_config_initialized) return;
    char user_email[128], user_pass[128];
    if (rc_load_creds(user_email, sizeof(user_email), user_pass, sizeof(user_pass)) &&
        rc_login(user_email, user_pass, true) == ESP_OK) {
        char *device_json = NULL;
        if (rc_get_device_by_mac(NULL, &device_json) == ESP_OK && device_json) {
            int batch = -1, interval = -1;
            rc_extract_device_config(device_json, &batch, &interval);
            free(device_json);
            if (batch > 0) device_readingsPerBatch = batch;
            if (interval > 0) device_wakeUpInterval = interval;
            device_config_initialized = true;
            ESP_LOGI(TAG_WR, "Applied remote config: readingsPerBatch=%d wakeUpInterval=%d",
                     device_readingsPerBatch, device_wakeUpInterval);
        }
    }
}

esp_err_t worker_role_start(void) {
    static bool espnow_inited = false;
    if (!espnow_inited) {
        if (espnow_manager_init(false, NULL) == ESP_OK) {
            espnow_inited = true;
        } else {
            ESP_LOGW(TAG_WR, "Failed to init ESPNOW (will still attempt legacy Wi-Fi path).");
        }
    }

    /* Debug identification flash */
    if (s_debug) {
        led_indicator_init();
        led_indicator_flash_worker_debug();
    }

    // Single cycle (mirrors existing loop) – deep sleep ends task.
    perform_sensor_reading();
    if (s_debug) {
        vTaskDelay(pdMS_TO_TICKS(150));
        led_indicator_off();
    }

    if (batch_store_count() >= device_readingsPerBatch) {
        ESP_LOGI(TAG_WR, "Batch full (%d/%d). Sending via ESPNOW.",
                 batch_store_count(), device_readingsPerBatch);

        if (espnow_inited) {
            char *json_payload = NULL;
            if (batch_store_build_json(&json_payload) == ESP_OK && json_payload) {
                size_t len = strlen(json_payload);
                esp_err_t se = espnow_manager_send_broadcast(json_payload, len);
                if (se == ESP_OK) {
                    ESP_LOGI(TAG_WR, "ESPNOW batch sent (%u bytes, may be truncated).", (unsigned)len);
                    batch_store_clear();
                } else {
                    ESP_LOGW(TAG_WR, "ESPNOW send failed (%s); retaining batch.", esp_err_to_name(se));
                }
                free(json_payload);
            } else {
                ESP_LOGE(TAG_WR, "Failed to build batch JSON; retaining batch.");
            }
        } else {
            /* Fallback: old Wi-Fi/cloud path (optional). */
            ESP_LOGI(TAG_WR, "ESPNOW unavailable; falling back to Wi-Fi/cloud.");
            if (wifi_manager_connect_saved(15000)) {
                ensure_time();
                fetch_remote_config();
                bool ok = send_batched_data();
                if (ok) batch_store_clear();
            } else {
                ESP_LOGW(TAG_WR, "Wi-Fi connect failed; retaining batch.");
            }
        }
    } else {
        ESP_LOGI(TAG_WR, "Batch incomplete (%d/%d).",
                 batch_store_count(), device_readingsPerBatch);
    }

    simulate_or_sleep(device_wakeUpInterval);

    if (s_debug) {
        return worker_role_start();
    }
    return ESP_OK; // Deep sleep path never reaches here
}