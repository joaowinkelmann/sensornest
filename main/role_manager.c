#include "role_manager.h"
#include "esp_log.h"
#include "nvs.h"
#include <string.h>
#include "worker_role.h"
#include "controller_role.h"

static const char *TAG_RM = "ROLE_MGR";
static device_role_t s_cached = DEVICE_ROLE_UNKNOWN;

static device_role_t parse_role(const char *str) {
    if (!str) return DEVICE_ROLE_UNKNOWN;
    if (strcmp(str, "WORKER") == 0) return DEVICE_ROLE_WORKER;
    if (strcmp(str, "CONTROLLER") == 0) return DEVICE_ROLE_CONTROLLER;
    return DEVICE_ROLE_UNKNOWN;
}

device_role_t role_manager_get_cached(void) { return s_cached; }

static device_role_t load_role_from_nvs(void) {
    char buf[16] = {0};
    nvs_handle_t nvs;
    if (nvs_open("device", NVS_READONLY, &nvs) == ESP_OK) {
        size_t len = sizeof(buf);
        if (nvs_get_str(nvs, "role", buf, &len) == ESP_OK) {
            ESP_LOGI(TAG_RM, "Loaded role from NVS: %s", buf);
        } else {
            ESP_LOGI(TAG_RM, "Role not set in NVS yet.");
        }
        nvs_close(nvs);
    }
    return parse_role(buf[0] ? buf : NULL);
}

esp_err_t role_manager_start(void) {
    if (s_cached == DEVICE_ROLE_UNKNOWN)
        s_cached = load_role_from_nvs();

    if (s_cached == DEVICE_ROLE_UNKNOWN) {
        ESP_LOGW(TAG_RM, "Role UNKNOWN: staying idle until provisioning sets role.");
        return ESP_ERR_INVALID_STATE;
    }

    switch (s_cached) {
        case DEVICE_ROLE_WORKER:
            ESP_LOGI(TAG_RM, "Starting WORKER role logic.");
            return worker_role_start();
        case DEVICE_ROLE_CONTROLLER:
            ESP_LOGI(TAG_RM, "Starting CONTROLLER role logic.");
            return controller_role_start();
        default:
            return ESP_ERR_INVALID_ARG;
    }
}