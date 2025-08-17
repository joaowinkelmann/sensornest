#include <stdio.h>
#include <string.h>
#include <stdlib.h> // Required for malloc/free
#include "time.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_random.h"
#include "esp_sntp.h"
#include "cJSON.h"
#include "dht.h"
#include <stdint.h> // added for int16_t
#include "esp_mac.h"
#include "esp_littlefs.h"
#include "nvs.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_timer.h"

// Pin config
#define DHT_TYPE DHT_TYPE_DHT11
#define DHT_GPIO 10

// --- WiFi connection logic (declarations moved up for visibility) ---
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);


// --- WiFi Configuration ---
#define WIFI_SSID      "Winkelmann"
#define WIFI_PASSWORD  "jr173806"

// Factory reset declarations START
#define BOOT_BTN_GPIO           GPIO_NUM_0     // BOOT button (IO0)
#define RESET_HOLD_MS           5000           // hold BOOT for 5s to reset
#define DOUBLE_RESET_WINDOW_MS  5000           // two resets within 5s

RTC_DATA_ATTR static uint32_t dr_count = 0;
RTC_DATA_ATTR static int64_t  dr_last_ms = 0;

static void check_double_reset(void);
static void factory_reset_button_task(void *pvParameters);
static void do_factory_reset(void);
// Factory reset declarations END

static const char *TAG = "MAIN_APP";
static char access_token[512] = {0};

static const char *winkels_root_ca_pem = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIFBTCCAu2gAwIBAgIQS6hSk/eaL6JzBkuoBI110DANBgkqhkiG9w0BAQsFADBP\n" \
"MQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJuZXQgU2VjdXJpdHkgUmVzZWFy\n" \
"Y2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBYMTAeFw0yNDAzMTMwMDAwMDBa\n" \
"Fw0yNzAzMTIyMzU5NTlaMDMxCzAJBgNVBAYTAlVTMRYwFAYDVQQKEw1MZXQncyBF\n" \
"bmNyeXB0MQwwCgYDVQQDEwNSMTAwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEK\n" \
"AoIBAQDPV+XmxFQS7bRH/sknWHZGUCiMHT6I3wWd1bUYKb3dtVq/+vbOo76vACFL\n" \
"YlpaPAEvxVgD9on/jhFD68G14BQHlo9vH9fnuoE5CXVlt8KvGFs3Jijno/QHK20a\n" \
"/6tYvJWuQP/py1fEtVt/eA0YYbwX51TGu0mRzW4Y0YCF7qZlNrx06rxQTOr8IfM4\n" \
"FpOUurDTazgGzRYSespSdcitdrLCnF2YRVxvYXvGLe48E1KGAdlX5jgc3421H5KR\n" \
"mudKHMxFqHJV8LDmowfs/acbZp4/SItxhHFYyTr6717yW0QrPHTnj7JHwQdqzZq3\n" \
"DZb3EoEmUVQK7GH29/Xi8orIlQ2NAgMBAAGjgfgwgfUwDgYDVR0PAQH/BAQDAgGG\n" \
"MB0GA1UdJQQWMBQGCCsGAQUFBwMCBggrBgEFBQcDATASBgNVHRMBAf8ECDAGAQH/\n" \
"AgEAMB0GA1UdDgQWBBS7vMNHpeS8qcbDpHIMEI2iNeHI6DAfBgNVHSMEGDAWgBR5\n" \
"tFnme7bl5AFzgAiIyBpY9umbbjAyBggrBgEFBQcBAQQmMCQwIgYIKwYBBQUHMAKG\n" \
"Fmh0dHA6Ly94MS5pLmxlbmNyLm9yZy8wEwYDVR0gBAwwCjAIBgZngQwBAgEwJwYD\n" \
"VR0fBCAwHjAcoBqgGIYWaHR0cDovL3gxLmMubGVuY3Iub3JnLzANBgkqhkiG9w0B\n" \
"AQsFAAOCAgEAkrHnQTfreZ2B5s3iJeE6IOmQRJWjgVzPw139vaBw1bGWKCIL0vIo\n" \
"zwzn1OZDjCQiHcFCktEJr59L9MhwTyAWsVrdAfYf+B9haxQnsHKNY67u4s5Lzzfd\n" \
"u6PUzeetUK29v+PsPmI2cJkxp+iN3epi4hKu9ZzUPSwMqtCceb7qPVxEbpYxY1p9\n" \
"1n5PJKBLBX9eb9LU6l8zSxPWV7bK3lG4XaMJgnT9x3ies7msFtpKK5bDtotij/l0\n" \
"GaKeA97pb5uwD9KgWvaFXMIEt8jVTjLEvwRdvCn294GPDF08U8lAkIv7tghluaQh\n" \
"1QnlE4SEN4LOECj8dsIGJXpGUk3aU3KkJz9icKy+aUgA+2cP21uh6NcDIS3XyfaZ\n" \
"QjmDQ993ChII8SXWupQZVBiIpcWO4RqZk3lr7Bz5MUCwzDIA359e57SSq5CCkY0N\n" \
"4B6Vulk7LktfwrdGNVI5BsC9qqxSwSKgRJeZ9wygIaehbHFHFhcBaMDKpiZlBHyz\n" \
"rsnnlFXCb5s8HKn5LsUgGvB24L7sGNZP2CX7dhHov+YhD+jozLW2p9W4959Bz2Ei\n" \
"RmqDtmiXLnzqTpXbI+suyCsohKRg6Un0RC47+cpiVwHiXZAW+cn8eiNIjqbVgXLx\n" \
"KPpdzvvtTnOPlC7SQZSYmdunr3Bf9b77AiC/ZidstK36dRILKz7OA54=\n" \
"-----END CERTIFICATE-----\n";

