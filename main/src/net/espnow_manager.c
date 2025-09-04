#include "net/espnow_manager.h"
#include "net/wifi_manager.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include <string.h>
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_mac.h"
#include "security/pmk.h"

static const char *TAG_EN = "ESPNOW_MGR"; // ensure single tag

static bool s_inited = false;
static bool s_is_controller = false;
static espnow_mgr_rx_cb_t s_rx = NULL;

/* NEW: fixed channel (0 = none) */
static uint8_t s_fixed_channel = 0;

#define CTRL_PREFIX "CTRL|"
#define DISCOVERY_PROBE "DISC?"
#define MAX_DISCOVERED 12

typedef struct {
    uint8_t mac[6];
    char name[33];
    int64_t last_seen_ms;
} discovered_ctrl_t;

static discovered_ctrl_t s_ctrls[MAX_DISCOVERED];
static int s_ctrl_count = 0;
static bool s_collect_mode = false;

/* --- Controller beacon task --- */
static TaskHandle_t s_beacon_task = NULL;
static char s_beacon_name[33] = "Controller";
static int s_beacon_interval_ms = 2000;

// NEW: prebuilt beacon payload to avoid per-loop snprintf stack usage
static uint8_t s_beacon_payload[64];
static size_t  s_beacon_payload_len = 0;
#define CTRL_BEACON_STACK   3072  // words (≈12 KB). Adjust if still low.

// Forward
static void update_beacon_payload_locked(void);
static void dump_hex(const uint8_t *d, int len, int max);

static void dump_hex(const uint8_t *d, int len, int max)
{
    if (!d || len <= 0) return;
    if (max > 64) max = 64;
    char line[3*64+1];
    int n = len < max ? len : max;
    int o = 0;
    for (int i=0;i<n && o < (int)sizeof(line)-3;i++) {
        o += snprintf(line+o, sizeof(line)-o, "%02X ", d[i]);
    }
    line[o]=0;
    ESP_LOGD(TAG_EN, "RX HEX (%d/%d): %s", n, len, line);
}

/* Single beacon send */
static void send_controller_beacon_once(bool log_always) {
    if (!s_inited || s_beacon_payload_len == 0) return;
    esp_err_t e = esp_now_send((uint8_t[6]){0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},
                               s_beacon_payload, s_beacon_payload_len);
    if (log_always || e != ESP_OK) {
        ESP_LOGI(TAG_EN, "Beacon send result=%s len=%d payload='%.*s'",
                 esp_err_to_name(e), (int)s_beacon_payload_len,
                 (int)s_beacon_payload_len, (char*)s_beacon_payload);
    }
}

// New helper: rebuild payload whenever name changes
static void update_beacon_payload_locked(void) {
    int n = snprintf((char*)s_beacon_payload, sizeof(s_beacon_payload),
                     CTRL_PREFIX "%s", s_beacon_name);
    if (n < 0) n = 0;
    if (n > (int)sizeof(s_beacon_payload)) n = sizeof(s_beacon_payload);
    s_beacon_payload_len = (size_t)n;
    ESP_LOGI(TAG_EN, "Beacon payload updated len=%d '%.*s'",
             (int)s_beacon_payload_len, (int)s_beacon_payload_len, s_beacon_payload);
}

/* --- Beacon task --- */
static void beacon_task(void *arg) {
    ESP_LOGI(TAG_EN, "ctrl_beacon task start (stack watermark=%u)",
             (unsigned)uxTaskGetStackHighWaterMark(NULL));
    uint32_t send_count = 0;
    for (;;) {
        send_controller_beacon_once(false);
        send_count++;
        if (send_count <= 5 || (send_count % 10) == 0) {
            ESP_LOGI(TAG_EN, "Beacon sent (%lu) interval=%dms watermark=%u",
                     (unsigned long)send_count, s_beacon_interval_ms,
                     (unsigned)uxTaskGetStackHighWaterMark(NULL));
        }
        vTaskDelay(pdMS_TO_TICKS(s_beacon_interval_ms));
    }
}

