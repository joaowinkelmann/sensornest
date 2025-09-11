#pragma once
#include <stdbool.h>
#include "esp_err.h"

/* Initialize Wi-Fi (lazy inside connect if not called). */
esp_err_t wifi_manager_init(void);

/* Connect using saved NVS creds (namespace "wifi": ssid/pass). timeout_ms total wait. */
bool wifi_manager_connect_saved(int timeout_ms);

/* Connect with provided credentials (not persisted). */
bool wifi_manager_connect(const char *ssid, const char *pass, int timeout_ms);

/* Disconnect STA (keeps driver up). */
void wifi_manager_disconnect(void);

/* Check if STA has IP. */
bool wifi_manager_is_connected(void);

/* Get current STA SSID (returns true if connected). */
bool wifi_manager_get_ssid(char *out, size_t len);

esp_err_t wifi_manager_prefetch_scan(int max_age_ms);
esp_err_t wifi_manager_get_cached_scan(char **out_json);