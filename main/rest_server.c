/*
 * Exposes routes for the front-end to use internally
*/
#include <string.h>
#include <fcntl.h>
#include "esp_http_server.h"
#include "esp_chip_info.h"
#include "esp_random.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_vfs.h"
#include "cJSON.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include <errno.h>

// Cert
#include "certs.h"
#include "rest_client.h"

static const char *TAG = "REST_SERVER";

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)
#define SCRATCH_BUFSIZE (10240)
#define MAX_URI_HANDLERS 32
static inline void rs_add_close_hdr(httpd_req_t *req);

// Move struct definition before using its type in prototype
typedef struct rest_server_context {
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE];
} rest_server_context_t;

// Forward declaration (after struct so the type is known)
static void register_captive_handlers(httpd_handle_t server, rest_server_context_t *ctx);

#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

#if CONFIG_EXAMPLE_DEPLOY_WEB_PAGES
/* Set HTTP response content type according to file extension */
static esp_err_t rs_set_content_type_from_file(httpd_req_t *req, const char *filepath)
{
    const char *type = "text/plain";
    if (CHECK_FILE_EXTENSION(filepath, ".html")) {
        type = "text/html";
    } else if (CHECK_FILE_EXTENSION(filepath, ".js")) {
        type = "application/javascript";
    } else if (CHECK_FILE_EXTENSION(filepath, ".css")) {
        type = "text/css";
    } else if (CHECK_FILE_EXTENSION(filepath, ".png")) {
        type = "image/png";
    } else if (CHECK_FILE_EXTENSION(filepath, ".ico")) {
        type = "image/x-icon";
    } else if (CHECK_FILE_EXTENSION(filepath, ".svg")) {
        type = "text/xml";
    }
    return httpd_resp_set_type(req, type);
}

/* Send HTTP response with the contents of the requested file */
static esp_err_t rs_rest_common_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];

    // --- NEW: Detect old captive portal UAs and serve a fallback page ---
    char ua[128] = {0};
    httpd_req_get_hdr_value_str(req, "User-Agent", ua, sizeof(ua));
    if (strstr(ua, "Chrome/60.") || strstr(ua, "CaptivePortal")) {
        // This UA is from an old Android captive portal WebView.
        // Serve a simple HTML page asking the user to open in a real browser.
        ESP_LOGW(TAG, "Old captive portal UA detected: '%s'. Serving fallback page.", ua);
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        const char *fallback_html =
            "<!DOCTYPE html><html><head><title>Wi-Fi Setup</title>"
            "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
            "<style>"
            " body { font-family: sans-serif; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; background-color: #f0f0f0; text-align: center; }"
            " div { padding: 20px; border: 1px solid #ccc; border-radius: 8px; background: #fff; }"
            " a { display: inline-block; padding: 15px 25px; font-size: 18px; color: white; background-color: #007bff; text-decoration: none; border-radius: 5px; }"
            "</style></head><body><div>"
            "<h1>Please open in browser</h1>"
            "<p>This network requires configuration in your main web browser.</p><br>"
            "<a href=\"http://192.168.4.1/\">Open Setup Page</a>"
            "</div></body></html>";
        return httpd_resp_send(req, fallback_html, HTTPD_RESP_USE_STRLEN);
    }

    rest_server_context_t *rest_context = (rest_server_context_t *)req->user_ctx;
    strlcpy(filepath, rest_context->base_path, sizeof(filepath));
    if (req->uri[strlen(req->uri) - 1] == '/') {
        strlcat(filepath, "/index.html", sizeof(filepath));
    } else {
        strlcat(filepath, req->uri, sizeof(filepath));
    }
    FILE *fd = fopen(filepath, "rb");
    if (!fd) {
        char host[128] = {0}, ua[128] = {0};
        httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host));
        httpd_req_get_hdr_value_str(req, "User-Agent", ua, sizeof(ua));
        ESP_LOGW(TAG, "rest_common_get_handler: Failed to open file : %s (uri=%s) Host=%s UA='%s'", filepath, req->uri, host[0]?host:"(no host)", ua);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    // FIX: call correct helper name (had missing rs_ prefix)
    rs_set_content_type_from_file(req, filepath);
    rs_add_close_hdr(req);   // <--- ensure client won't keep socket alive

    char *chunk = rest_context->scratch;
    ssize_t read_bytes;
    do {
        /* Read file in chunks into the scratch buffer */
        read_bytes = fread(chunk, 1, SCRATCH_BUFSIZE, fd);
        if (read_bytes == -1) {
            ESP_LOGE(TAG, "Failed to read file : %s", filepath);
        } else if (read_bytes > 0) {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
                // --- NEW: Add a small delay on EAGAIN to allow client to catch up ---
                if (httpd_get_global_transport_ctx(req->handle) && errno == EAGAIN) {
                    vTaskDelay(pdMS_TO_TICKS(50)); // Wait 50ms
                    if (httpd_resp_send_chunk(req, chunk, read_bytes) == ESP_OK) {
                        continue; // Retry succeeded
                    }
                }
                fclose(fd);
                ESP_LOGE(TAG, "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                return ESP_FAIL;
            }
        }
    } while (read_bytes > 0);
    /* Close file after sending complete */
    fclose(fd);
    ESP_LOGI(TAG, "File sending complete: %s", filepath);
    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}