// --- NEW: Struct to hold response data for the event handler ---
typedef struct {
    char *buffer;
    int buffer_size;
    int data_len;
} http_response_data_t;

// --- Function Prototypes ---
esp_err_t get_auth_token();
void send_sensor_data();
void authenticated_post_task(void *pvParameters);
static void set_setup_stage(const char *stage);
extern void captive_dns_task(void *pv); // from main/captive_dns.c
// static void wifi_init_sta(void); // no longer called directly in app_main

// --- NEW: prototypes for boot flow and REST server ---
esp_err_t start_rest_server(const char *base_path); // from rest_server.c
static bool wifi_boot_flow(void);
static bool load_wifi_credentials(char *out_ssid, size_t ssid_len, char *out_pass, size_t pass_len);
static bool connect_sta_with(const char *ssid, const char *pass);
static void start_softap_and_rest_server(void);
static esp_err_t mount_www_fs(void);

// --- Main Application Entry Point ---
void app_main(void) {
    nvs_flash_init(); // Simplified NVS init

    // +++ ADD: run double-reset detector early
    check_double_reset();

    // +++ ADD: start BOOT long-press watcher
    xTaskCreate(factory_reset_button_task, "factory_reset_btn", 2048, NULL, 3, NULL);

    // NEW: Try STA with saved credentials; if it fails or absent, start SoftAP+REST
    bool connected = wifi_boot_flow();

    if (connected) {
        // Network available: continue the existing flow
        // (Time sync and auth+data sending are already handled inside the task)
        xTaskCreate(&authenticated_post_task, "authenticated_post_task", 8192, NULL, 5, NULL);
    } else {
        ESP_LOGW(TAG, "Provisioning portal started in SoftAP mode. Connect to the AP and open http://192.168.4.1/");
    }
}

