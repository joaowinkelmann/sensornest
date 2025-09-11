#pragma once
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_wifi_types.h"
#include "esp_now.h" // For esp_now_send_status_t

// --- NEW: Event-driven, queue-based architecture ---

typedef enum {
    ESPNOW_MGR_SEND_CB,
    ESPNOW_MGR_RECV_CB,
} espnow_mgr_event_id_t;

typedef struct {
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    esp_now_send_status_t status;
} espnow_mgr_event_send_cb_t;

typedef struct {
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    uint8_t *data;
    int data_len;
    wifi_interface_t ifidx;
} espnow_mgr_event_recv_cb_t;

typedef struct {
    espnow_mgr_event_id_t id;
    union {
        espnow_mgr_event_send_cb_t send_cb;
        espnow_mgr_event_recv_cb_t recv_cb;
    } info;
} espnow_mgr_event_t;


/* Application-level callbacks, now invoked from a lower-priority task */
typedef void (*espnow_mgr_send_cb_t)(const uint8_t *mac_addr, esp_now_send_status_t status);
typedef void (*espnow_mgr_rx_cb_t)(const espnow_mgr_event_recv_cb_t *recv_info);


/* Initialize ESP-NOW manager. This will create the event queue and processing task. */
esp_err_t espnow_manager_init(void);

/* Deinitialize ESP-NOW manager. */
void espnow_manager_deinit(void);

/* Register application-level callbacks */
void espnow_manager_register_callbacks(espnow_mgr_send_cb_t send_cb, espnow_mgr_rx_cb_t recv_cb);

/* Configure the device role. Must be called after init. */
esp_err_t espnow_manager_config_role(bool is_controller);

/* Send data to a specific MAC address. */
esp_err_t espnow_manager_send_to_mac(const uint8_t *mac, const void *data, size_t len);

/* Broadcast send. */
esp_err_t espnow_manager_send_broadcast(const void *data, size_t len);

/* Send data to the configured controller (if any). */
esp_err_t espnow_manager_send_to_controller(const void *data, size_t len);

/* Controller: start periodic broadcast beacon. */
esp_err_t espnow_manager_start_controller_beacon(const char *name, int interval_ms);

/* Set controller beacon name at runtime. */
void espnow_manager_set_controller_name(const char *name);

/* Worker: collect controller beacons. */
esp_err_t espnow_manager_collect_controllers(int duration_ms, char **out_json);

/* Check if ESPNOW manager is initialized */
bool espnow_manager_is_initialized(void);

/* Set/get fixed ESPNOW channel */
esp_err_t espnow_manager_set_fixed_channel(uint8_t ch);
uint8_t  espnow_manager_get_fixed_channel(void);

/* Store/retrieve controller MAC */
void espnow_manager_set_controller_mac(const uint8_t mac[6]);
bool espnow_manager_get_controller_mac(uint8_t mac[6]);

/* NEW: Configure ESP-NOW for power-saving mode (for worker) */
esp_err_t espnow_manager_enable_power_saving(void);