/* Parse controller beacons */
static void handle_possible_ctrl_beacon(const uint8_t *mac, const uint8_t *data, int len) {
    ESP_LOGD(TAG_EN, "Beacon candidate len=%d from %02X:%02X:%02X:%02X:%02X:%02X",
             len, mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    if (len < (int)strlen(CTRL_PREFIX) + 1) {
        ESP_LOGV(TAG_EN, "Beacon reject: too short");
        return;
    }
    if (memcmp(data, CTRL_PREFIX, strlen(CTRL_PREFIX)) != 0) {
        ESP_LOGV(TAG_EN, "Beacon reject: prefix mismatch");
        return;
    }
    const char *name = (const char*)data + strlen(CTRL_PREFIX);
    size_t name_len = len - strlen(CTRL_PREFIX);
    if (name_len > 32) name_len = 32;
    ESP_LOGI(TAG_EN, "Controller beacon: name='%.*s' MAC=%02X:%02X:%02X:%02X:%02X:%02X",
             (int)name_len, name, mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);

    int64_t now_ms = esp_timer_get_time()/1000;
    for (int i=0;i<s_ctrl_count;i++) {
        if (memcmp(s_ctrls[i].mac, mac, 6)==0) {
            memcpy(s_ctrls[i].name, name, name_len);
            s_ctrls[i].name[name_len]=0;
            s_ctrls[i].last_seen_ms = now_ms;
            return;
        }
    }
    if (s_ctrl_count < MAX_DISCOVERED) {
        memcpy(s_ctrls[s_ctrl_count].mac, mac, 6);
        memcpy(s_ctrls[s_ctrl_count].name, name, name_len);
        s_ctrls[s_ctrl_count].name[name_len]=0;
        s_ctrls[s_ctrl_count].last_seen_ms = now_ms;
        s_ctrl_count++;
        ESP_LOGV(TAG_EN, "Discovered controller #%d '%s'", s_ctrl_count, s_ctrls[s_ctrl_count-1].name);
    } else {
        ESP_LOGW(TAG_EN, "Discovery list full; ignoring new controller");
    }
}

static void espnow_recv_cb_wrapper(const uint8_t *mac, const uint8_t *data, int len) {
    if (!data || len <= 0) return;

    // Active probe handling: if worker asks, controller replies immediately
    if (s_is_controller && len == (int)strlen(DISCOVERY_PROBE) &&
        memcmp(data, DISCOVERY_PROBE, strlen(DISCOVERY_PROBE)) == 0) {
        ESP_LOGI(TAG_EN, "Received discovery probe from %02X:%02X:%02X:%02X:%02X:%02X -> sending immediate beacon",
                 mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
        send_controller_beacon_once(true);
    }

    if (s_collect_mode) handle_possible_ctrl_beacon(mac, data, len);
    if (s_rx) s_rx(mac, data, len); // user callback
}

static void espnow_recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5,0,0)
    const uint8_t *mac = info ? info->src_addr : NULL;
#else
    const uint8_t *mac = info ? info->peer_addr : NULL;
#endif
    if (!mac || !data || len<=0) return;
    ESP_LOGD(TAG_EN, "RX %d bytes", len);
    dump_hex(data, len, 32);
    espnow_recv_cb_wrapper(mac, data, len);
}

static esp_err_t ensure_wifi_for_espnow(void) {
    wifi_mode_t mode;
    esp_err_t r = esp_wifi_get_mode(&mode);
    if (r == ESP_OK) {
        // Driver already started (AP or APSTA). Ensure STA present.
        if (mode == WIFI_MODE_AP) {
            ESP_LOGI(TAG_EN, "Adjusting Wi-Fi mode AP -> APSTA for ESP-NOW.");
            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
        }
        return ESP_OK;
    }
    ESP_LOGI(TAG_EN, "Wi-Fi not started yet; initializing via wifi_manager.");
    return wifi_manager_init();
}

esp_err_t espnow_manager_start_controller_beacon(const char *name, int interval_ms) {
    if (!s_inited) return ESP_ERR_INVALID_STATE;
    if (interval_ms < 500) interval_ms = 500;
    if (name && name[0]) {
        strlcpy(s_beacon_name, name, sizeof(s_beacon_name));
    }
    s_beacon_interval_ms = interval_ms;
    update_beacon_payload_locked();  // rebuild payload before (re)starting task
    if (s_beacon_task) {
        ESP_LOGI(TAG_EN, "Controller beacon already running.");
        return ESP_OK;
    }
    if (xTaskCreate(beacon_task, "ctrl_beacon", CTRL_BEACON_STACK, NULL, 3, &s_beacon_task) != pdPASS) {
        ESP_LOGE(TAG_EN, "Failed to create ctrl_beacon task");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG_EN, "Controller beacon started (name='%s', interval=%d ms, stack=%d words)",
             s_beacon_name, s_beacon_interval_ms, CTRL_BEACON_STACK);
    return ESP_OK;
}

bool espnow_manager_is_initialized(void) {
    return s_inited;
}