// --- NEW: Event handler specifically for the authentication request ---
esp_err_t _auth_http_event_handler(esp_http_client_event_t *evt) {
    http_response_data_t *response_data = (http_response_data_t *)evt->user_data;
    switch(evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            // Check if we have enough space in the buffer
            if (response_data->buffer_size - response_data->data_len >= evt->data_len) {
                memcpy(response_data->buffer + response_data->data_len, evt->data, evt->data_len);
                response_data->data_len += evt->data_len;
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            // Null-terminate the buffer to make it a valid string
            response_data->buffer[response_data->data_len] = '\0';
            break;
        default:
            break;
    }
    return ESP_OK;
}

// --- Main Task to Orchestrate Auth and Data Sending ---
void authenticated_post_task(void *pvParameters) {
    ESP_LOGI(TAG, "--- Starting Authenticated POST Task ---");
    
    // Time Sync Logic
    ESP_LOGI(TAG, "Synchronizing time with NTP...");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
    setenv("TZ", "GMT+3", 1);
    tzset();
    int retry = 0;
    while (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < 15) {
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    // Step 1: Authenticate and get the token
    esp_err_t auth_status = get_auth_token();

    // Step 2: If authentication was successful, send the sensor data
    if (auth_status == ESP_OK) {
        ESP_LOGI(TAG, "Authentication successful, proceeding to send sensor data.");
        send_sensor_data();
    } else {
        ESP_LOGE(TAG, "Authentication failed. Aborting task.");
    }

    ESP_LOGI(TAG, "--- Authenticated POST Task Finished ---");
    vTaskDelete(NULL);
}

// --- REVISED: Function to perform the authentication request ---
esp_err_t get_auth_token() {
    static const char *AUTH_TAG = "AUTH_CLIENT";
    const char *login_url = "https://winkels.com.br/tempmesh/api/auth/login/";
    const char *post_data = "{\"email\": \"test@example.com\", \"password\": \"password123\"}";
    
    // Prepare a buffer for the response
    http_response_data_t response_data = {0};
    response_data.buffer_size = 1024; // Allocate a 1KB buffer for the response
    response_data.buffer = malloc(response_data.buffer_size);
    if (response_data.buffer == NULL) {
        ESP_LOGE(AUTH_TAG, "Failed to allocate memory for response buffer");
        return ESP_FAIL;
    }

    esp_http_client_config_t config = {
        .url = login_url,
        .cert_pem = winkels_root_ca_pem,
        .event_handler = _auth_http_event_handler, // Use our new event handler
        .user_data = &response_data,             // Pass our buffer struct to the handler
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_err_t err = esp_http_client_perform(client);
    esp_err_t return_status = ESP_FAIL;

    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(AUTH_TAG, "Auth request status = %d", status_code);

        if (status_code == 201 || status_code == 200) {
            ESP_LOGI(AUTH_TAG, "Auth response body: %s", response_data.buffer);
            cJSON *json = cJSON_Parse(response_data.buffer);
            if (json) {
                cJSON *token_item = cJSON_GetObjectItem(json, "access_token");
                if (token_item && cJSON_IsString(token_item)) {
                    strncpy(access_token, token_item->valuestring, sizeof(access_token) - 1);
                    ESP_LOGI(AUTH_TAG, "Successfully extracted access token.");
                    return_status = ESP_OK;
                } else {
                    ESP_LOGE(AUTH_TAG, "Failed to find 'access_token' in JSON response.");
                }
                cJSON_Delete(json);
            } else {
                ESP_LOGE(AUTH_TAG, "Failed to parse JSON response.");
            }
        }
    } else {
        ESP_LOGE(AUTH_TAG, "Authentication request failed: %s", esp_err_to_name(err));
    }
    
    esp_http_client_cleanup(client);
    free(response_data.buffer); // Clean up the response buffer
    return return_status;
}

// --- Function to send the sensor data (replaced fake data with real DHT11 read) ---
void send_sensor_data() {
    static const char *DATA_TAG = "DATA_CLIENT";
    char json_payload[512];
    char time_buffer[30];
    time_t now;
    time(&now);
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);

    // Read DHT11 sensor
    int16_t humidity = 0;
    int16_t temperature = 0;
    esp_err_t res = dht_read_data(DHT_TYPE, DHT_GPIO, &humidity, &temperature);

    // Divide by 10, because dht_read_data returns temp and hum * 10
    temperature /= 10;
    humidity /= 10;

    if (res != ESP_OK) {
        ESP_LOGE(DATA_TAG, "DHT read failed: %s", esp_err_to_name(res));
        return;
    } else {
        ESP_LOGI(DATA_TAG, "DHT Read - Humidity: %d%%, Temperature: %dC", humidity, temperature);
    }

    // Note: for DHT11 driver returns integer values (no decimal). Adjust if using DHT22.
    float temp_f = (float)temperature;
    float hum_f = (float)humidity;

    snprintf(json_payload, sizeof(json_payload),
             "{\"data\":[{\"mac\":\"24:58:7C:CC:E5:B4\",\"temp\":%.2f,\"hum\":%.2f,\"lat\":-29.7178,\"lon\":-52.4267,\"readingTime\":\"%s\"}]}",
             temp_f, hum_f, time_buffer);
    ESP_LOGI(DATA_TAG, "Generated JSON Payload: %s", json_payload);

    esp_http_client_config_t config = {
        .url = "https://winkels.com.br/tempmesh/api/readings/add/",
        .cert_pem = winkels_root_ca_pem,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    char bearer_header[600];
    snprintf(bearer_header, sizeof(bearer_header), "Bearer %s", access_token);
    esp_http_client_set_header(client, "Authorization", bearer_header);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_payload, strlen(json_payload));

    ESP_LOGI(DATA_TAG, "Performing authenticated HTTPS POST...");
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(DATA_TAG, "✅ HTTPS POST successful, status = %d", esp_http_client_get_status_code(client));
    } else {
        ESP_LOGE(DATA_TAG, "❌ HTTPS POST failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

// --- NEW: Load Wi-Fi credentials from NVS ---
static bool load_wifi_credentials(char *out_ssid, size_t ssid_len, char *out_pass, size_t pass_len) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("wifi", NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "No 'wifi' namespace yet in NVS (err=%s)", esp_err_to_name(err));
        return false;
    }

    size_t ssz = ssid_len;
    size_t psz = pass_len;
    err = nvs_get_str(nvs, "ssid", out_ssid, &ssz);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "SSID not found in NVS");
        nvs_close(nvs);
        return false;
    }
    err = nvs_get_str(nvs, "pass", out_pass, &psz);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Password not found in NVS");
        nvs_close(nvs);
        return false;
    }
    nvs_close(nvs);
    ESP_LOGI(TAG, "Loaded Wi-Fi creds from NVS: SSID='%s'", out_ssid);
    return true;
}

