#include "net/espnow_manager.h"
#include "net/wifi_manager.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_now.h"
#include <string.h>
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include "esp_mac.h"
#include "security/pmk.h"

#define ESPNOW_QUEUE_SIZE   16
#define ESPNOW_MAX_DELAY    pdMS_TO_TICKS(500)
#define ESPNOW_TASK_STACK   4096
#define ESPNOW_TASK_PRIO    4

static const char *TAG_EN = "ESPNOW_MGR";

static QueueHandle_t s_espnow_queue = NULL;
static TaskHandle_t s_espnow_task_handle = NULL;

static espnow_mgr_send_cb_t s_send_cb = NULL;
static espnow_mgr_rx_cb_t s_recv_cb = NULL;

static bool s_inited = false;
static bool s_is_controller = false;

/* NEW: fixed channel (0 = none) */
static uint8_t s_fixed_channel = 0;

#define CTRL_PREFIX "CTRL|"
#define DISCOVERY_PROBE "DISC?"
#define MAX_DISCOVERED 12

typedef struct {
    uint8_t mac[6];
    char name[33];
    uint8_t channel; // NEW: store channel
    int64_t last_seen_ms;
} discovered_ctrl_t;

static discovered_ctrl_t s_ctrls[MAX_DISCOVERED];
static int s_ctrl_count = 0;
static bool s_collect_mode = false;

/* --- Controller beacon task --- */
static TaskHandle_t s_beacon_task = NULL;
static char s_beacon_name[33] = "Controller";
static int s_beacon_interval_ms = 2000;

// Forward
static void update_beacon_payload_locked(void);
static void dump_hex(const uint8_t *d, int len, int max);
// Correct the signature of the internal send callback forward declaration
static void internal_send_cb(const esp_now_send_info_t *info, esp_now_send_status_t status);
static void internal_recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int len);
static void espnow_manager_task(void *pvParameter);
static void handle_possible_ctrl_beacon(const uint8_t *mac, const uint8_t *data, int len);

// NEW: prebuilt beacon payload to avoid per-loop snprintf stack usage
static uint8_t s_beacon_payload[64];
static size_t  s_beacon_payload_len = 0;
#define CTRL_BEACON_STACK   3072  // words (~12 KB). Adjust if still low.

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

    // Explicitly use the AP interface for beacons to control the channel.
    // The STA interface channel will be dictated by the connected router.
    esp_now_peer_info_t peer = {
        .ifidx = WIFI_IF_AP,
        .channel = 0, // Use the current channel of the specified interface (AP)
        .encrypt = false
    };
    memcpy(peer.peer_addr, (uint8_t[6]){0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}, 6);

    // Add/modify peer to ensure it's on the correct interface and channel
    if (!esp_now_is_peer_exist(peer.peer_addr)) {
        esp_now_add_peer(&peer);
    } else {
        esp_now_mod_peer(&peer);
    }

    esp_err_t e = esp_now_send(peer.peer_addr, s_beacon_payload, s_beacon_payload_len);
    if (log_always || e != ESP_OK) {
        ESP_LOGI(TAG_EN, "Beacon send result=%s len=%d payload='%.*s'",
                 esp_err_to_name(e), (int)s_beacon_payload_len,
                 (int)s_beacon_payload_len, (char*)s_beacon_payload);
    }
}

// New helper: rebuild payload whenever name changes
static void update_beacon_payload_locked(void) {
    uint8_t ch = 0;
    wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
    esp_wifi_get_channel(&ch, &second);
    if (ch == 0) ch = s_fixed_channel ? s_fixed_channel : 1; // Fallback

    int n = snprintf((char*)s_beacon_payload, sizeof(s_beacon_payload),
                     CTRL_PREFIX "%u|%s", ch, s_beacon_name);
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

static void internal_send_cb(const esp_now_send_info_t *info, esp_now_send_status_t status) {
    if (s_espnow_queue == NULL || info == NULL) return;
    espnow_mgr_event_t evt;
    evt.id = ESPNOW_MGR_SEND_CB;
    memcpy(evt.info.send_cb.mac_addr, info->des_addr, ESP_NOW_ETH_ALEN);
    evt.info.send_cb.status = status;
    if (xQueueSend(s_espnow_queue, &evt, ESPNOW_MAX_DELAY) != pdTRUE) {
        ESP_LOGW(TAG_EN, "Send CB queue full");
    }
}

static void internal_recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (s_espnow_queue == NULL || info == NULL || data == NULL || len <= 0) {
        return;
    }

    espnow_mgr_event_t evt;
    evt.id = ESPNOW_MGR_RECV_CB;
    espnow_mgr_event_recv_cb_t *recv_cb_info = &evt.info.recv_cb;
    memcpy(recv_cb_info->mac_addr, info->src_addr, ESP_NOW_ETH_ALEN);
    // recv_cb_info->ifidx = info->rx_ctrl->ifidx; // comentado por enquanto, ver mais tarde
    recv_cb_info->data = malloc(len);
    if (recv_cb_info->data == NULL) {
        ESP_LOGE(TAG_EN, "Failed to malloc for recv_cb");
        return;
    }
    memcpy(recv_cb_info->data, data, len);
    recv_cb_info->data_len = len;

    if (xQueueSend(s_espnow_queue, &evt, ESPNOW_MAX_DELAY) != pdTRUE) {
        ESP_LOGW(TAG_EN, "Recv CB queue full");
        free(recv_cb_info->data);
    }
}

