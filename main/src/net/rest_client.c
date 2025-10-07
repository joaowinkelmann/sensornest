/*
 * Serves as a REST client for the cloud API.
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_mac.h"
#include "cJSON.h"
#include "security/certs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include <stdint.h>

bool rc_get_token(char *out, size_t len);
bool rc_has_token(void);

static const char *RC_TAG = "REST_client";
extern const char *cloud_ca_pem;

#define CLOUD_BASE_URL            "https://winkels.com.br/sensornest/api"
#define CLOUD_LOGIN_URL           CLOUD_BASE_URL "/auth/login"
#define CLOUD_DEVICE_MAC_URL      CLOUD_BASE_URL "/devices/mac/"   /* append AA:BB:CC:DD:EE:FF */
#define CLOUD_READINGS_URL        CLOUD_BASE_URL "/readings/add"  /* POST */
#define CLOUD_MESHES_URL          CLOUD_BASE_URL "/meshes"         /* GET */
#define CLOUD_DEVICE_REGISTER_URL CLOUD_BASE_URL "/devices"        /* POST */
#define CLOUD_DEVICE_UPDATE_URL   CLOUD_BASE_URL "/devices/"       /* PATCH {id} */

#define NVS_NS_AUTH             "auth"
#define NVS_KEY_TOKEN           "token"
#define NVS_KEY_EMAIL           "email"
#define NVS_KEY_PASS            "pass"

/* --- NVS helpers (UPDATED) --- */
static bool rc_store_auth(const char *email, const char *password, const char *token) {
    nvs_handle_t h;
    if (nvs_open(NVS_NS_AUTH, NVS_READWRITE, &h) != ESP_OK) return false;
    esp_err_t err = ESP_OK;
    if (token && token[0]) err = nvs_set_str(h, NVS_KEY_TOKEN, token);
    if (err == ESP_OK && email && email[0]) err = nvs_set_str(h, NVS_KEY_EMAIL, email);
    if (err == ESP_OK && password && password[0]) err = nvs_set_str(h, NVS_KEY_PASS, password);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err == ESP_OK;
}

bool rc_load_creds(char *email, size_t e_len, char *pass, size_t p_len) {
    nvs_handle_t h;
    if (nvs_open(NVS_NS_AUTH, NVS_READONLY, &h) != ESP_OK) return false;
    bool ok = true;
    if (email && e_len) {
        size_t sz = e_len;
        if (nvs_get_str(h, NVS_KEY_EMAIL, email, &sz) != ESP_OK) ok = false;
    }
    if (pass && p_len) {
        size_t sz = p_len;
        if (nvs_get_str(h, NVS_KEY_PASS, pass, &sz) != ESP_OK) ok = false;
    }
    nvs_close(h);
    return ok;
}

bool rc_get_token(char *out, size_t len) {
    if (!out || len == 0) return false;
    nvs_handle_t h;
    if (nvs_open(NVS_NS_AUTH, NVS_READONLY, &h) != ESP_OK) return false;
    size_t need = len;
    esp_err_t err = nvs_get_str(h, NVS_KEY_TOKEN, out, &need);
    nvs_close(h);
    if (err != ESP_OK) return false;
    out[len - 1] = 0;
    return true;
}

bool rc_has_token(void) {
    nvs_handle_t h;
    if (nvs_open(NVS_NS_AUTH, NVS_READONLY, &h) != ESP_OK) return false;
    size_t need = 0;
    esp_err_t err = nvs_get_str(h, NVS_KEY_TOKEN, NULL, &need);
    nvs_close(h);
    return (err == ESP_OK && need > 1);
}