// --- NEW: Try to connect in STA mode with provided credentials ---
static bool connect_sta_with(const char *ssid, const char *pass) {
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = { 0 };
    strlcpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Waiting for WiFi connection (STA)...");
    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(15000) // 15s timeout
    );

    bool ok = (bits & WIFI_CONNECTED_BIT);
    if (ok) {
        ESP_LOGI(TAG, "WiFi Connected (STA).");
        // --- NEW: mark connected ---
        set_setup_stage("connected");
    } else {
        ESP_LOGW(TAG, "WiFi connection failed or timed out.");
        esp_wifi_stop();
    }

    // Clean up handler instances (optional now, safer later)
    esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip);
    esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id);
    vEventGroupDelete(s_wifi_event_group);
    return ok;
}

// --- NEW: Start SoftAP with "ESP-{shortID}" and launch REST server ---
static void start_softap_and_rest_server(void) {
    ESP_LOGW(TAG, "Starting SoftAP for provisioning...");

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    // Create both AP and STA default interfaces so we can scan/connect while AP is up
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Generate AP SSID "ESP-<MAC tail>"
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    char ap_ssid[32];
    snprintf(ap_ssid, sizeof(ap_ssid), "ESP-%02X%02X%02X", mac[3], mac[4], mac[5]);

    wifi_config_t ap_config = (wifi_config_t){ 0 };
    strlcpy((char *)ap_config.ap.ssid, ap_ssid, sizeof(ap_config.ap.ssid));
    ap_config.ap.ssid_len = strlen(ap_ssid);
    ap_config.ap.channel = 1;
    ap_config.ap.max_connection = 4;
    ap_config.ap.authmode = WIFI_AUTH_OPEN;

    // IMPORTANT: APSTA so we can scan via STA while AP remains accessible
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGW(TAG, "SoftAP SSID: %s, open auth. DHCP at 192.168.4.1", ap_ssid);

    // --- NEW: mark provisioning stage ---
    set_setup_stage("provisioning");

    // Start HTTP REST server
    ESP_ERROR_CHECK(mount_www_fs());
    ESP_ERROR_CHECK(start_rest_server("/www"));

    // Start captive DNS so all hostnames resolve to AP IP and probes hit our server
    xTaskCreate(captive_dns_task, "captive_dns", 8192, NULL, 5, NULL);

    // --- NEW: mark provisioning stage ---
    set_setup_stage("provisioning");
}