/* Parse controller beacons */
static void handle_possible_ctrl_beacon(const uint8_t *mac, const uint8_t *data, int len) {
    ESP_LOGD(TAG_EN, "Beacon candidate len=%d from %02X:%02X:%02X:%02X:%02X:%02X",
             len, mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    if (len < (int)strlen(CTRL_PREFIX) + 3) { // e.g. CTRL|1|A
        ESP_LOGV(TAG_EN, "Beacon reject: too short");
        return;
    }
    if (memcmp(data, CTRL_PREFIX, strlen(CTRL_PREFIX)) != 0) {
        ESP_LOGV(TAG_EN, "Beacon reject: prefix mismatch");
        return;
    }

    // New format: CTRL|<channel>|<name>
    const char *payload = (const char*)data + strlen(CTRL_PREFIX);
    char *pipe = strchr(payload, '|');
    if (!pipe) {
        ESP_LOGW(TAG_EN, "Invalid beacon format (no pipe separator)");
        return;
    }

    int channel = atoi(payload);
    const char *name = pipe + 1;
    size_t name_len = len - (name - (const char*)data);

    if (channel <= 0 || channel > 13) {
        ESP_LOGW(TAG_EN, "Invalid channel '%d' in beacon", channel);
        return;
    }

    if (name_len > 32) name_len = 32;
    ESP_LOGI(TAG_EN, "Discovered controller beacon: name='%.*s' MAC=%02X:%02X:%02X:%02X:%02X:%02X ch=%d",
             (int)name_len, name, mac[0],mac[1],mac[2],mac[3],mac[4],mac[5], channel);

    int64_t now_ms = esp_timer_get_time()/1000;
    for (int i=0;i<s_ctrl_count;i++) {
        if (memcmp(s_ctrls[i].mac, mac, 6)==0) {
            memcpy(s_ctrls[i].name, name, name_len);
            s_ctrls[i].name[name_len]=0;
            s_ctrls[i].channel = channel;
            s_ctrls[i].last_seen_ms = now_ms;
            return;
        }
    }
    if (s_ctrl_count < MAX_DISCOVERED) {
        memcpy(s_ctrls[s_ctrl_count].mac, mac, 6);
        memcpy(s_ctrls[s_ctrl_count].name, name, name_len);
        s_ctrls[s_ctrl_count].name[name_len]=0;
        s_ctrls[s_ctrl_count].channel = channel;
        s_ctrls[s_ctrl_count].last_seen_ms = now_ms;
        s_ctrl_count++;
        ESP_LOGV(TAG_EN, "Discovered controller #%d '%s'", s_ctrl_count, s_ctrls[s_ctrl_count-1].name);
    } else {
        ESP_LOGW(TAG_EN, "Discovery list full; ignoring new controller");
    }
}

// static void espnow_recv_cb_wrapper(const uint8_t *mac, const uint8_t *data, int len, wifi_interface_t ifidx) {
//     if (!mac || !data || len <= 0) return;

//     if (s_recv_cb) {
//         s_recv_cb(mac, data, len, ifidx);
//     }

//     if (s_collect_mode) {
//         handle_possible_ctrl_beacon(mac, data, len);
//     }
// }

// static void espnow_recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
//     const uint8_t *mac = info ? info->src_addr : NULL;
//     if (!mac) return;

//     // Correct way to get the interface: look up the peer info.
//     esp_now_peer_info_t peer_info;
//     wifi_interface_t ifidx = WIFI_IF_STA; // Default to STA
//     if (esp_now_get_peer(mac, &peer_info) == ESP_OK) {
//         ifidx = peer_info.ifidx;
//     }

//     espnow_recv_cb_wrapper(mac, data, len, ifidx);
// }

/* Add unicast send that prefers controller MAC, falls back to broadcast.
 * Existing helpers set/get controller MAC (already declared in header).
 */
esp_err_t espnow_manager_send_to_controller(const void *data, size_t len) {
    if (!s_inited || !data || len == 0) return ESP_ERR_INVALID_STATE;
    if (len > 1000) {
        ESP_LOGW(TAG_EN, "Truncating payload %u -> 1000 bytes", (unsigned)len);
        len = 1000;
    }
    uint8_t mac[6];
    if (espnow_manager_get_controller_mac(mac)) {
        // Check if peer exists, add if not, to prevent warnings
        if (!esp_now_is_peer_exist(mac)) {
            esp_now_peer_info_t peer = {0};
            memcpy(peer.peer_addr, mac, 6);
            peer.ifidx = WIFI_IF_STA;
            peer.channel = s_fixed_channel; // Use the stored channel
            peer.encrypt = false;
            esp_err_t add_err = esp_now_add_peer(&peer);
            if (add_err != ESP_OK) {
                ESP_LOGE(TAG_EN, "Failed to add unicast peer: %s", esp_err_to_name(add_err));
            }
        }
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

esp_err_t espnow_manager_collect_controllers(int duration_ms, char **out_json) {
    if (!s_inited || !out_json) return ESP_ERR_INVALID_ARG;

    s_ctrl_count = 0;
    memset(s_ctrls, 0, sizeof(s_ctrls));
    s_collect_mode = true;

    ESP_LOGI(TAG_EN, "Starting controller discovery for %d ms...", duration_ms);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    s_collect_mode = false;
    ESP_LOGI(TAG_EN, "Controller discovery finished. Found %d controllers.", s_ctrl_count);

    cJSON *root = cJSON_CreateArray();
    if (!root) return ESP_ERR_NO_MEM;

    for (int i = 0; i < s_ctrl_count; i++) {
        cJSON *ctrl = cJSON_CreateObject();
        char mac_str[18];
        snprintf(mac_str, sizeof(mac_str), MACSTR, MAC2STR(s_ctrls[i].mac));
        cJSON_AddStringToObject(ctrl, "mac", mac_str);
        cJSON_AddStringToObject(ctrl, "name", s_ctrls[i].name);
        cJSON_AddNumberToObject(ctrl, "channel", s_ctrls[i].channel);
        cJSON_AddItemToArray(root, ctrl);
    }

    *out_json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    return *out_json ? ESP_OK : ESP_ERR_NO_MEM;
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

esp_err_t espnow_manager_init(void) {
    if (s_inited) return ESP_OK;

    s_espnow_queue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(espnow_mgr_event_t));
    if (s_espnow_queue == NULL) {
        ESP_LOGE(TAG_EN, "Failed to create ESP-NOW event queue");
        return ESP_ERR_NO_MEM;
    }

    ESP_ERROR_CHECK(wifi_manager_init());

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(internal_send_cb));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(internal_recv_cb));
    ESP_ERROR_CHECK(esp_now_set_pmk(espnow_pmk));

    if (xTaskCreate(espnow_manager_task, "espnow_mgr_task", ESPNOW_TASK_STACK, NULL, ESPNOW_TASK_PRIO, &s_espnow_task_handle) != pdPASS) {
        ESP_LOGE(TAG_EN, "Failed to create ESP-NOW task");
        vQueueDelete(s_espnow_queue);
        s_espnow_queue = NULL;
        esp_now_deinit();
        return ESP_ERR_NO_MEM;
    }

    s_inited = true;
    ESP_LOGI(TAG_EN, "ESP-NOW Manager initialized successfully.");
    return ESP_OK;
}

void espnow_manager_deinit(void) {
    if (!s_inited) return;
    if (s_espnow_task_handle) {
        vTaskDelete(s_espnow_task_handle);
        s_espnow_task_handle = NULL;
    }
    if (s_espnow_queue) {
        vQueueDelete(s_espnow_queue);
        s_espnow_queue = NULL;
    }
    esp_now_deinit();
    s_inited = false;
    ESP_LOGI(TAG_EN, "ESP-NOW Manager de-initialized.");
}

void espnow_manager_register_callbacks(espnow_mgr_send_cb_t send_cb, espnow_mgr_rx_cb_t recv_cb) {
    s_send_cb = send_cb;
    s_recv_cb = recv_cb;
}

esp_err_t espnow_manager_config_role(bool is_controller) {
    s_is_controller = is_controller;
    wifi_mode_t mode;
    ESP_ERROR_CHECK(esp_wifi_get_mode(&mode));

    if (is_controller && mode == WIFI_MODE_STA) {
        // ... existing logic to set APSTA mode ...
        wifi_config_t apcfg = {0};
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
        snprintf((char*)apcfg.ap.ssid, sizeof(apcfg.ap.ssid), "TMCTRL-%02X%02X%02X", mac[3],mac[4],mac[5]);
        apcfg.ap.ssid_len       = strlen((char*)apcfg.ap.ssid);
        apcfg.ap.channel        = s_fixed_channel ? s_fixed_channel : 1;
        apcfg.ap.max_connection = 4;
        apcfg.ap.authmode       = WIFI_AUTH_OPEN;
        ESP_LOGI(TAG_EN, "Enabling APSTA (AP SSID=%s ch=%d)", apcfg.ap.ssid, apcfg.ap.channel);
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &apcfg));
    }

    // Add broadcast peer(s)
    esp_now_peer_info_t peer_sta = { .ifidx = WIFI_IF_STA, .channel = 0, .encrypt = false };
    memcpy(peer_sta.peer_addr, (uint8_t[6]){0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}, 6);
    if (!esp_now_is_peer_exist(peer_sta.peer_addr)) esp_now_add_peer(&peer_sta);

    if (mode == WIFI_MODE_APSTA || mode == WIFI_MODE_AP) {
        esp_now_peer_info_t peer_ap = { .ifidx = WIFI_IF_AP, .channel = 0, .encrypt = false };
        memcpy(peer_ap.peer_addr, (uint8_t[6]){0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}, 6);
        if (!esp_now_is_peer_exist(peer_ap.peer_addr)) esp_now_add_peer(&peer_ap);
    }
    return ESP_OK;
}