#endif // CONFIG_EXAMPLE_DEPLOY_WEB_PAGES

/* Simple handler for light brightness control */
static esp_err_t rs_light_brightness_post_handler(httpd_req_t *req)
{
    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    int received = 0;
    if (total_len >= SCRATCH_BUFSIZE) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    int red = cJSON_GetObjectItem(root, "red")->valueint;
    int green = cJSON_GetObjectItem(root, "green")->valueint;
    int blue = cJSON_GetObjectItem(root, "blue")->valueint;
    ESP_LOGI(TAG, "Light control: red = %d, green = %d, blue = %d", red, green, blue);
    cJSON_Delete(root);
    httpd_resp_sendstr(req, "Post control value successfully");
    return ESP_OK;
}

/* Simple handler for getting system handler */
static esp_err_t rs_system_info_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    cJSON_AddStringToObject(root, "chip", CONFIG_IDF_TARGET);
    cJSON_AddStringToObject(root, "idf_version", IDF_VER);
    cJSON_AddNumberToObject(root, "cores", chip_info.cores);
    const char *sys_info = cJSON_Print(root);
    httpd_resp_sendstr(req, sys_info);
    free((void *)sys_info);
    cJSON_Delete(root);
    return ESP_OK;
}

/* Simple handler for getting temperature data */
static esp_err_t rs_temperature_data_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    // Note: we're simulating temperature data with a random number for demonstration purposes
    cJSON_AddNumberToObject(root, "raw", esp_random() % 20);
    const char *sys_info = cJSON_Print(root);
    httpd_resp_sendstr(req, sys_info);
    free((void *)sys_info);
    cJSON_Delete(root);
    return ESP_OK;
}

/* --- NEW: Helper to set setup stage in NVS --- */
static void nvs_set_setup_stage(const char *stage) {
    nvs_handle_t nvs;
    if (nvs_open("device", NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_str(nvs, "setup", stage);
        nvs_commit(nvs);
        nvs_close(nvs);
    }
}

/* --- NEW: POST /api/v1/device/role --- */
static esp_err_t rs_device_role_post_handler(httpd_req_t *req) {
    int total_len = req->content_len;
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    if (total_len >= SCRATCH_BUFSIZE) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    int received = 0, cur_len = 0;
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len - cur_len);
        if (received <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "recv failed");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid json");
        return ESP_FAIL;
    }
    const cJSON *j_role = cJSON_GetObjectItem(root, "role");
    if (!cJSON_IsString(j_role)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "role required");
        return ESP_FAIL;
    }
    const char *role = j_role->valuestring;
    if (strcmp(role, "CONTROLLER") != 0 && strcmp(role, "WORKER") != 0) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid role");
        return ESP_FAIL;
    }

    nvs_handle_t nvs;
    esp_err_t err = nvs_open("device", NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "nvs open failed");
        return ESP_FAIL;
    }
    err = nvs_set_str(nvs, "role", role);
    if (err == ESP_OK) err = nvs_commit(nvs);
    nvs_close(nvs);
    cJSON_Delete(root);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "nvs write failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Device role set to: %s", role);

    nvs_set_setup_stage("role_selected");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\",\"role_saved\":true}");
    return ESP_OK;
}