esp_err_t espnow_manager_set_fixed_channel(uint8_t ch) {
    if (ch > 13) return ESP_ERR_INVALID_ARG;
    s_fixed_channel = ch;
    ESP_LOGI(TAG_EN, "ESPNOW fixed channel set to %u", s_fixed_channel);
    return ESP_OK;
}
uint8_t espnow_manager_get_fixed_channel(void) {
    return s_fixed_channel;
}

esp_err_t espnow_manager_init(bool controller, espnow_mgr_rx_cb_t rx_cb) {
    ESP_LOGI(TAG_EN, "espnow_manager_init called (controller=%d) current_state: inited=%d",
             controller, s_inited);
    if (s_inited) {
        if (controller && !s_is_controller) { s_is_controller = true; s_rx = rx_cb; }
        else if (rx_cb) s_rx = rx_cb;
        return ESP_OK;
    }

    esp_err_t err = ensure_wifi_for_espnow();
    if (err != ESP_OK) {
        ESP_LOGE(TAG_EN, "ensure_wifi_for_espnow failed: %s", esp_err_to_name(err));
        return err;
    }

    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);

    /* Force APSTA for controller so AP interface exists for broadcast */
    if (controller && mode == WIFI_MODE_STA) {
        wifi_config_t apcfg = {0};
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);   // now valid
        snprintf((char*)apcfg.ap.ssid, sizeof(apcfg.ap.ssid), "TMCTRL-%02X%02X%02X", mac[3],mac[4],mac[5]);
        apcfg.ap.ssid_len       = strlen((char*)apcfg.ap.ssid);
        /* Use fixed channel if configured, otherwise default to 1 */
        apcfg.ap.channel        = s_fixed_channel ? s_fixed_channel : 1;
        apcfg.ap.max_connection = 4;
        apcfg.ap.authmode       = WIFI_AUTH_OPEN;
        ESP_LOGI(TAG_EN, "Enabling APSTA (AP SSID=%s ch=%d) (fixed_channel=%u)",
                 apcfg.ap.ssid, apcfg.ap.channel, s_fixed_channel);
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &apcfg));
        mode = WIFI_MODE_APSTA;
    }

    uint8_t primary_ch = 0;
    wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
    esp_wifi_get_channel(&primary_ch, &second);
    ESP_LOGI(TAG_EN, "Wi-Fi mode=%d primary_channel=%u second_chan=%d before esp_now_init",
             mode, primary_ch, second);

    err = esp_now_init();
    if (err == ESP_ERR_ESPNOW_EXIST) err = ESP_OK;
    if (err != ESP_OK) {
        ESP_LOGE(TAG_EN, "esp_now_init failed: %s", esp_err_to_name(err));
        return err;
    }

    esp_now_set_pmk(espnow_pmk);

    // Always add STA broadcast peer
    esp_now_peer_info_t peer_sta = {0};
    memset(peer_sta.peer_addr, 0xFF, 6);
    peer_sta.ifidx = WIFI_IF_STA;
    peer_sta.channel = 0;
    peer_sta.encrypt = false;
    if (esp_now_add_peer(&peer_sta) == ESP_OK)
        ESP_LOGI(TAG_EN, "Added broadcast peer (STA)");

    // Add AP broadcast peer if APSTA
    if (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA) {
        esp_now_peer_info_t peer_ap = {0};
        memset(peer_ap.peer_addr, 0xFF, 6);
        peer_ap.ifidx = WIFI_IF_AP;
        peer_ap.channel = 0;
        peer_ap.encrypt = false;
        if (esp_now_add_peer(&peer_ap) == ESP_OK)
            ESP_LOGI(TAG_EN, "Added broadcast peer (AP)");
    }

    esp_now_register_recv_cb(espnow_recv_cb);

    if (controller) {
        s_is_controller = true;
        s_rx = rx_cb;
        ESP_LOGI(TAG_EN, "Controller ESPNOW ready (mode=%d ch=%u fixed=%u)", mode, primary_ch, s_fixed_channel);
    } else {
        ESP_LOGI(TAG_EN, "Worker ESPNOW ready (mode=%d ch=%u fixed=%u)", mode, primary_ch, s_fixed_channel);
    }

    s_inited = true;
    return ESP_OK;
}

