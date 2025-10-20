#include "net/wifi_manager.h"
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "cJSON.h"
#include "esp_timer.h"

static const char *TAG = "WIFI_MGR";

static bool s_initialized = false;
static bool s_sta_created = false;
static EventGroupHandle_t s_evt_group;
#define BIT_CONNECTED BIT0
#define BIT_FAIL      BIT1

static void sta_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupSetBits(s_evt_group, BIT_FAIL);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_evt_group, BIT_CONNECTED);
    }
}

esp_err_t wifi_manager_init(void) {
    if (s_initialized) return ESP_OK;

    esp_err_t err;

    err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_netif_init failed: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "event_loop_create_default failed: %s", esp_err_to_name(err));
        return err;
    }

    if (!s_sta_created) {
        if (esp_netif_get_handle_from_ifkey("WIFI_STA_DEF")) {
            ESP_LOGI(TAG, "STA netif already exists (provisioning path); reusing.");
            s_sta_created = true;
        } else {
            esp_netif_create_default_wifi_sta();
            s_sta_created = true;
        }
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err == ESP_ERR_WIFI_INIT_STATE) {
        err = ESP_OK;
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(err));
        return err;
    }

    esp_wifi_set_mode(WIFI_MODE_STA);

    err = esp_wifi_start();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_wifi_start failed: %s", esp_err_to_name(err));
        return err;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Initialized.");
    return ESP_OK;
}

static bool load_saved(char *ssid, size_t ssid_len, char *pass, size_t pass_len) {
    nvs_handle_t nvs;
    if (nvs_open("wifi", NVS_READONLY, &nvs) != ESP_OK) return false;
    size_t ssz = ssid_len, psz = pass_len;
    esp_err_t err = nvs_get_str(nvs, "ssid", ssid, &ssz);
    if (err == ESP_OK) err = nvs_get_str(nvs, "pass", pass, &psz);
    nvs_close(nvs);
    return err == ESP_OK;
}

bool wifi_manager_is_connected(void) {
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) != ESP_OK) return false;
    esp_netif_t *sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!sta) return false;
    esp_netif_ip_info_t ip;
    if (esp_netif_get_ip_info(sta, &ip) != ESP_OK) return false;
    return ip.ip.addr != 0;
}

bool wifi_manager_get_ssid(char *out, size_t len) {
    if (!wifi_manager_is_connected()) return false;
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) != ESP_OK) return false;
    if (out && len) strlcpy(out, (const char*)ap.ssid, len);
    return true;
}

static bool connect_internal(const char *ssid, const char *pass, int timeout_ms) {
    if (wifi_manager_is_connected()) return true;

    wifi_config_t cfg = {0};
    strlcpy((char*)cfg.sta.ssid, ssid, sizeof(cfg.sta.ssid));
    strlcpy((char*)cfg.sta.password, pass ? pass : "", sizeof(cfg.sta.password));
    cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));

    s_evt_group = xEventGroupCreate();
    esp_event_handler_instance_t inst_any=NULL, inst_ip=NULL;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, sta_event_handler, NULL, &inst_any));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, sta_event_handler, NULL, &inst_ip));

    ESP_LOGI(TAG, "Connecting to SSID '%s'...", ssid);
    esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_connect failed: %s", esp_err_to_name(err));
    }

    EventBits_t bits = xEventGroupWaitBits(s_evt_group, BIT_CONNECTED | BIT_FAIL, pdFALSE, pdFALSE,
                                           pdMS_TO_TICKS(timeout_ms > 0 ? timeout_ms : 15000));

    esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, inst_ip);
    esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, inst_any);
    vEventGroupDelete(s_evt_group);
    s_evt_group = NULL;

    if (bits & BIT_CONNECTED) {
        ESP_LOGI(TAG, "Connected.");
        return true;
    }
    ESP_LOGW(TAG, "Connect timeout/fail.");
    return false;
}

bool wifi_manager_connect(const char *ssid, const char *pass, int timeout_ms) {
    if (wifi_manager_init() != ESP_OK) return false;
    return connect_internal(ssid, pass, timeout_ms);
}

bool wifi_manager_connect_saved(int timeout_ms) {
    char ssid[33]={0}, pass[65]={0};
    if (!load_saved(ssid, sizeof(ssid), pass, sizeof(pass))) {
        ESP_LOGE(TAG, "No saved Wi-Fi credentials.");
        return false;
    }
    return wifi_manager_connect(ssid, pass, timeout_ms);
}

void wifi_manager_disconnect(void) {
    if (!s_initialized) return;
    esp_wifi_disconnect();
    ESP_LOGI(TAG, "Disconnected.");
}

static char *s_cached_scan_json = NULL;
static uint32_t s_cache_ts_ms = 0;
static SemaphoreHandle_t s_scan_mux = NULL;