/* --- NEW: GET /api/v1/setup/status --- */
static esp_err_t rs_setup_status_get_handler(httpd_req_t *req) {
    char role[16] = {0};
    char stage[32] = {0};
    bool has_wifi = false;
    bool connected = false;
    char ssid[33] = {0};
    char ip_str[16] = {0};

    // Read role and stage from NVS
    nvs_handle_t nvs_dev;
    if (nvs_open("device", NVS_READONLY, &nvs_dev) == ESP_OK) {
        size_t rsz = sizeof(role);
        size_t ssz = sizeof(stage);
        nvs_get_str(nvs_dev, "role", role, &rsz);
        nvs_get_str(nvs_dev, "setup", stage, &ssz);
        nvs_close(nvs_dev);
    }

    // Check if Wi-Fi creds exist
    nvs_handle_t nvs_wifi;
    if (nvs_open("wifi", NVS_READONLY, &nvs_wifi) == ESP_OK) {
        size_t ssz = sizeof(ssid);
        if (nvs_get_str(nvs_wifi, "ssid", ssid, &ssz) == ESP_OK) {
            has_wifi = true;
        }
        nvs_close(nvs_wifi);
        ssid[0] = '\0'; // don't expose saved SSID here
    }

    // Connection info
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        connected = true;
        strncpy((char *)ssid, (const char *)ap_info.ssid, sizeof(ssid) - 1);
        esp_netif_t *sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (sta) {
            esp_netif_ip_info_t ip;
            if (esp_netif_get_ip_info(sta, &ip) == ESP_OK) {
                snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip.ip));
            }
        }
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "role", role[0] ? role : NULL);
    cJSON_AddStringToObject(root, "stage", stage[0] ? stage : NULL);
    cJSON_AddBoolToObject(root, "has_wifi", has_wifi);
    cJSON_AddBoolToObject(root, "connected", connected);
    if (connected) {
        cJSON_AddStringToObject(root, "ssid", ssid);
        cJSON_AddStringToObject(root, "ip", ip_str);
    }

    const char *json = cJSON_PrintUnformatted(root);

    ESP_LOGI(TAG, "Setup status: role=%s, stage=%s, has_wifi=%d, connected=%d, ssid=%s, ip=%s",
             role[0] ? role : "N/A", stage[0] ? stage : "N/A", has_wifi, connected, ssid, ip_str);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);
    free((void *)json);
    cJSON_Delete(root);
    return ESP_OK;
}

/* Provisioning handler - save WiFi credentials */
static esp_err_t rs_wifi_provision_post_handler(httpd_req_t *req)
{
    int total_len = req->content_len;
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    if (total_len >= SCRATCH_BUFSIZE) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    int received = 0, cur_len = 0;
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len - cur_len);
        if (received <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "recv failed");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';
    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid json");
        return ESP_FAIL;
    }
    const cJSON *j_ssid = cJSON_GetObjectItem(root, "ssid");
    const cJSON *j_pass = cJSON_GetObjectItem(root, "password");
    if (!cJSON_IsString(j_ssid) || !cJSON_IsString(j_pass)) {
        ESP_LOGE(TAG, "Invalid Wi-Fi provision data: %s", buf);
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "ssid/password required");
        return ESP_FAIL;
    }
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("wifi", NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "nvs open failed");
        return ESP_FAIL;
    }
    err = nvs_set_str(nvs, "ssid", j_ssid->valuestring);
    if (err == ESP_OK) err = nvs_set_str(nvs, "pass", j_pass->valuestring);
    if (err == ESP_OK) err = nvs_commit(nvs);
    nvs_close(nvs);
    cJSON_Delete(root);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "nvs write failed");
        return ESP_FAIL;
    }
    // --- NEW: advance setup stage ---
    nvs_set_setup_stage("wifi_saved");

    ESP_LOGI(TAG, "Wi-Fi credentials saved: SSID=%s", j_ssid->valuestring);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"saved\"}");
    return ESP_OK;
}

