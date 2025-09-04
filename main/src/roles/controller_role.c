#include "roles/controller_role.h"
#include "esp_log.h"
#include "roles/role_manager.h"
#include "sensor/sensor.h"
#include "storage/batch_store.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sntp.h"
#include "net/rest_client.h"
#include "net/wifi_manager.h"
#include "net/espnow_manager.h"
#include "indicator/led_indicator.h"
#include "nvs.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "cJSON.h"

#define CONTROLLER_MAIN_STACK   8192   // Increase from 4096 (TLS & JSON need >6K)
#define CONTROLLER_MAIN_PRIO    5

static const char *TAG_CR = "CTRL_ROLE";
static bool s_debug = false;

/* Reuse RTC vars defined in tempmesh.c */
extern RTC_DATA_ATTR bool time_is_set;
extern RTC_DATA_ATTR bool device_config_initialized;
extern RTC_DATA_ATTR int  device_readingsPerBatch;
extern RTC_DATA_ATTR int  device_wakeUpInterval;

/* Reuse helper functions still in tempmesh.c */
extern bool send_batched_data(void);

/* Local helpers (duplicated from worker for now; later unify) */
static void ensure_time(void) {
    if (time_is_set) return;
    ESP_LOGI(TAG_CR, "Syncing SNTP time (controller)...");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
    int retry = 0;
    while (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < 15) {
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    if (retry < 15) {
        time_is_set = true;
        ESP_LOGI(TAG_CR, "Time synced.");
    } else {
        ESP_LOGW(TAG_CR, "Time sync failed.");
    }
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
            if (batch > 0)    device_readingsPerBatch = batch;
            if (interval > 0) device_wakeUpInterval   = interval;
            device_config_initialized = true;
            ESP_LOGI(TAG_CR, "Applied remote config: readingsPerBatch=%d wakeUpInterval=%d",
                     device_readingsPerBatch, device_wakeUpInterval);
        }
    }
}

/* NEW: accumulate worker batches (raw JSON) for merge */
#define MAX_WORKER_BATCHES 6
static char *s_worker_batches[MAX_WORKER_BATCHES];
static int   s_worker_batch_count = 0;

static void free_worker_batches(void) {
    for (int i=0;i<s_worker_batch_count;i++) {
        free(s_worker_batches[i]);
        s_worker_batches[i]=NULL;
    }
    s_worker_batch_count=0;
}

static void stash_worker_batch(const char *json, int len) {
    if (!json || len<=0) return;
    if (s_worker_batch_count == MAX_WORKER_BATCHES) {
        free(s_worker_batches[0]);
        memmove(&s_worker_batches[0], &s_worker_batches[1],
                sizeof(char*)*(MAX_WORKER_BATCHES-1));
        s_worker_batch_count--;
    }
    s_worker_batches[s_worker_batch_count++] = strndup(json, len);
    ESP_LOGI(TAG_CR, "Stashed worker batch (%d stored).", s_worker_batch_count);
}

static char *merge_local_and_worker_batches(char *local_json) {
    if (s_worker_batch_count == 0) return local_json;
    // Expect format: {"readings":[ ... ]}
    cJSON *root = cJSON_Parse(local_json ? local_json : "{\"readings\":[]}");
    if (!root) return local_json;
    cJSON *main_arr = cJSON_GetObjectItem(root,"readings");
    if (!cJSON_IsArray(main_arr)) {
        cJSON_Delete(root);
        return local_json;
    }
    for (int i=0;i<s_worker_batch_count;i++) {
        cJSON *wr = cJSON_Parse(s_worker_batches[i]);
        if (!wr) continue;
        cJSON *arr = cJSON_GetObjectItem(wr,"readings");
        if (cJSON_IsArray(arr)) {
            int sz = cJSON_GetArraySize(arr);
            for (int j=0;j<sz;j++) {
                cJSON *itm = cJSON_DetachItemFromArray(arr, 0);
                if (itm) cJSON_AddItemToArray(main_arr, itm);
            }
        }
        cJSON_Delete(wr);
    }
    char *merged = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (merged) {
        free(local_json);
        free_worker_batches();
        return merged;
    } else {
        ESP_LOGW(TAG_CR, "Failed to merge local and worker batches.");
        // keep original local_json if merge failed
        free_worker_batches();
        return local_json;
    }
}