esp_err_t espnow_manager_enable_power_saving(void) {
    #if CONFIG_ESPNOW_ENABLE_POWER_SAVE
    ESP_LOGI(TAG_EN, "Enabling ESP-NOW Power-Saving mode.");
    ESP_ERROR_CHECK(esp_now_set_wake_window(CONFIG_ESPNOW_WAKE_WINDOW));
    ESP_ERROR_CHECK(esp_wifi_connectionless_module_set_wake_interval(CONFIG_ESPNOW_WAKE_INTERVAL));
    return ESP_OK;
    #else
    ESP_LOGW(TAG_EN, "Power saving not enabled in menuconfig. Skipping.");
    return ESP_ERR_NOT_SUPPORTED;
    #endif
}

// --- Main Processing Task ---

static void espnow_manager_task(void *pvParameter) {
    espnow_mgr_event_t evt;
    ESP_LOGI(TAG_EN, "ESP-NOW manager task started.");

    for (;;) {
        if (xQueueReceive(s_espnow_queue, &evt, portMAX_DELAY) == pdTRUE) {
            switch (evt.id) {
                case ESPNOW_MGR_SEND_CB: {
                    if (s_send_cb) {
                        s_send_cb(evt.info.send_cb.mac_addr, evt.info.send_cb.status);
                    }
                    break;
                }
                case ESPNOW_MGR_RECV_CB: {
                    if (s_collect_mode) {
                        handle_possible_ctrl_beacon(evt.info.recv_cb.mac_addr, evt.info.recv_cb.data, evt.info.recv_cb.data_len);
                    }
                    if (s_recv_cb) {
                        s_recv_cb(&evt.info.recv_cb);
                    }
                    // IMPORTANT: Free the data buffer allocated in the recv callback
                    free(evt.info.recv_cb.data);
                    break;
                }
                default:
                    ESP_LOGW(TAG_EN, "Unknown event type: %d", evt.id);
                    break;
            }
        }
    }
}