/* Provisioning handler - retry connection using saved credentials */
static esp_err_t rs_wifi_retry_post_handler(httpd_req_t *req)
{
    wifi_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    nvs_handle_t nvs;
    if (nvs_open("wifi", NVS_READONLY, &nvs) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "no creds");
        return ESP_FAIL;
    }
    size_t ssz = sizeof(cfg.sta.ssid);
    size_t psz = sizeof(cfg.sta.password);
    esp_err_t err = nvs_get_str(nvs, "ssid", (char*)cfg.sta.ssid, &ssz);
    if (err == ESP_OK) err = nvs_get_str(nvs, "pass", (char*)cfg.sta.password, &psz);
    nvs_close(nvs);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid creds");
        return ESP_FAIL;
    }

    // Keep AP up while connecting via STA
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    esp_wifi_start();
    esp_wifi_connect();

    // --- NEW: mark as connecting ---
    nvs_set_setup_stage("connecting");

    ESP_LOGI(TAG, "Retrying Wi-Fi connection with SSID: %s", cfg.sta.ssid);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"connecting\"}");
    return ESP_OK;
}

/* --- NEW: Wi-Fi scan (GET /api/v1/wifi/scan) --- */
static esp_err_t rs_wifi_scan_get_handler(httpd_req_t *req) {
    // Ensure we're in APSTA so scan can run without dropping AP
    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);
    if (mode != WIFI_MODE_APSTA) {
        esp_wifi_set_mode(WIFI_MODE_APSTA);
    }

    wifi_scan_config_t scan_cfg = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time = {
            .active = { .min = 100, .max = 300 },
        },
    };

    esp_err_t err = esp_wifi_scan_start(&scan_cfg, true); // block until done
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "scan failed");
        return ESP_FAIL;
    }

    uint16_t ap_num = 0;
    esp_wifi_scan_get_ap_num(&ap_num);
    wifi_ap_record_t *ap_records = calloc(ap_num, sizeof(wifi_ap_record_t));
    if (!ap_records) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no memory");
        return ESP_FAIL;
    }
    esp_wifi_scan_get_ap_records(&ap_num, ap_records);

    cJSON *root = cJSON_CreateArray();
    for (int i = 0; i < ap_num; ++i) {
        cJSON *ap = cJSON_CreateObject();
        cJSON_AddStringToObject(ap, "ssid", (const char *)ap_records[i].ssid);
        cJSON_AddNumberToObject(ap, "rssi", ap_records[i].rssi);
        cJSON_AddNumberToObject(ap, "channel", ap_records[i].primary);
        cJSON_AddNumberToObject(ap, "auth", ap_records[i].authmode);
        cJSON_AddItemToArray(root, ap);
    }
    free(ap_records);

    ESP_LOGI(TAG, "Wi-Fi scan found %d APs", ap_num);

    const char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);
    free((void *)json);
    cJSON_Delete(root);
    return ESP_OK;
}

/* --- NEW: Wi-Fi status (GET /api/v1/wifi/status) --- */
static esp_err_t rs_wifi_status_get_handler(httpd_req_t *req) {
    bool connected = false;
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        connected = true;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "connected", connected);
    if (connected) {
        cJSON_AddStringToObject(root, "ssid", (const char *)ap_info.ssid);
        cJSON_AddNumberToObject(root, "rssi", ap_info.rssi);

        esp_netif_t *sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (sta) {
            esp_netif_ip_info_t ip;
            if (esp_netif_get_ip_info(sta, &ip) == ESP_OK) {
                char ip_str[16];
                snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip.ip));
                cJSON_AddStringToObject(root, "ip", ip_str);
            }
        }
    }

    httpd_resp_set_type(req, "application/json");
    const char *json = cJSON_PrintUnformatted(root);
    httpd_resp_sendstr(req, json);
    free((void *)json);
    cJSON_Delete(root);
    return ESP_OK;
}

