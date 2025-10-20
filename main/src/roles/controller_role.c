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
#include "esp_now.h"
#include "indicator/led_indicator.h"
#include "nvs.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "cJSON.h"

#define CONTROLLER_MAIN_STACK   8192   // Increase from 4096 (TLS & JSON need >6K)
#define CONTROLLER_MAIN_PRIO    5

#define TIME_SYNC_PREFIX "TIME:" // For sending time back to worker
#define ACK_PREFIX "ACK:"       // For sending ACK with config back to worker

static const char *TAG_CR = "CTRL_ROLE";
static bool s_debug = false;
static bool s_controller_sensor_disabled = true;

void controller_role_set_debug(bool enabled) {
    s_debug = enabled;
}

/* Reuse RTC vars defined in sensornest.c */
extern RTC_DATA_ATTR bool time_is_set;
extern RTC_DATA_ATTR bool device_config_initialized;
extern RTC_DATA_ATTR int  device_readingsPerBatch;
extern RTC_DATA_ATTR int  device_wakeUpInterval;

/* Reuse helper functions still in sensornest.c */
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

// Acumula 6 lotes de workers para envio conjunto
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

    // Start with a fresh root object: {"data": []}
    cJSON *root = cJSON_CreateObject();
    if (!root) return local_json;
    cJSON *main_arr = cJSON_CreateArray();
    if (!main_arr) {
        cJSON_Delete(root);
        return local_json;
    }
    cJSON_AddItemToObject(root, "data", main_arr);

    // Add local readings first, if any
    if (local_json) {
        cJSON *local_root = cJSON_Parse(local_json);
        if (local_root) {
            cJSON *local_data = cJSON_GetObjectItem(local_root, "data");
            if (cJSON_IsArray(local_data)) {
                cJSON *reading = NULL;
                while ((reading = cJSON_DetachItemFromArray(local_data, 0)) != NULL) {
                    cJSON_AddItemToArray(main_arr, reading);
                }
            }
            cJSON_Delete(local_root);
        }
        free(local_json); // free the original local json string
    }

    // Add worker readings
    for (int i=0;i<s_worker_batch_count;i++) {
        cJSON *wr = cJSON_Parse(s_worker_batches[i]);
        if (!wr) continue;
        cJSON *arr = cJSON_GetObjectItem(wr,"data");
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

    free_worker_batches(); // Clear worker batches after successful merge
    return merged;
}

/* Periodic controller loop (always-on, no deep sleep) */
static void controller_main_task(void *arg) {
    ESP_LOGI(TAG_CR, "Controller loop started (interval=%d s, batch=%d, debug=%d, stack=%d)",
             device_wakeUpInterval, device_readingsPerBatch, s_debug, CONTROLLER_MAIN_STACK);
    for (;;) {
        // Check for and process worker batches frequently
        if (s_worker_batch_count > 0) {
            ESP_LOGI(TAG_CR, "Preparing combined batch (local=%d worker_batches=%d)", batch_store_count(), s_worker_batch_count);
            if (wifi_manager_connect_saved(15000)) {
                ensure_time();
                fetch_remote_config();
                char *local_json = NULL;
                batch_store_build_json(&local_json); // can be NULL if no local readings
                char *merged_json = merge_local_and_worker_batches(local_json);
                if (merged_json) {
                    if (rc_post_readings(merged_json) == ESP_OK) {
                        ESP_LOGI(TAG_CR, "Combined batch posted successfully.");
                        batch_store_clear(); // Also clears local readings that were merged
                    } else {
                        ESP_LOGW(TAG_CR, "Combined batch post failed; will retry.");
                    }
                    free(merged_json);
                }
            } else {
                ESP_LOGW(TAG_CR, "Wi-Fi connect failed; will retry next interval.");
            }
        } else if (!s_controller_sensor_disabled && batch_store_count() >= device_readingsPerBatch) {
            // Process local-only batch if it's full
            ESP_LOGI(TAG_CR, "Preparing local-only batch (count=%d)", batch_store_count());
            if (wifi_manager_connect_saved(15000)) {
                ensure_time();
                fetch_remote_config();
                char *local_json = NULL;
                if (batch_store_build_json(&local_json) == ESP_OK && local_json) {
                    if (rc_post_readings(local_json) == ESP_OK) {
                        ESP_LOGI(TAG_CR, "Local batch posted successfully.");
                        batch_store_clear();
                    } else {
                        ESP_LOGW(TAG_CR, "Local batch post failed; retaining.");
                    }
                    free(local_json);
                }
            }
        }

        // Sensor reading logic remains unchanged
        if (!s_controller_sensor_disabled && !batch_store_is_full()) {
            sensor_reading_t r;
            if (sensor_read_once(s_debug, &r) == ESP_OK && batch_store_add(&r) == ESP_OK) {
                ESP_LOGI(TAG_CR, "Stored local reading #%d (T=%.1f H=%.1f)", batch_store_count(), r.temperature, r.humidity);
            }
        }

        int wait_s = device_wakeUpInterval > 0 ? device_wakeUpInterval : 10;
        vTaskDelay(pdMS_TO_TICKS(wait_s * 1000));
    }
}

