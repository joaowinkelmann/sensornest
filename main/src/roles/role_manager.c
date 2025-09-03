#include "roles/role_manager.h"
#include "esp_log.h"
#include "nvs.h"
#include <string.h>
#include "roles/worker_role.h"
#include "roles/controller_role.h"

static const char *TAG_RM = "ROLE_MGR";
static device_role_t s_cached = DEVICE_ROLE_UNKNOWN;

static device_role_t parse_role(const char *str) {
    if (!str) return DEVICE_ROLE_UNKNOWN;
    if (strcmp(str, "WORKER") == 0) return DEVICE_ROLE_WORKER;
    if (strcmp(str, "CONTROLLER") == 0) return DEVICE_ROLE_CONTROLLER;
    return DEVICE_ROLE_UNKNOWN;
}

device_role_t role_manager_get_cached(void) { return s_cached; }

static bool load_role_from_nvs(char *out_buf, size_t out_len)
{
    if (!out_buf || out_len == 0) return false;
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("device", NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        return false;
    }

    size_t len = out_len; // provide full capacity to nvs_get_str
    err = nvs_get_str(nvs, "role", out_buf, &len);
    nvs_close(nvs);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return false;
    }
    if (err != ESP_OK) {
        ESP_LOGW("ROLE_MGR", "Failed to read 'role' from NVS: %s", esp_err_to_name(err));
        // make sure buffer is safe to print
        out_buf[0] = '\0';
        return false;
    }

    // nvs_get_str returns length INCLUDING null terminator in 'len'
    if (len == 0 || len > out_len) {
        out_buf[0] = '\0';
        return false;
    }

    // Defensive: ensure null-termination
    out_buf[out_len - 1] = '\0';
    return true;
}

static device_role_t load_role_from_nvs_old(void) {
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
        s_cached = load_role_from_nvs_old();

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