/* Add handlers to make mobile OSes trigger captive portal UI */
static esp_err_t rs_probe_generate_204_handler(httpd_req_t *req)
{
    char host[64] = {0}, ua[128] = {0};
    httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host));
    httpd_req_get_hdr_value_str(req, "User-Agent", ua, sizeof(ua));
    ESP_LOGI(TAG, "probe /generate_204 from %s UA='%s'", host[0] ? host : "(no host)", ua);

    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    rs_add_close_hdr(req);
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t rs_probe_hotspot_detect_handler(httpd_req_t *req)
{
    char host[64] = {0}, ua[128] = {0};
    httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host));
    httpd_req_get_hdr_value_str(req, "User-Agent", ua, sizeof(ua));
    ESP_LOGI(TAG, "probe /hotspot-detect.html from %s UA='%s'", host[0] ? host : "(no host)", ua);

    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    return httpd_resp_send(req, NULL, 0);
}

/* catch-all GET -> redirect to landing page (index.html) */
static esp_err_t rs_captive_redirect_handler(httpd_req_t *req)
{
    char host[128] = {0}, ua[128] = {0}, referer[128] = {0};
    httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host));
    httpd_req_get_hdr_value_str(req, "User-Agent", ua, sizeof(ua));
    httpd_req_get_hdr_value_str(req, "Referer", referer, sizeof(referer));

    ESP_LOGI(TAG, "captive redirect: uri=%s host=%s UA='%s' Referer='%s'", 
             req->uri, host[0]?host:"(no host)", ua, referer);

    // This handler should only be active when web pages are NOT deployed.
    // The main file server will handle redirects and fallbacks otherwise.
    #if !CONFIG_EXAMPLE_DEPLOY_WEB_PAGES
    const char *target = "/";
    char location[256];
    snprintf(location, sizeof(location), "http://192.168.4.1%s", target);

    ESP_LOGI(TAG, "External domain redirect: %s -> %s", req->uri, location);

    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", location);
    httpd_resp_sendstr(req, "");
    #else
    // If web pages are deployed, the common_get_handler will catch everything.
    // This handler shouldn't be reached, but if it is, forward to the file server.
    return rs_rest_common_get_handler(req);
    #endif
    return ESP_OK;
}

// --- NEW: helper to read JSON body safely (used by cloud handlers) ---
static bool rs_read_body(httpd_req_t *req, char *buf, size_t cap) {
    int total = req->content_len;
    if (total <= 0 || total >= (int)cap) return false;
    int off = 0;
    while (off < total) {
        int r = httpd_req_recv(req, buf + off, total - off);
        if (r <= 0) return false;
        off += r;
    }
    buf[total] = 0;
    return true;
}

// --- NEW: POST /api/v1/cloud/login (reuses rc_login + rc_get_device_by_mac) ---
static esp_err_t rs_cloud_login_post_handler(httpd_req_t *req) {
    rest_server_context_t *ctx = (rest_server_context_t*)req->user_ctx;
    if (!rs_read_body(req, ctx->scratch, sizeof(ctx->scratch))) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid body");
        return ESP_FAIL;
    }
    cJSON *root = cJSON_Parse(ctx->scratch);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid json");
        return ESP_FAIL;
    }
    cJSON *j_email = cJSON_GetObjectItem(root, "email");
    cJSON *j_pass  = cJSON_GetObjectItem(root, "password");
    if (!cJSON_IsString(j_email) || !cJSON_IsString(j_pass)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "email/password required");
        return ESP_FAIL;
    }
    esp_err_t err = rc_login(j_email->valuestring, j_pass->valuestring);
    if (err != ESP_OK) {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"error\":\"login_failed\"}");
        return ESP_OK;
    }
    // Optionally fetch device metadata immediately
    char *device_json = NULL;
    esp_err_t derr = rc_get_device_by_mac(NULL, &device_json);
    httpd_resp_set_type(req, "application/json");
    if (derr == ESP_OK && device_json) {
        size_t resp_len = strlen(device_json) + 32;
        char *resp = malloc(resp_len);
        if (!resp) {
            free(device_json);
            httpd_resp_sendstr(req, "{\"status\":\"ok\",\"device\":null}");
            cJSON_Delete(root);
            return ESP_OK;
        }
        snprintf(resp, resp_len, "{\"status\":\"ok\",\"device\":%s}", device_json);
        httpd_resp_sendstr(req, resp);
        free(resp);
        free(device_json);
    } else {
        httpd_resp_sendstr(req, "{\"status\":\"ok\",\"device\":null}");
        if (device_json) free(device_json);
    }
    cJSON_Delete(root);
    return ESP_OK;
}