static void controller_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGD(TAG_CR, "ACK sent successfully to " MACSTR, MAC2STR(mac_addr));
    } else {
        ESP_LOGW(TAG_CR, "ACK send failed to " MACSTR, MAC2STR(mac_addr));
    }
}

static void controller_recv_cb(const espnow_mgr_event_recv_cb_t *recv_info) {
    ESP_LOGI(TAG_CR, "EVENT: ESPNOW RX %dB from " MACSTR " on if %d",
             recv_info->data_len, MAC2STR(recv_info->mac_addr), recv_info->ifidx);

    // Stash the batch. The main loop will handle posting.
    stash_worker_batch((const char*)recv_info->data, recv_info->data_len);

    if (time_is_set) {
        time_t now;
        time(&now);
        char payload[128];
        int payload_len = snprintf(payload, sizeof(payload), ACK_PREFIX "%lld:%d:%d",
                                   (long long)now, device_readingsPerBatch, device_wakeUpInterval);
        if (payload_len > 0) {
            esp_err_t err = espnow_manager_send_to_mac(recv_info->mac_addr, (const uint8_t*)payload, payload_len);
            if (err == ESP_OK) {
                ESP_LOGI(TAG_CR, "ACK sent successfully to " MACSTR, MAC2STR(recv_info->mac_addr));
                // Flash green to indicate successful communication handshake
                if (s_debug) {
                    led_indicator_flash_success();
                }
            } else {
                ESP_LOGW(TAG_CR, "Failed to queue ACK for sending: %s", esp_err_to_name(err));
            }
        }
    }
}

esp_err_t controller_role_start(void) {
    ESP_LOGI(TAG_CR, "Controller role starting (debug=%d).", s_debug);

    // Init ESPNOW manager and register our callbacks
    espnow_manager_init();
    espnow_manager_register_callbacks(controller_send_cb, controller_recv_cb);
    espnow_manager_config_role(true);

    // Now connect to Wi-Fi. The driver will use the primary channel set by ESPNOW init.
    if (wifi_manager_connect_saved(15000)) {
        ensure_time();
        fetch_remote_config();
    } else {
        ESP_LOGW(TAG_CR, "Initial Wi-Fi connect failed. Time/config will be fetched later.");
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
        uint8_t sensor_disabled = 0;
        if (nvs_get_u8(nvs, "ctrl_sensor_disabled", &sensor_disabled) == ESP_OK) {
            s_controller_sensor_disabled = (sensor_disabled == 1);
        }
        nvs_close(nvs);
    }
    espnow_manager_start_controller_beacon(ctrl_name, 1000); // shorter interval for better discovery while testing
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);

    ESP_LOGI(TAG_CR, "Controller beacon activated: name='%s' interval=%dms, MAC='" MACSTR "'",
            ctrl_name, 1000, MAC2STR(mac));
    uint8_t ch=espnow_manager_get_fixed_channel(); wifi_second_chan_t sc=WIFI_SECOND_CHAN_NONE;
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