esp_err_t espnow_manager_collect_controllers(int duration_ms, char **out_json) {
    ESP_LOGI(TAG_EN, "collect_controllers start duration=%dms inited=%d collect_mode=%d",
             duration_ms, s_inited, s_collect_mode);
    if (!out_json) return ESP_ERR_INVALID_ARG;
    *out_json = NULL;
    if (!s_inited) {
        ESP_LOGE(TAG_EN, "collect_controllers abort: ESPNOW not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if (s_collect_mode) {
        ESP_LOGW(TAG_EN, "collect_controllers already in progress");
        return ESP_ERR_INVALID_STATE;
    }
    if (duration_ms < 1000) duration_ms = 1000;
    if (duration_ms > 15000) duration_ms = 15000;

    s_ctrl_count = 0;
    memset(s_ctrls, 0, sizeof(s_ctrls));
    s_collect_mode = true;
    ESP_LOGI(TAG_EN, "Starting controller discovery window: %d ms", duration_ms);

    // ACTIVE DISCOVERY PROBE
    const char probe[] = DISCOVERY_PROBE;
    esp_err_t pe = espnow_manager_send_broadcast(probe, sizeof(probe)-1);
    ESP_LOGI(TAG_EN, "Sent discovery probe result=%s", esp_err_to_name(pe));

    int elapsed = 0;
    const int step = 100;
    int midProbeSent = 0;

    // If fixed channel set, prefer it and disable hopping
    const int hop_interval_ms = 400;
    int next_hop_ms = hop_interval_ms;
    int hop_index = 0;
    const uint8_t hop_channels[] = {1,6,11,3,9,2,4,5,7,8,10}; // fallback
    int busy_sets = 0;
    bool hopping_allowed = (s_fixed_channel == 0);

    if (s_fixed_channel) {
        esp_err_t e = esp_wifi_set_channel(s_fixed_channel, WIFI_SECOND_CHAN_NONE);
        if (e == ESP_OK) {
            ESP_LOGI(TAG_EN, "Discovery: set fixed channel %u for scan", s_fixed_channel);
        } else {
            ESP_LOGW(TAG_EN, "Discovery: failed to set fixed channel %u: %s", s_fixed_channel, esp_err_to_name(e));
            // allow hopping as fallback
            hopping_allowed = true;
        }
    }

    while (elapsed < duration_ms) {
        vTaskDelay(pdMS_TO_TICKS(step));
        elapsed += step;

        if (hopping_allowed && elapsed >= next_hop_ms && elapsed < duration_ms) {
            uint8_t ch = hop_channels[hop_index % (int)(sizeof(hop_channels))];
            esp_err_t ce = esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
            if (ce == ESP_OK) {
                ESP_LOGD(TAG_EN, "Discovery hop -> channel %u (elapsed=%d)", ch, elapsed);
                busy_sets = 0;
            } else {
                if (++busy_sets == 1) {
                    ESP_LOGW(TAG_EN, "Channel hop blocked (AP client or scan active) – staying on current channel.");
                } else if (busy_sets == 3) {
                    ESP_LOGW(TAG_EN, "Repeated channel set failures (3). Disabling hopping this scan.");
                    hopping_allowed = false;
                }
            }
            hop_index++;
            next_hop_ms += hop_interval_ms;
        }

        if (!midProbeSent && duration_ms >= 3000 && elapsed >= duration_ms/2) {
            espnow_manager_send_broadcast(probe, sizeof(probe)-1);
            ESP_LOGI(TAG_EN, "Mid-scan discovery probe sent at %d ms", elapsed);
            midProbeSent = 1;
        }
    }
    s_collect_mode = false;
    uint8_t ch=0; wifi_second_chan_t sc=WIFI_SECOND_CHAN_NONE;
    esp_wifi_get_channel(&ch,&sc);
    ESP_LOGI(TAG_EN, "Discovery finished on channel=%u found=%d", ch, s_ctrl_count);

    cJSON *arr = cJSON_CreateArray();
    if (!arr) return ESP_ERR_NO_MEM;
    for (int i=0;i<s_ctrl_count;i++) {
        cJSON *o = cJSON_CreateObject();
        char mac_str[18];
        snprintf(mac_str,sizeof(mac_str),"%02X:%02X:%02X:%02X:%02X:%02X",
                 s_ctrls[i].mac[0],s_ctrls[i].mac[1],s_ctrls[i].mac[2],
                 s_ctrls[i].mac[3],s_ctrls[i].mac[4],s_ctrls[i].mac[5]);
        cJSON_AddStringToObject(o,"mac",mac_str);
        cJSON_AddStringToObject(o,"name",s_ctrls[i].name[0]?s_ctrls[i].name:"Controller");
        cJSON_AddItemToArray(arr,o);
    }
    char *json = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);
    if (!json) return ESP_ERR_NO_MEM;
    *out_json = json;

    if (s_ctrl_count == 0) {
        ESP_LOGW(TAG_EN, "WARNING: No controllers discovered!");
    }

    ESP_LOGI(TAG_EN, "collect_controllers done found=%d", s_ctrl_count);
    return ESP_OK;
}

