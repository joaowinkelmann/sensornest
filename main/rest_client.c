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
#include "certs.h"

static const char *RC_TAG = "rest_client";
extern const char *winkels_root_ca_pem;

#define CLOUD_BASE_URL          "https://winkels.com.br/tempmesh/api"
#define CLOUD_LOGIN_URL         CLOUD_BASE_URL "/auth/login/"
#define CLOUD_DEVICE_MAC_URL    CLOUD_BASE_URL "/devices/mac/"   /* append AA:BB:CC:DD:EE:FF */
#define CLOUD_READINGS_URL      CLOUD_BASE_URL "/readings/add/"   /* POST */

#define NVS_NS_AUTH             "auth"
#define NVS_KEY_TOKEN           "token"
#define NVS_KEY_EMAIL           "email"

/* --- NVS helpers --- */
static bool rc_store_token(const char *email, const char *token) {
    if (!token || !token[0]) return false;
    nvs_handle_t h;
    if (nvs_open(NVS_NS_AUTH, NVS_READWRITE, &h) != ESP_OK) return false;
    esp_err_t err = nvs_set_str(h, NVS_KEY_TOKEN, token);
    if (err == ESP_OK && email && email[0]) err = nvs_set_str(h, NVS_KEY_EMAIL, email);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err == ESP_OK;
}

bool rc_get_token(char *out, size_t len) {
    if (!out || !len) return false;
    out[0] = 0;
    nvs_handle_t h;
    if (nvs_open(NVS_NS_AUTH, NVS_READONLY, &h) != ESP_OK) return false;
    size_t sz = len;
    if (nvs_get_str(h, NVS_KEY_TOKEN, out, &sz) != ESP_OK) {
        nvs_close(h);
        return false;
    }
    nvs_close(h);
    return true;
}

bool rc_has_token(void) {
    char tmp[16];
    return rc_get_token(tmp, sizeof(tmp));
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
 * (mirrors the old get_auth_token() pattern in tempmesh.c).
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
    if (!winkels_root_ca_pem) {
        ESP_LOGW(RC_TAG, "cert pointer NULL");
        return;
    }
    char snippet[81];
    size_t n = strlen(winkels_root_ca_pem);
    if (n > 80) n = 80;
    memcpy(snippet, winkels_root_ca_pem, n);
    snippet[n] = 0;
    ESP_LOGI(RC_TAG, "Root CA first %u chars:\n%s", (unsigned)n, snippet);
}