/* Periodic controller loop (always-on, no deep sleep) */
static void controller_main_task(void *arg) {
    ESP_LOGI(TAG_CR, "Controller loop started (interval=%d s, batch=%d, debug=%d, stack=%d)",
             device_wakeUpInterval, device_readingsPerBatch, s_debug, CONTROLLER_MAIN_STACK);
    for (;;) {
        if (!batch_store_is_full()) {
            sensor_reading_t r;
            if (sensor_read_once(s_debug, &r) == ESP_OK &&
                batch_store_add(&r) == ESP_OK) {
                ESP_LOGI(TAG_CR, "Stored reading #%d (T=%.1f H=%.1f)",
                         batch_store_count(), r.temperature, r.humidity);
            }
        }

        if (batch_store_count() >= device_readingsPerBatch ||
            (s_worker_batch_count > 0 && batch_store_count() > 0)) {
            ESP_LOGI(TAG_CR, "Preparing combined batch (local=%d worker_batches=%d)",
                     batch_store_count(), s_worker_batch_count);
            if (wifi_manager_connect_saved(15000)) {
                ensure_time();
                fetch_remote_config();
                char *local_json = NULL;
                if (batch_store_build_json(&local_json) == ESP_OK && local_json) {
                    char *merged = merge_local_and_worker_batches(local_json);
                    if (rc_post_readings(merged) == ESP_OK) {
                        ESP_LOGI(TAG_CR, "Combined batch posted successfully.");
                        batch_store_clear();
                    } else {
                        ESP_LOGW(TAG_CR, "Combined batch post failed; retaining local batch.");
                    }
                    free(merged);
                }
            } else {
                ESP_LOGW(TAG_CR, "Wi-Fi connect failed; will retry next interval.");
            }
        } else {
            ESP_LOGD(TAG_CR, "Batch incomplete (%d/%d) worker_batches=%d",
                     batch_store_count(), device_readingsPerBatch, s_worker_batch_count);
        }

        int wait_s = device_wakeUpInterval > 0 ? device_wakeUpInterval : 10;
        vTaskDelay(pdMS_TO_TICKS(wait_s * 1000));
    }
}

static void controller_rx_espnow_cb(const uint8_t *mac, const uint8_t *data, int len) {
    if (!data || len <= 0) return;
    int n = len < 120 ? len : 120;
    char snippet[121]; memcpy(snippet,data,n); snippet[n]=0;
    ESP_LOGI(TAG_CR, "ESPNOW RX %dB from %02X:%02X:%02X:%02X:%02X:%02X '%s'%s",
             len, mac[0],mac[1],mac[2],mac[3],mac[4],mac[5],
             snippet, (len>n?"...":""));
    // Assume payload is JSON batch {"readings":[...]} – stash for merge
    if (len < 10) return;
    stash_worker_batch((const char*)data, len);
}

void controller_role_set_debug(bool enabled) { s_debug = enabled; }

esp_err_t controller_role_start(void) {
    ESP_LOGI(TAG_CR, "Controller role starting (debug=%d).", s_debug);

    // Init ESPNOW listener early
    esp_err_t en = espnow_manager_init(true, controller_rx_espnow_cb);
    if (en != ESP_OK) {
        ESP_LOGW(TAG_CR, "ESPNOW init failed: %s (continuing without worker ingestion)", esp_err_to_name(en));
    }

    /* Start pulsing LED if debug mode */
    if (s_debug) {
        if (led_indicator_init() == ESP_OK) {
            led_indicator_controller_pulse_start();
        }
    } else {
        led_indicator_controller_pulse_stop();
    }

    // Load controller name (optional)
    char ctrl_name[33]="Controller";
    nvs_handle_t nvs;
    if (nvs_open("device", NVS_READONLY, &nvs)==ESP_OK) {
        size_t l=sizeof(ctrl_name);
        nvs_get_str(nvs,"name",ctrl_name,&l);
        nvs_close(nvs);
    }
    espnow_manager_start_controller_beacon(ctrl_name, 1000); // shorter interval for better discovery while testing
    ESP_LOGI(TAG_CR, "Controller beacon activated name='%s' interval=%dms",
             ctrl_name, 1000);
    uint8_t ch=0; wifi_second_chan_t sc=WIFI_SECOND_CHAN_NONE;
    esp_wifi_get_channel(&ch,&sc);
    ESP_LOGI(TAG_CR, "Beacon active on channel=%u", ch);

    if (xTaskCreate(controller_main_task,
                    "controller_main",
                    CONTROLLER_MAIN_STACK,
                    NULL,
                    CONTROLLER_MAIN_PRIO,
                    NULL) != pdPASS) {
        ESP_LOGE(TAG_CR, "Failed to create controller_main task.");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}