/* --- Generic HTTP response accumulation (simple blocking) --- */
static esp_err_t rc_http_fetch(esp_http_client_handle_t client, char **out_buf, int *out_len, int limit) {
    if (!out_buf) return ESP_ERR_INVALID_ARG;
    *out_buf = NULL;

    // Ensure the request is actually performed before trying to read
    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(RC_TAG, "http perform failed: %s", esp_err_to_name(err));
        return err;
    }

    int status = esp_http_client_get_status_code(client);
    int content_len = esp_http_client_get_content_length(client);
    bool is_chunked = esp_http_client_is_chunked_response(client);
    ESP_LOGD(RC_TAG, "http fetch: status=%d content_len=%d chunked=%d", status, content_len, is_chunked);

    int buf_size = content_len > 0 ? content_len : limit;
    if (buf_size > limit) buf_size = limit;
    if (buf_size <= 0) buf_size = 1;

    char *buf = malloc(buf_size + 1);
    if (!buf) {
        ESP_LOGE(RC_TAG, "http fetch: alloc failed for %d bytes", buf_size);
        return ESP_ERR_NO_MEM;
    }

    // Use esp_http_client_read_response which handles chunked responses and reads until EOF or buffer fills.
    int read_len = esp_http_client_read_response(client, buf, buf_size);
    if (read_len < 0) {
        ESP_LOGE(RC_TAG, "http fetch: read_response failed: %d", read_len);
        free(buf);
        return ESP_FAIL;
    }

    // If response was larger than buffer, try to capture remaining bytes in a loop (best-effort)
    if (read_len == buf_size && !is_chunked && content_len > buf_size) {
        ESP_LOGW(RC_TAG, "http fetch: response truncated (content_len=%d buf=%d)", content_len, buf_size);
    }

    buf[read_len] = '\0';
    *out_buf = buf;
    if (out_len) *out_len = read_len;
    return ESP_OK;
}