esp_err_t rc_login(const char *email, const char *password) {
    if (!email || !password) return ESP_ERR_INVALID_ARG;

    rc_log_cert_snippet();

    /* Allocate response buffer (enough for current login JSON ~ <512B) */
    rc_resp_accum_t acc = {
        .cap = 1024,
        .len = 0
    };
    acc.buf = calloc(1, acc.cap);
    if (!acc.buf) return ESP_ERR_NO_MEM;

    esp_http_client_config_t cfg = {
        .url = CLOUD_LOGIN_URL,
        .method = HTTP_METHOD_POST,
        .cert_pem = winkels_root_ca_pem,   // using extern from certs.c
        .event_handler = rc_http_event_handler,
        .user_data = &acc,
        .timeout_ms = 10000,
        .buffer_size = 1024,      // RX internal buffer
        .buffer_size_tx = 1024    // TX internal buffer
    };

    ESP_LOGI(RC_TAG, "login: POST %s", CLOUD_LOGIN_URL);
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        free(acc.buf);
        return ESP_FAIL;
    }

    char body[256];
    snprintf(body, sizeof(body), "{\"email\":\"%s\",\"password\":\"%s\"}", email, password);
    ESP_LOGI(RC_TAG, "login: request body (%d bytes): %s", (int)strlen(body), body);

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body, strlen(body));

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    int64_t cl = esp_http_client_get_content_length(client);
    bool chunked = esp_http_client_is_chunked_response(client);
    ESP_LOGI(RC_TAG, "login: perform status=%s http_status=%d content_len=%lld chunked=%d",
             esp_err_to_name(err), status, (long long)cl, chunked);

    /* Force termination if handler didn't receive FINISH (defensive) */
    if (acc.len < acc.cap) acc.buf[acc.len] = 0; else acc.buf[acc.cap - 1] = 0;

    if (err != ESP_OK) {
        ESP_LOGE(RC_TAG, "login: perform failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        free(acc.buf);
        return err;
    }

    /* Log raw response (truncate if large) */
    if (acc.len == 0) {
        ESP_LOGW(RC_TAG, "login: response body empty (status=%d)", status);
    } else {
        int shown = acc.len < 400 ? acc.len : 400;
        ESP_LOGI(RC_TAG, "login: raw response (%d bytes, showing %d): %.*s",
                 (int)acc.len, shown, shown, acc.buf);
    }

    if (status != 200 && status != 201) {
        ESP_LOGW(RC_TAG, "login: unexpected HTTP status=%d", status);
        esp_http_client_cleanup(client);
        free(acc.buf);
        return ESP_FAIL;
    }

    if (acc.len == 0) {
        ESP_LOGE(RC_TAG, "login: empty body on 200 -> server anomaly or read issue");
        esp_http_client_cleanup(client);
        free(acc.buf);
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(acc.buf);
    if (!root) {
        ESP_LOGE(RC_TAG, "login: JSON parse failed (len=%d)", (int)acc.len);
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

    bool stored = rc_store_token(email, acc_tok->valuestring);
    ESP_LOGI(RC_TAG, "login: token length=%d stored=%d",
             (int)strlen(acc_tok->valuestring), stored);

    cJSON_Delete(root);
    esp_http_client_cleanup(client);
    free(acc.buf);

    return stored ? ESP_OK : ESP_FAIL;
}

/* --- Fetch device by MAC (NULL => use own) 
 * out_json: receives raw JSON body (null-terminated)
 * Caller owns buffer (malloc). */
esp_err_t rc_get_device_by_mac(const char *mac, char **out_json) {
    if (!out_json) return ESP_ERR_INVALID_ARG;
    *out_json = NULL;

    char token[512];
    if (!rc_get_token(token, sizeof(token))) {
        ESP_LOGW(RC_TAG, "no token stored");
        return ESP_ERR_INVALID_STATE;
    }

    char mac_buf[32];
    if (!mac || !mac[0]) {
        if (!rc_format_own_mac(mac_buf, sizeof(mac_buf))) return ESP_FAIL;
        mac = mac_buf;
    }

    char url[256];
    snprintf(url, sizeof(url), "%s%s", CLOUD_DEVICE_MAC_URL, mac);

    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .cert_pem = winkels_root_ca_pem,
        .timeout_ms = 10000
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return ESP_FAIL;

    char auth[600];
    snprintf(auth, sizeof(auth), "Bearer %s", token);
    esp_http_client_set_header(client, "Authorization", auth);

    char *resp = NULL;
    int rlen = 0;
    esp_err_t err = rc_http_fetch(client, &resp, &rlen, 4096);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        ESP_LOGE(RC_TAG, "device fetch HTTP error: %s", esp_err_to_name(err));
        free(resp);
        return err;
    }
    if (status == 200) {
        *out_json = resp;
        ESP_LOGI(RC_TAG, "device fetch success (%d bytes)", rlen);
        return ESP_OK;
    }
    ESP_LOGW(RC_TAG, "device fetch status=%d body=%s", status, resp ? resp : "");
    free(resp);
    if (status == 404) return ESP_ERR_NOT_FOUND;
    if (status == 401) return ESP_ERR_INVALID_STATE;
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

    esp_http_client_config_t cfg = {
        .url = CLOUD_READINGS_URL,
        .method = HTTP_METHOD_POST,
        .cert_pem = winkels_root_ca_pem,
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

    if (err != ESP_OK) {
        ESP_LOGE(RC_TAG, "post_readings: perform err=%s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    int content_len = esp_http_client_get_content_length(client);
    if (content_len < 0) content_len = 0;
    int read_cap = content_len > 0 && content_len < 512 ? content_len : 512;
    char resp[513];
    int r = esp_http_client_read_response(client, resp, read_cap);
    if (r < 0) r = 0;
    resp[r] = 0;

    ESP_LOGI(RC_TAG, "post_readings: http_status=%d resp_snippet='%s'", status, resp);

    esp_http_client_cleanup(client);

    if (status == 200 || status == 201) return ESP_OK;
    if (status == 401) return ESP_ERR_INVALID_STATE;
    return ESP_FAIL;
}