#pragma once
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_now.h"

/* RX callback (controller side). Data is not null‑terminated. */
typedef void (*espnow_mgr_rx_cb_t)(const uint8_t *mac, const uint8_t *data, int len);

/* Initialize ESPNOW (idempotent).
 * controller=true => register receive callback.
 */
esp_err_t espnow_manager_init(bool controller, espnow_mgr_rx_cb_t rx_cb);

/* Broadcast send (truncates >200 bytes). */
esp_err_t espnow_manager_send_broadcast(const void *data, size_t len);

/* Helper to fetch (and cache) controller MAC if later you move away from broadcast.
 * Currently unused (broadcast only). */
bool espnow_manager_get_controller_mac(uint8_t mac[6]);

/* Optional: set controller MAC (e.g., discovered) – retained in RAM. */
void espnow_manager_set_controller_mac(const uint8_t mac[6]);

/* Controller: start periodic broadcast beacon with name. interval_ms >= 500. */
esp_err_t espnow_manager_start_controller_beacon(const char *name, int interval_ms);

/* Worker: collect controller beacons for duration_ms (blocking delay loop).
 * On success *out_json holds a malloc'd JSON array string; caller must free().
 * Format: [{"mac":"AA:BB:CC:DD:EE:FF","name":"ControllerName"}]
 */
esp_err_t espnow_manager_collect_controllers(int duration_ms, char **out_json);

/* Check if ESPNOW manager is initialized */
bool espnow_manager_is_initialized(void);

esp_err_t espnow_manager_debug_set_channel(uint8_t ch); // debug helper