static void wifi_build_and_store_scan(void)
{
    // perform blocking scan (caller is the background task)
    wifi_scan_config_t scan_cfg = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time = { .active = { .min = 100, .max = 300 } },
    };
    if (esp_wifi_scan_start(&scan_cfg, true) != ESP_OK) {
        ESP_LOGW(TAG, "prefetch scan: esp_wifi_scan_start failed");
        return;
    }

    uint16_t ap_num = 0;
    esp_wifi_scan_get_ap_num(&ap_num);
    if (ap_num == 0) {
        ESP_LOGI(TAG, "prefetch scan: no APs found");
    }

    wifi_ap_record_t *ap_records = calloc(ap_num, sizeof(wifi_ap_record_t));
    if (!ap_records) return;
    esp_wifi_scan_get_ap_records(&ap_num, ap_records);

    cJSON *arr = cJSON_CreateArray();
    if (arr) {
        for (int i = 0; i < ap_num; ++i) {
            cJSON *ap = cJSON_CreateObject();
            if (!ap) continue;
            cJSON_AddStringToObject(ap, "ssid", (const char *)ap_records[i].ssid);
            cJSON_AddNumberToObject(ap, "rssi", ap_records[i].rssi);
            cJSON_AddNumberToObject(ap, "channel", ap_records[i].primary);
            cJSON_AddNumberToObject(ap, "auth", ap_records[i].authmode);
            cJSON_AddItemToArray(arr, ap);
        }
    }

    char *json = arr ? cJSON_PrintUnformatted(arr) : NULL;
    if (arr) cJSON_Delete(arr);
    free(ap_records);

    // store under mutex
    if (!s_scan_mux) s_scan_mux = xSemaphoreCreateMutex();
    if (s_scan_mux) {
        if (xSemaphoreTake(s_scan_mux, pdMS_TO_TICKS(2000)) == pdTRUE) {
            if (s_cached_scan_json) free(s_cached_scan_json);
            s_cached_scan_json = json; // may be NULL
            s_cache_ts_ms = esp_timer_get_time() / 1000; // ms
            xSemaphoreGive(s_scan_mux);
        } else {
            if (json) free(json);
            ESP_LOGW(TAG, "prefetch scan: failed to take scan mutex");
        }
    } else {
        // no mutex -> leak not ideal, but keep safe fallback
        if (s_cached_scan_json) free(s_cached_scan_json);
        s_cached_scan_json = json;
        s_cache_ts_ms = esp_timer_get_time() / 1000;
    }

    ESP_LOGI(TAG, "prefetch scan done, APs=%d", ap_num);
}

static void wifi_prefetch_task(void *pv)
{
    (void)pv;
    wifi_build_and_store_scan();
    vTaskDelete(NULL);
}

/* Start an asynchronous scan that will update the cached JSON.
 * If max_age_ms > 0 and the cache is fresh, this is a no-op.
 */
esp_err_t wifi_manager_prefetch_scan(int max_age_ms)
{
    uint32_t now = esp_timer_get_time() / 1000;
    if (!s_scan_mux) s_scan_mux = xSemaphoreCreateMutex();
    if (s_scan_mux && xSemaphoreTake(s_scan_mux, pdMS_TO_TICKS(2000)) == pdTRUE) {
        uint32_t age = (s_cache_ts_ms == 0) ? UINT32_MAX : (now - s_cache_ts_ms);
        xSemaphoreGive(s_scan_mux);
        if (max_age_ms > 0 && age < (uint32_t)max_age_ms) {
            // cache fresh enough
            ESP_LOGD(TAG, "prefetch: cache fresh (age=%u ms) skip", age);
            return ESP_OK;
        }
    }

    // spawn background task to do the blocking scan and build JSON
    if (xTaskCreate(wifi_prefetch_task, "wifi_prefetch", 4096, NULL, 3, NULL) != pdPASS) {
        ESP_LOGW(TAG, "prefetch: failed to create task");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

/* Return a malloc'd JSON string with cached scan results. Caller must free().
 * Returns ESP_OK on success, ESP_ERR_NOT_FOUND if no cache present.
 */
esp_err_t wifi_manager_get_cached_scan(char **out_json)
{
    if (!out_json) return ESP_ERR_INVALID_ARG;
    *out_json = NULL;
    if (!s_scan_mux) s_scan_mux = xSemaphoreCreateMutex();
    if (s_scan_mux && xSemaphoreTake(s_scan_mux, pdMS_TO_TICKS(2000)) == pdTRUE) {
        if (s_cached_scan_json) {
            *out_json = strdup(s_cached_scan_json);
            xSemaphoreGive(s_scan_mux);
            return (*out_json) ? ESP_OK : ESP_ERR_NO_MEM;
        }
        xSemaphoreGive(s_scan_mux);
    } else {
        // fallthrough
    }
    return ESP_ERR_NOT_FOUND;
}