/* --- Public: format own station MAC (uppercase colon separated) --- */
char *rc_format_own_mac(char *dst, size_t len) {
    if (len < 18) return NULL;
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(dst, len, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return dst;
}

/* Replace previous rc_http_fetch-based login with event-driven accumulation
 * (mirrors the old get_auth_token() pattern in sensornest.c).
 */

/* Accumulator for esp_http_client event handler */
typedef struct {
    char  *buf;
    size_t cap;
    size_t len;
} rc_resp_accum_t;

static esp_err_t rc_http_event_handler(esp_http_client_event_t *evt) {
    if (!evt || !evt->user_data) return ESP_OK;
    rc_resp_accum_t *acc = (rc_resp_accum_t *)evt->user_data;
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (evt->data_len > 0 && acc->len + evt->data_len < acc->cap) {
                memcpy(acc->buf + acc->len, evt->data, evt->data_len);
                acc->len += evt->data_len;
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            if (acc->len < acc->cap) acc->buf[acc->len] = 0;
            else acc->buf[acc->cap - 1] = 0;
            break;
        default:
            break;
    }
    return ESP_OK;
}

/* Debug helper to log a safe snippet of a (possibly large) PEM */
static void rc_log_cert_snippet(void) {
    return;
    if (!cloud_ca_pem) {
        ESP_LOGW(RC_TAG, "cert pointer NULL");
        return;
    }
    char snippet[81];
    size_t n = strlen(cloud_ca_pem);
    if (n > 80) n = 80;
    memcpy(snippet, cloud_ca_pem, n);
    snippet[n] = 0;
    ESP_LOGI(RC_TAG, "Root CA first %u chars:\n%s", (unsigned)n, snippet);
}

static esp_err_t rc_perform_login(const char *email, const char *password, bool store_creds);

esp_err_t rc_login(const char *email, const char *password, bool use_cache) {
    if (use_cache && rc_has_token()) {
        ESP_LOGI(RC_TAG, "rc_login: Using cached token.");
        return ESP_OK;
    }
    return rc_perform_login(email, password, true);
}

static esp_err_t rc_force_reauth(void) {
    char email[128] = {0};
    char pass[128]  = {0};
    if (!rc_load_creds(email, sizeof(email), pass, sizeof(pass))) {
        ESP_LOGE(RC_TAG, "force_reauth: stored credentials missing");
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGW(RC_TAG, "force_reauth: performing login with stored credentials");
    return rc_perform_login(email, pass, false);
}

static esp_err_t rc_perform_login(const char *email, const char *password, bool store_creds) {
    if (!email || !password) return ESP_ERR_INVALID_ARG;
    rc_log_cert_snippet();

    const int max_attempts = 3;
    int attempt = 0;
    esp_err_t err = ESP_FAIL;
    int http_status = 0;

    for (attempt = 1; attempt <= max_attempts; ++attempt) {
        rc_resp_accum_t acc = { .cap = 2048, .len = 0 };
        acc.buf = calloc(1, acc.cap);
        if (!acc.buf) return ESP_ERR_NO_MEM;

        esp_http_client_config_t cfg = {
            .url = CLOUD_LOGIN_URL,
            .method = HTTP_METHOD_POST,
            .cert_pem = cloud_ca_pem,
            .event_handler = rc_http_event_handler,
            .user_data = &acc,
            .timeout_ms = 8000,            // reduced
            .buffer_size = 1024,
            .buffer_size_tx = 1024,
            .skip_cert_common_name_check = true   // (optional: allow if CN mismatch; keep if needed)
        };

        ESP_LOGI(RC_TAG, "login: attempt %d/%d POST %s", attempt, max_attempts, CLOUD_LOGIN_URL);
        esp_http_client_handle_t client = esp_http_client_init(&cfg);
        if (!client) {
            free(acc.buf);
            return ESP_FAIL;
        }

        char body[256];
        snprintf(body, sizeof(body), "{\"email\":\"%s\",\"password\":\"%s\"}", email, password);
        esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_header(client, "Accept", "application/json");
        esp_http_client_set_post_field(client, body, strlen(body));

        err = esp_http_client_perform(client);
        http_status = esp_http_client_get_status_code(client);
        int64_t cl = esp_http_client_get_content_length(client);
        bool chunked = esp_http_client_is_chunked_response(client);

        if (acc.len < acc.cap) acc.buf[acc.len] = 0;
        else acc.buf[acc.cap - 1] = 0;

        ESP_LOGI(RC_TAG, "login: perform result=%s http_status=%d content_len=%lld chunked=%d",
                 esp_err_to_name(err), http_status, (long long)cl, chunked);

        // Snapshot a body snippet
        if (acc.len) {
            char snippet[161];
            size_t n = acc.len < 160 ? acc.len : 160;
            memcpy(snippet, acc.buf, n);
            snippet[n] = 0;
            ESP_LOGD(RC_TAG, "login: body snippet: %s", snippet);
        }

        if (err != ESP_OK) {
            esp_http_client_cleanup(client);
            free(acc.buf);
            if (err == ESP_ERR_HTTP_CONNECT || err == ESP_ERR_HTTP_EAGAIN) {
                if (attempt < max_attempts) {
                    int backoff_ms = 1000 << (attempt - 1); // 1000,2000,4000
                    ESP_LOGW(RC_TAG, "login: transient connect error (%s); backoff %d ms",
                             esp_err_to_name(err), backoff_ms);
                    vTaskDelay(pdMS_TO_TICKS(backoff_ms));
                    continue;
                }
            }
            return err;
        }

        // Completed HTTP transaction
        if (http_status == 200 || http_status == 201) {
            cJSON *root = cJSON_Parse(acc.buf);
            if (!root) {
                ESP_LOGE(RC_TAG, "login: JSON parse failed");
                esp_http_client_cleanup(client);
                free(acc.buf);
                return ESP_FAIL;
            }
            cJSON *acc_tok = cJSON_GetObjectItem(root, "access_token");
            if (!cJSON_IsString(acc_tok)) {
                ESP_LOGE(RC_TAG, "login: access_token missing");
                cJSON_Delete(root);
                esp_http_client_cleanup(client);
                free(acc.buf);
                return ESP_FAIL;
            }
            bool stored = rc_store_auth(email, store_creds ? password : NULL, acc_tok->valuestring);
            ESP_LOGI(RC_TAG, "login: token length=%d stored=%d",
                     (int)strlen(acc_tok->valuestring), stored);
            cJSON_Delete(root);
            esp_http_client_cleanup(client);
            free(acc.buf);
            return stored ? ESP_OK : ESP_FAIL;
        }

        if (http_status == 401) {
            ESP_LOGW(RC_TAG, "login: invalid credentials (401)");
            esp_http_client_cleanup(client);
            free(acc.buf);
            return ESP_ERR_INVALID_STATE; // signal invalid credentials
        }

        // Other server errors -> maybe retry
        esp_http_client_cleanup(client);
        free(acc.buf);
        if (http_status >= 500 && attempt < max_attempts) {
            int backoff_ms = 1000 << (attempt - 1);
            ESP_LOGW(RC_TAG, "login: server error %d -> retry in %d ms", http_status, backoff_ms);
            vTaskDelay(pdMS_TO_TICKS(backoff_ms));
            continue;
        }
        ESP_LOGE(RC_TAG, "login: failed with HTTP status %d (no retry)", http_status);
        return ESP_FAIL;
    }

    return err;
}

/* --- Fetch device by MAC (NULL => use own) 
 * out_json: receives raw JSON body (null-terminated)
 * Caller owns buffer (malloc). */
esp_err_t rc_get_device_by_mac(const char *mac, char **out_json) {
    if (!out_json) return ESP_ERR_INVALID_ARG;
    *out_json = NULL;

    char token[512];
    if (!rc_get_token(token, sizeof(token))) {
        ESP_LOGW(RC_TAG, "device fetch: no token (call rc_login first)");
        return ESP_ERR_INVALID_STATE;
    }
retry_auth:
    char mac_buf[32];
    if (!mac || !mac[0]) {
        if (!rc_format_own_mac(mac_buf, sizeof(mac_buf))) return ESP_FAIL;
        mac = mac_buf;
    }

    char url[256];
    snprintf(url, sizeof(url), "%s%s", CLOUD_DEVICE_MAC_URL, mac);

    rc_resp_accum_t acc = { .buf = malloc(4096), .cap = 4096, .len = 0 };
    if (!acc.buf) return ESP_ERR_NO_MEM;

    esp_err_t err = ESP_FAIL;
    int status = 0;
    for (int attempt = 1; attempt <= 3; ++attempt) {
        acc.len = 0;
        esp_http_client_config_t cfg = {
            .url = url,
            .method = HTTP_METHOD_GET,
            .cert_pem = cloud_ca_pem,
            .timeout_ms = 15000,
            .event_handler = rc_http_event_handler,
            .user_data = &acc,
            .keep_alive_enable = true,
        };
        esp_http_client_handle_t client = esp_http_client_init(&cfg);
        if (!client) { free(acc.buf); return ESP_FAIL; }

        char auth[600];
        snprintf(auth, sizeof(auth), "Bearer %s", token);
        esp_http_client_set_header(client, "Authorization", auth);

        err = esp_http_client_perform(client);
        status = esp_http_client_get_status_code(client);
        if (acc.len < acc.cap) acc.buf[acc.len] = 0; else acc.buf[acc.cap - 1] = 0;
        esp_http_client_cleanup(client);

        if (err == ESP_ERR_HTTP_CONNECT) {
            ESP_LOGW(RC_TAG, "device fetch connect timeout (attempt %d/3)", attempt);
            if (attempt < 3) vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }
        break; // either success or non-connect error
    }

    if (err != ESP_OK) {
        ESP_LOGE(RC_TAG, "device fetch HTTP error: %s", esp_err_to_name(err));
        free(acc.buf);
        return err;
    }
    if (status == 200) {
        *out_json = acc.buf;
        ESP_LOGI(RC_TAG, "device fetch success (%d bytes)", (int)acc.len);
        return ESP_OK;
    }
    if (status == 401) {
        ESP_LOGW(RC_TAG, "device fetch 401 -> re-auth & retry");
        free(acc.buf);
        if (rc_force_reauth() == ESP_OK && rc_get_token(token, sizeof(token))) {
            goto retry_auth;
        }
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGW(RC_TAG, "device fetch status=%d body=%s", status, acc.buf ? acc.buf : "");
    free(acc.buf);
    if (status == 404) return ESP_ERR_NOT_FOUND;
    if (status == 401) return ESP_ERR_INVALID_STATE;
    return (status == 0) ? ESP_ERR_HTTP_CONNECT : ESP_FAIL;
}

/* --- NEW: Fetch all meshes for the authenticated user --- */
esp_err_t rc_get_meshes(char **out_json) {
    if (!out_json) return ESP_ERR_INVALID_ARG;
    *out_json = NULL;
    char token[512];
    if (!rc_get_token(token, sizeof(token))) {
        ESP_LOGW(RC_TAG, "meshes fetch: no token (call rc_login first)");
        return ESP_ERR_INVALID_STATE;
    }
retry_auth:
    rc_resp_accum_t acc = { .buf = malloc(8192), .cap = 8192, .len = 0 };
    if (!acc.buf) return ESP_ERR_NO_MEM;

    esp_err_t err = ESP_FAIL;
    int status = 0;
    for (int attempt = 1; attempt <= 3; ++attempt) {
        acc.len = 0;
        esp_http_client_config_t cfg = {
            .url = CLOUD_MESHES_URL,
            .method = HTTP_METHOD_GET,
            .cert_pem = cloud_ca_pem,
            .timeout_ms = 15000,
            .event_handler = rc_http_event_handler,
            .user_data = &acc,
            .keep_alive_enable = true,
        };
        esp_http_client_handle_t client = esp_http_client_init(&cfg);
        if (!client) { free(acc.buf); return ESP_FAIL; }

        char auth[600];
        snprintf(auth, sizeof(auth), "Bearer %s", token);
        esp_http_client_set_header(client, "Authorization", auth);

        err = esp_http_client_perform(client);
        status = esp_http_client_get_status_code(client);
        if (acc.len < acc.cap) acc.buf[acc.len] = 0; else acc.buf[acc.cap - 1] = 0;
        esp_http_client_cleanup(client);

        if (err == ESP_ERR_HTTP_CONNECT) {
            ESP_LOGW(RC_TAG, "meshes fetch connect timeout (attempt %d/3)", attempt);
            if (attempt < 3) vTaskDelay(pdMS_TO_TICKS(1500));
            continue;
        }
        break;
    }

    if (err != ESP_OK) {
        ESP_LOGE(RC_TAG, "meshes fetch HTTP error: %s", esp_err_to_name(err));
        free(acc.buf);
        return err;
    }
    if (status == 200) {
        *out_json = acc.buf;
        ESP_LOGI(RC_TAG, "meshes fetch success (%d bytes)", (int)acc.len);
        return ESP_OK;
    }
    if (status == 401) {
        ESP_LOGW(RC_TAG, "meshes fetch 401 -> re-auth & retry");
        free(acc.buf);
        if (rc_force_reauth() == ESP_OK && rc_get_token(token, sizeof(token))) {
            goto retry_auth;
        }
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGW(RC_TAG, "meshes fetch status=%d body=%s", status, acc.buf ? acc.buf : "");
    free(acc.buf);
    return (status == 0) ? ESP_ERR_HTTP_CONNECT : ESP_FAIL;
}

/* --- NEW: Update the current device on the cloud (name/readings/wake/mesh/role) --- */
esp_err_t rc_update_device(const char *name, int readingsPerBatch, int wakeUpInterval,
                           const char *mesh_id, const char *role) {
     // At least one arg should be provided
    if ((!name || !name[0]) && readingsPerBatch <= 0 && wakeUpInterval <= 0 &&
        (!mesh_id || !mesh_id[0]) && (!role || !role[0])) {
         return ESP_ERR_INVALID_ARG;
     }

    char *device_json = NULL;
    char device_id[40] = {0};

    // 1. Get the full device object to find its ID
    esp_err_t err = rc_get_device_by_mac(NULL, &device_json);
    if (err != ESP_OK) {
        ESP_LOGE(RC_TAG, "update_device: could not get own device details first. err=%s", esp_err_to_name(err));
        return err;
    }
    if (!device_json) return ESP_FAIL;

    cJSON *root = cJSON_Parse(device_json);
    if (root) {
        cJSON *j_id = cJSON_GetObjectItem(root, "id");
        if (cJSON_IsString(j_id)) {
            strlcpy(device_id, j_id->valuestring, sizeof(device_id));
        }
        cJSON_Delete(root);
    }
    free(device_json);

    if (!device_id[0]) {
        ESP_LOGE(RC_TAG, "update_device: could not parse device ID from cloud response.");
        return ESP_FAIL;
    }

    // Build JSON body with only provided fields
    cJSON *body_obj = cJSON_CreateObject();
    if (!body_obj) return ESP_ERR_NO_MEM;
    if (name && name[0]) cJSON_AddStringToObject(body_obj, "name", name);
    if (readingsPerBatch > 0) cJSON_AddNumberToObject(body_obj, "readingsPerBatch", readingsPerBatch);
    if (wakeUpInterval > 0) cJSON_AddNumberToObject(body_obj, "wakeUpInterval", wakeUpInterval);
    if (mesh_id && mesh_id[0]) cJSON_AddStringToObject(body_obj, "meshId", mesh_id);
    if (role && role[0]) cJSON_AddStringToObject(body_obj, "role", role);

    char *body = cJSON_PrintUnformatted(body_obj);
    cJSON_Delete(body_obj);
    if (!body) return ESP_ERR_NO_MEM;

    char token[512];
    if (!rc_get_token(token, sizeof(token))) {
        free(body);
        return ESP_ERR_INVALID_STATE;
    }

retry_once:
    char url[256];
    snprintf(url, sizeof(url), "%s%s", CLOUD_DEVICE_UPDATE_URL, device_id);

    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_PATCH,
        .cert_pem = cloud_ca_pem,
        .timeout_ms = 10000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) { free(body); return ESP_FAIL; }

    char auth[600];
    snprintf(auth, sizeof(auth), "Bearer %s", token);
    esp_http_client_set_header(client, "Authorization", auth);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body, strlen(body));

    err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    free(body);

    if (err != ESP_OK) {
        ESP_LOGE(RC_TAG, "update_device: HTTP PATCH error: %s", esp_err_to_name(err));
        return err;
    }
    if (status >= 200 && status < 300) {
        ESP_LOGI(RC_TAG, "update_device: success. Device %s updated", device_id);
        return ESP_OK;
    }
    if (status == 401) {
        ESP_LOGW(RC_TAG, "update_device: 401 -> re-auth & retry");
        if (rc_force_reauth() == ESP_OK && rc_get_token(token, sizeof(token))) {
            goto retry_once;
        }
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGE(RC_TAG, "update_device: failed with HTTP status %d", status);
    if (status == 404) return ESP_ERR_NOT_FOUND;
    return ESP_FAIL;
}

/* --- NEW: Register the current device with the cloud --- */
esp_err_t rc_register_device(const char *name, const char *role, const char *mesh_id, int readingsPerBatch, int wakeUpInterval, int x, int y) {
    if (!name || !role || !mesh_id) return ESP_ERR_INVALID_ARG;

    char token[512];
    if (!rc_get_token(token, sizeof(token))) {
        return ESP_ERR_INVALID_STATE; // Not logged in
    }

    char mac_str[18];
    rc_format_own_mac(mac_str, sizeof(mac_str));

    esp_err_t err = ESP_FAIL;

retry_once:
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", name);
    cJSON_AddStringToObject(root, "role", role);
    cJSON_AddStringToObject(root, "meshId", mesh_id);
    cJSON_AddStringToObject(root, "macAddress", mac_str);
    cJSON_AddStringToObject(root, "status", "ACTIVE");
    cJSON_AddNumberToObject(root, "readingsPerBatch", readingsPerBatch);
    cJSON_AddNumberToObject(root, "wakeUpInterval", wakeUpInterval);
    cJSON_AddNumberToObject(root, "x", x);
    cJSON_AddNumberToObject(root, "y", y);
    char *body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!body) return ESP_ERR_NO_MEM;

    rc_resp_accum_t acc = { .buf = malloc(1024), .cap = 1024, .len = 0 };
    if (!acc.buf) {
        free(body);
        return ESP_ERR_NO_MEM;
    }

    esp_http_client_config_t cfg = {
        .url = CLOUD_DEVICE_REGISTER_URL,
        .method = HTTP_METHOD_POST,
        .cert_pem = cloud_ca_pem,
        .timeout_ms = 20000,
        .event_handler = rc_http_event_handler,
        .user_data = &acc,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        free(body);
        return ESP_FAIL;
    }

    char auth[600];
    snprintf(auth, sizeof(auth), "Bearer %s", token);
    esp_http_client_set_header(client, "Authorization", auth);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body, strlen(body));

    err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    if (acc.len > 0) {
        acc.buf[acc.len] = 0;
        ESP_LOGI(RC_TAG, "register_device: response body: %s", acc.buf);
    }
    esp_http_client_cleanup(client);
    free(body);

    if (err != ESP_OK) {
        ESP_LOGE(RC_TAG, "register_device: HTTP POST error: %s", esp_err_to_name(err));
        return err;
    }

    if (status == 201) {
        ESP_LOGI(RC_TAG, "register_device: success");
        return ESP_OK;
    }

    if (status == 401) {
        ESP_LOGW(RC_TAG, "register_device: 401 -> re-auth & retry");
        free(acc.buf);
        if (rc_force_reauth() == ESP_OK && rc_get_token(token, sizeof(token))) {
            goto retry_once;
        }
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGE(RC_TAG, "register_device: failed with HTTP status %d", status);
    free(acc.buf);
    return ESP_FAIL;
}


/* OPTIONAL: convenience to extract selected config fields (not required yet) */
esp_err_t rc_extract_device_config(const char *device_json,
                                   int *readingsPerBatch,
                                   int *wakeUpInterval) {
    if (!device_json) return ESP_ERR_INVALID_ARG;
    cJSON *root = cJSON_Parse(device_json);
    if (!root) return ESP_FAIL;
    if (readingsPerBatch) {
        cJSON *n = cJSON_GetObjectItem(root, "readingsPerBatch");
        *readingsPerBatch = cJSON_IsNumber(n) ? n->valueint : -1;
    }
    if (wakeUpInterval) {
        cJSON *n = cJSON_GetObjectItem(root, "wakeUpInterval");
        *wakeUpInterval = cJSON_IsNumber(n) ? n->valueint : -1;
    }
    cJSON_Delete(root);
    return ESP_OK;
}

/* (Optional) Clear stored token */
esp_err_t rc_clear_token(void) {
    nvs_handle_t h;
    if (nvs_open(NVS_NS_AUTH, NVS_READWRITE, &h) != ESP_OK) return ESP_FAIL;
    nvs_erase_key(h, NVS_KEY_TOKEN);
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(RC_TAG, "token cleared");
    return ESP_OK;
}

/* Simple self-test (can be invoked manually) */
void rc_debug_print_token(void) {
    char t[64];
    if (rc_get_token(t, sizeof(t)))
        ESP_LOGI(RC_TAG, "stored token (truncated): %.32s...", t);
    else
        ESP_LOGI(RC_TAG, "no stored token");
}

/* Post a readings batch (expects payload like {"data":[...]} ) */
esp_err_t rc_post_readings(const char *json_payload) {
    if (!json_payload || !json_payload[0]) return ESP_ERR_INVALID_ARG;

    char token[512];
    if (!rc_get_token(token, sizeof(token))) {
        ESP_LOGE(RC_TAG, "post_readings: no stored token");
        return ESP_ERR_INVALID_STATE;
    }
    int retry = 0;
retry_send:
    esp_http_client_config_t cfg = {
        .url = CLOUD_READINGS_URL,
        .method = HTTP_METHOD_POST,
        .cert_pem = cloud_ca_pem,
        .timeout_ms = 10000
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return ESP_FAIL;
    char auth[600];
    snprintf(auth, sizeof(auth), "Bearer %s", token);
    esp_http_client_set_header(client, "Authorization", auth);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_payload, strlen(json_payload));
    ESP_LOGI(RC_TAG, "post_readings: POST %s len=%d", CLOUD_READINGS_URL, (int)strlen(json_payload));
    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    int content_len = esp_http_client_get_content_length(client);
    if (content_len < 0) content_len = 0;
    int read_cap = content_len > 0 && content_len < 512 ? content_len : 512;
    char resp[513];
    int r = esp_http_client_read_response(client, resp, read_cap);
    if (r < 0) r = 0;
    resp[r] = 0;
    ESP_LOGI(RC_TAG, "post_readings: http_status=%d resp_snippet='%s'", status, resp);
    esp_http_client_cleanup(client);
    if (err != ESP_OK) {
        ESP_LOGE(RC_TAG, "post_readings: perform err=%s", esp_err_to_name(err));
        return err;
    }
    if (status == 200 || status == 201) return ESP_OK;
    if (status >= 400 && status < 500) {
        ESP_LOGW(RC_TAG, "post_readings: server rejected payload (status=%d). Consider checking field names.", status);
    }
    if (status == 401 && retry == 0) {
        ESP_LOGW(RC_TAG, "post_readings: 401 -> re-auth & retry");
        if (rc_force_reauth() == ESP_OK && rc_get_token(token, sizeof(token))) {
            retry++;
            goto retry_send;
        }
        return ESP_ERR_INVALID_STATE;
    }
    if (status == 401) return ESP_ERR_INVALID_STATE;
    return ESP_FAIL;
}