/* Replace old esp_now_register_recv_cb call site */
esp_err_t espnow_manager_send_broadcast(const void *data, size_t len) {
    if (!s_inited) return ESP_ERR_INVALID_STATE;
    if (!data || len == 0) return ESP_ERR_INVALID_ARG;
    if (len > 1000) {
        ESP_LOGW(TAG_EN, "Truncating payload %u -> 1000 bytes", (unsigned)len);
        len = 1000;
    }
    uint8_t bcast[6]; memset(bcast, 0xFF, 6);
    esp_err_t err = esp_now_send(bcast, (const uint8_t *)data, len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_EN, "esp_now_send failed: %s", esp_err_to_name(err));
    }
    return err;
}

// OPTIONAL helper to force channel (call from REST endpoint for debugging)
/* Force Wi-Fi channel for ESP-NOW (STA must be disconnected): */
esp_err_t espnow_manager_debug_set_channel(uint8_t ch) {
    if (ch < 1 || ch > 13) return ESP_ERR_INVALID_ARG;
    esp_err_t e = esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    ESP_LOGI(TAG_EN, "Force set channel=%u result=%s", ch, esp_err_to_name(e));
    return e;
}

void espnow_manager_set_controller_name(const char *name)
{
    if (name && name[0]) {
        strlcpy(s_beacon_name, name, sizeof(s_beacon_name));
    } else {
        strlcpy(s_beacon_name, "Controller", sizeof(s_beacon_name));
    }
    update_beacon_payload_locked();
    ESP_LOGI(TAG_EN, "Controller beacon name set to '%s' (task=%s)",
             s_beacon_name, s_beacon_task ? "running" : "stopped");
}

// Static storage for optional controller MAC (kept in RAM)
static uint8_t s_controller_mac[6];
static bool    s_have_controller_mac = false;

/* Store controller MAC in RAM (NULL clears). */
void espnow_manager_set_controller_mac(const uint8_t mac[6])
{
    if (!mac) {
        s_have_controller_mac = false;
        memset(s_controller_mac, 0, sizeof(s_controller_mac));
        ESP_LOGI(TAG_EN, "Controller MAC cleared");
        return;
    }
    memcpy(s_controller_mac, mac, sizeof(s_controller_mac));
    s_have_controller_mac = true;
    ESP_LOGI(TAG_EN, "Controller MAC set to %02X:%02X:%02X:%02X:%02X:%02X",
             s_controller_mac[0], s_controller_mac[1], s_controller_mac[2],
             s_controller_mac[3], s_controller_mac[4], s_controller_mac[5]);
}

/* Retrieve controller MAC if set. Returns true if MAC was copied. */
bool espnow_manager_get_controller_mac(uint8_t mac[6])
{
    if (!s_have_controller_mac) return false;
    if (mac) memcpy(mac, s_controller_mac, sizeof(s_controller_mac));
    return true;
}

/* Existing helpers set/get controller MAC (already declared in header).
 * Add unicast send that prefers controller MAC, falls back to broadcast.
 */
esp_err_t espnow_manager_send_to_controller(const void *data, size_t len) {
    if (!s_inited || !data || len == 0) return ESP_ERR_INVALID_STATE;
    if (len > 1000) {
        ESP_LOGW(TAG_EN, "Truncating payload %u -> 1000 bytes", (unsigned)len);
        len = 1000;
    }
    uint8_t mac[6];
    if (espnow_manager_get_controller_mac(mac)) {
        esp_now_peer_info_t peer = {0};
        memcpy(peer.peer_addr, mac, 6);
        peer.ifidx = WIFI_IF_STA;   // prefer STA
        peer.channel = 0;
        peer.encrypt = false;
        bool add_needed = true;
        // Cheap check: try to add (ignore EXIST)
        if (esp_now_add_peer(&peer) == ESP_ERR_ESPNOW_EXIST) add_needed = false;
        esp_err_t e = esp_now_send(mac, (const uint8_t*)data, len);
        if (e == ESP_OK) {
            ESP_LOGD(TAG_EN, "Unicast ESPNOW %uB to controller %02X:%02X:%02X:%02X:%02X:%02X",
                     (unsigned)len, mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
            return ESP_OK;
        }
        ESP_LOGW(TAG_EN, "Unicast send failed (%s); falling back to broadcast", esp_err_to_name(e));
    }
    return espnow_manager_send_broadcast(data, len);
}