/* Send data to a specific MAC address. */
esp_err_t espnow_manager_send_to_mac(const uint8_t *mac, const void *data, size_t len) {
    if (!s_inited || !mac || !data || len == 0) return ESP_ERR_INVALID_STATE;
    if (len > ESP_NOW_MAX_DATA_LEN) {
        ESP_LOGW(TAG_EN, "Truncating payload %u -> %d bytes", (unsigned)len, ESP_NOW_MAX_DATA_LEN);
        len = ESP_NOW_MAX_DATA_LEN;
    }

    // Ensure peer exists before sending
    if (!esp_now_is_peer_exist(mac)) {
        esp_now_peer_info_t peer = {0};
        memcpy(peer.peer_addr, mac, 6);
        peer.ifidx = WIFI_IF_STA; // Default to STA, can be refined
        peer.channel = 0; // Use current channel
        peer.encrypt = false; // Using PMK, so this is false
        esp_err_t add_err = esp_now_add_peer(&peer);
        if (add_err != ESP_OK && add_err != ESP_ERR_ESPNOW_EXIST) {
            ESP_LOGE(TAG_EN, "Failed to add peer %02X:%02X:%02X:%02X:%02X:%02X, error: %s",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], esp_err_to_name(add_err));
            return add_err;
        }
    }

    return esp_now_send(mac, (const uint8_t *)data, len);
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