// --- NEW: GET /api/v1/cloud/device?mac=AA:BB:CC:DD:EE:FF ---
static esp_err_t rs_cloud_device_get_handler(httpd_req_t *req) {
    char query[128] = {0};
    char mac[32] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        httpd_query_key_value(query, "mac", mac, sizeof(mac));
    }
    char *device_json = NULL;
    esp_err_t err = rc_get_device_by_mac(mac[0] ? mac : NULL, &device_json);
    httpd_resp_set_type(req, "application/json");
    if (err == ESP_OK && device_json) {
        httpd_resp_sendstr(req, device_json);
        free(device_json);
        return ESP_OK;
    }
    if (err == ESP_ERR_INVALID_STATE) {
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_sendstr(req, "{\"error\":\"unauthorized\"}");
        return ESP_OK;
    }
    if (err == ESP_ERR_NOT_FOUND) {
        httpd_resp_set_status(req, "404 Not Found");
        httpd_resp_sendstr(req, "{\"error\":\"device_not_found\"}");
        return ESP_OK;
    }
    httpd_resp_set_status(req, "502 Bad Gateway");
    httpd_resp_sendstr(req, "{\"error\":\"cloud_error\"}");
    return ESP_OK;
}

esp_err_t start_rest_server(const char *base_path)
{
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(base_path && strlen(base_path) < ESP_VFS_PATH_MAX, ESP_ERR_INVALID_ARG, TAG, "Invalid base path");
    rest_server_context_t *rest_context = calloc(1, sizeof(rest_server_context_t));
    ESP_RETURN_ON_FALSE(rest_context, ESP_ERR_NO_MEM, TAG, "No memory for rest context");
    strlcpy(rest_context->base_path, base_path, sizeof(rest_context->base_path));

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = MAX_URI_HANDLERS;

    /* --- NEW: socket lifecycle tuning to mitigate ENFILE/EMFILE (accept error 23) --- */
    config.lru_purge_enable = true;      // Reclaim least‑recently used session if max sockets reached
    config.enable_so_linger = true;      // Enable SO_LINGER on closed sockets
    config.linger_timeout = 0;           // 0 => send RST on close (frees TIME_WAIT immediately). Use 1–2 if you prefer graceful FIN.
    // Optionally raise if you increased lwIP limits:
    // config.max_open_sockets = 8;

    ESP_LOGI(TAG, "Starting HTTP Server");
    ESP_GOTO_ON_ERROR(httpd_start(&server, &config), err, TAG, "Failed to start http server");

    /* Register API endpoints (specific paths) */
    httpd_uri_t system_info_get_uri = {
        .uri = "/api/v1/system/info",
        .method = HTTP_GET,
        .handler = rs_system_info_get_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &system_info_get_uri);

    httpd_uri_t temperature_data_get_uri = {
        .uri = "/api/v1/temp/raw",
        .method = HTTP_GET,
        .handler = rs_temperature_data_get_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &temperature_data_get_uri);

    httpd_uri_t light_brightness_post_uri = {
        .uri = "/api/v1/light/brightness",
        .method = HTTP_POST,
        .handler = rs_light_brightness_post_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &light_brightness_post_uri);

    httpd_uri_t wifi_provision_post_uri = {
        .uri = "/api/v1/provision/wifi",
        .method = HTTP_POST,
        .handler = rs_wifi_provision_post_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &wifi_provision_post_uri);

    httpd_uri_t wifi_retry_post_uri = {
        .uri = "/api/v1/provision/retry",
        .method = HTTP_POST,
        .handler = rs_wifi_retry_post_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &wifi_retry_post_uri);

    httpd_uri_t device_role_post_uri = {
        .uri = "/api/v1/device/role",
        .method = HTTP_POST,
        .handler = rs_device_role_post_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &device_role_post_uri);

    httpd_uri_t setup_status_get_uri = {
        .uri = "/api/v1/setup/status",
        .method = HTTP_GET,
        .handler = rs_setup_status_get_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &setup_status_get_uri);

    httpd_uri_t wifi_scan_get_uri = {
        .uri = "/api/v1/wifi/scan",
        .method = HTTP_GET,
        .handler = rs_wifi_scan_get_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &wifi_scan_get_uri);

    httpd_uri_t wifi_status_get_uri = {
        .uri = "/api/v1/wifi/status",
        .method = HTTP_GET,
        .handler = rs_wifi_status_get_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &wifi_status_get_uri);

    // --- NEW registrations for cloud integration ---
    httpd_uri_t cloud_login_post_uri = {
        .uri = "/api/v1/cloud/login",
        .method = HTTP_POST,
        .handler = rs_cloud_login_post_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &cloud_login_post_uri);

    httpd_uri_t cloud_device_get_uri = {
        .uri = "/api/v1/cloud/device",
        .method = HTTP_GET,
        .handler = rs_cloud_device_get_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &cloud_device_get_uri);

    /* Register captive portal probe handlers (specific paths) */
    register_captive_handlers(server, rest_context);

    #if CONFIG_EXAMPLE_DEPLOY_WEB_PAGES
    /* Register file server for static assets LAST, so it acts as a fallback for non-API/probe routes */
    httpd_uri_t common_get_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = rs_rest_common_get_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &common_get_uri);
    #else
    /* If no web files, register a catch-all captive redirect as the last resort */
    httpd_uri_t wildcard = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = rs_captive_redirect_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &wildcard);
    #endif

    return ESP_OK;