// --- NEW: Boot flow orchestrator ---
static bool wifi_boot_flow(void) {
    char ssid[33] = {0};
    char pass[65] = {0};

    if (load_wifi_credentials(ssid, sizeof(ssid), pass, sizeof(pass))) {
        if (connect_sta_with(ssid, pass)) {
            return true; // STA connected
        }
        // --- NEW: fall through to provisioning if failed ---
        set_setup_stage("provisioning");
        ESP_LOGW(TAG, "Saved credentials failed to connect. Falling back to SoftAP provisioning.");
    } else {
        // --- NEW: no creds yet
        set_setup_stage("provisioning");
        ESP_LOGI(TAG, "No saved Wi-Fi credentials found. Starting provisioning SoftAP.");
    }

    start_softap_and_rest_server();
    return false;
}

// --- WiFi connection logic (unchanged) ---
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) { esp_wifi_connect(); }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) { xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT); }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) { xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT); }
}

// --- NEW: LittleFS mount helper ---
static esp_err_t mount_www_fs(void) {
    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/www",
        .partition_label = "www",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };
    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount LittleFS at /www: %s", esp_err_to_name(ret));
        return ret;
    }
    size_t total = 0, used = 0;
    ret = esp_littlefs_info(conf.partition_label, &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "LittleFS mounted: total=%u, used=%u", (unsigned)total, (unsigned)used);
    }
    return ESP_OK;
}

// --- NEW: persist setup stage helper ---
static void set_setup_stage(const char *stage) {
    nvs_handle_t nvs;
    if (nvs_open("device", NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_str(nvs, "setup", stage);
        nvs_commit(nvs);
        nvs_close(nvs);
    }
}

// +++ ADD: factory reset implementation
static void do_factory_reset(void) {
    ESP_LOGW(TAG, "Factory reset requested: erasing NVS and restarting...");
    // Option A: erase all NVS (clears Wi-Fi, device setup, etc.)
    // Note: PHY will recalibrate on next boot.
    ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_ERROR_CHECK(nvs_flash_init());
    set_setup_stage("factory_reset");
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_restart();
}

// +++ ADD: detect two resets within a short window
static void check_double_reset(void) {
    esp_reset_reason_t r = esp_reset_reason();
    int64_t now_ms = esp_timer_get_time() / 1000;
    if (r == ESP_RST_EXT || r == ESP_RST_SW || r == ESP_RST_BROWNOUT) {
        if ((now_ms - dr_last_ms) < DOUBLE_RESET_WINDOW_MS) {
            dr_count++;
        } else {
            dr_count = 1;
        }
        dr_last_ms = now_ms;
        if (dr_count >= 2) {
            do_factory_reset();
        }
    } else {
        dr_count = 0;
        dr_last_ms = now_ms;
    }
}

// +++ ADD: long-press BOOT button to reset
static void factory_reset_button_task(void *pv) {
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << BOOT_BTN_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);

    for (;;) {
        if (gpio_get_level(BOOT_BTN_GPIO) == 0) { // pressed = low
            int64_t start_ms = esp_timer_get_time() / 1000;
            while (gpio_get_level(BOOT_BTN_GPIO) == 0) {
                if (((esp_timer_get_time() / 1000) - start_ms) >= RESET_HOLD_MS) {
                    ESP_LOGW(TAG, "BOOT held for %d ms -> factory reset", RESET_HOLD_MS);
                    do_factory_reset();
                }
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}