err:
    if (rest_context) {
        free(rest_context);
    }
    return ret;
}

//  --- ADD forward declarations for captive probe handlers & registrar ---
static esp_err_t rs_probe_generate_204_handler(httpd_req_t *req);
static esp_err_t rs_probe_hotspot_detect_handler(httpd_req_t *req);
static esp_err_t rs_captive_redirect_handler(httpd_req_t *req);

// Registrar to fix implicit declaration error
static void register_captive_handlers(httpd_handle_t server, rest_server_context_t *ctx) {
    httpd_uri_t probe_generate_204 = {
        .uri      = "/generate_204",          // Android
        .method   = HTTP_GET,
        .handler  = rs_probe_generate_204_handler,
        .user_ctx = ctx
    };
    httpd_register_uri_handler(server, &probe_generate_204);

    httpd_uri_t probe_hotspot_detect = {
        .uri      = "/hotspot-detect.html",   // iOS / macOS
        .method   = HTTP_GET,
        .handler  = rs_probe_hotspot_detect_handler,
        .user_ctx = ctx
    };
    httpd_register_uri_handler(server, &probe_hotspot_detect);

    // Windows NCSI
    httpd_uri_t probe_ncsi = {
        .uri      = "/ncsi.txt",
        .method   = HTTP_GET,
        .handler  = rs_probe_generate_204_handler,
        .user_ctx = ctx
    };
    httpd_register_uri_handler(server, &probe_ncsi);

    // Generic catch for some OEMs
    httpd_uri_t probe_fwlink = {
        .uri      = "/fwlink",
        .method   = HTTP_GET,
        .handler  = rs_probe_generate_204_handler,
        .user_ctx = ctx
    };
    httpd_register_uri_handler(server, &probe_fwlink);

    // The main "/*" handler in start_rest_server will now handle general captive redirects.
}

static inline void rs_add_close_hdr(httpd_req_t *req) {
    httpd_resp_set_hdr(req, "Connection", "close");
}