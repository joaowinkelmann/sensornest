#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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
#include <stdint.h>
#include "esp_mac.h"
#include "esp_littlefs.h"
#include "nvs.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "net/rest_client.h"
#include <stdarg.h>
#include "security/certs.h"
#include "sensor/sensor.h"
#include "storage/batch_store.h"
#include "roles/role_manager.h"
#include "roles/worker_role.h"
#include "roles/controller_role.h"
#include "net/wifi_manager.h"
#include "net/espnow_manager.h"

static bool g_debug_mode_enabled = false;

// Track one‑time creation/initialization of STA netif & Wi-Fi driver
static bool s_sta_netif_created = false;
static bool s_wifi_inited = false;
static bool s_wifi_started = false;

// Constants
#define us_PER_SECOND 1000000ULL  /* Conversion factor for micro seconds to seconds */

// Config do sensor
#define DHT_TYPE DHT_TYPE_DHT11
#define DHT_GPIO 4
#define DHT_PULLUP_MS 1500 // Tempo de espera antes de realizar a leitura do sensor DHT em millissegundos

// --- WiFi connection logic (declarations moved up for visibility) ---
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

// BOOT button: per-target defaults.
#ifndef BOOT_BTN_GPIO
#if CONFIG_IDF_TARGET_ESP32S3
#define BOOT_BTN_GPIO           GPIO_NUM_0     // typical S3 devkit BOOT
#elif CONFIG_IDF_TARGET_ESP32C3
#define BOOT_BTN_GPIO           GPIO_NUM_9     // common C3 devkit BOOT (change if yours differs)
#else
#define BOOT_BTN_GPIO           GPIO_NUM_0     // fallback
#endif
#endif

#define BOOT_BTN_HOLD_RESET_MS  300           // segurar BOOT por 0.3 segundo para reset de fábrica
static void factory_reset_button_task(void *pvParameters);
static void do_factory_reset(void);

static const char *TAG = "MAIN_APP";
extern const char *cloud_ca_pem;

// --- NEW: Struct to hold response data for the event handler ---
typedef struct {
    char *buffer;
    int buffer_size;
    int data_len;
} http_response_data_t;

// --- Function Prototypes ---
static void set_setup_stage(const char *stage);
extern void captive_dns_task(void *pv);

// --- NEW: prototypes for boot flow and REST server ---
esp_err_t start_rest_server(const char *base_path);
static bool wifi_boot_flow(void);
static bool load_wifi_credentials(char *out_ssid, size_t ssid_len, char *out_pass, size_t pass_len);
static void start_softap_and_rest_server(void);
static esp_err_t mount_www_fs(void);
const char* get_esp_model(void);
static void run_pre_provision_controller_scan(int duration_ms);  // <--- NEW

// REMOVE old MAX_READINGS_IN_BATCH + RTC arrays:
// #define MAX_READINGS_IN_BATCH 50
// RTC_DATA_ATTR static int readings_count = 0;
// RTC_DATA_ATTR static sensor_reading_t sensor_readings_batch[MAX_READINGS_IN_BATCH]; // Array para armazenar o lote
RTC_DATA_ATTR bool time_is_set = false;
RTC_DATA_ATTR bool device_config_initialized = false;
RTC_DATA_ATTR int device_readingsPerBatch = 2;
RTC_DATA_ATTR int device_wakeUpInterval   = 10;


// Forward declare old functions we keep temporarily
bool send_batched_data(void);
void perform_sensor_reading(void);
void simulate_or_sleep(uint64_t seconds);

// Remove outdated prototype:
// void collect_and_send_task(void);

static void go_to_sleep(uint64_t sleep_time_seconds) {
    // if debug mode is on, don't go to sleep
    if (g_debug_mode_enabled) {
        ESP_LOGI(TAG, "Debug mode is enabled, not going to sleep.");
        // wait for the specified time, but don't power off the chipset
        vTaskDelay(pdMS_TO_TICKS(sleep_time_seconds * 1000));
        return;
    }


    ESP_LOGI(TAG, "Entrando em deep sleep por %llu segundos.", sleep_time_seconds);
    
    // Desliga o Wi-Fi para economizar energia
    esp_wifi_stop();

    // Configura o timer de despertar
    esp_sleep_enable_timer_wakeup(sleep_time_seconds * us_PER_SECOND);
    
    // Inicia o deep sleep
    esp_deep_sleep_start();
}


void simulate_or_sleep(uint64_t seconds) {
    if (g_debug_mode_enabled) {
        ESP_LOGI(TAG, "[SIM] Sleeping (simulated) for %llu s (no deep sleep / serial kept).", seconds);
        vTaskDelay(pdMS_TO_TICKS(seconds * 1000));
    } else {
        go_to_sleep(seconds); // real deep sleep (does not return)
    }
}

void collect_and_send_task(void) {
    for (;;) {
        perform_sensor_reading();

        if (batch_store_count() >= device_readingsPerBatch) {
            ESP_LOGI(TAG, "Batch full (%d/%d). Connecting to Wi-Fi to send data.", batch_store_count(), device_readingsPerBatch);

            if (wifi_manager_connect_saved(15000)) {
                if (!time_is_set) {
                    ESP_LOGI(TAG, "Syncing NTP time...");
                    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
                    esp_sntp_setservername(0, "pool.ntp.org");
                    esp_sntp_init();
                    setenv("TZ", "GMT+3", 1);
                    tzset();
                    int retry = 0;
                    while (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < 15) {
                        vTaskDelay(pdMS_TO_TICKS(2000));
                    }
                    if (retry < 15) {
                        time_is_set = true;
                        ESP_LOGI(TAG, "Time synced.");
                    } else {
                        ESP_LOGE(TAG, "Time sync failed.");
                    }
                }

                if (!device_config_initialized) {
                    char user_email[128], user_pass[128];
                    if (rc_load_creds(user_email, sizeof(user_email), user_pass, sizeof(user_pass)) &&
                        rc_login(user_email, user_pass, true) == ESP_OK) {
                        char *device_json = NULL;
                        if (rc_get_device_by_mac(NULL, &device_json) == ESP_OK && device_json) {
                            int batch = -1, interval = -1;
                            rc_extract_device_config(device_json, &batch, &interval);
                            ESP_LOGI(TAG, "Fetched device config: wakeUpInterval=%d, readingsPerBatch=%d", interval, batch);
                            free(device_json);
                            if (batch > 0) device_readingsPerBatch = batch;
                            if (interval > 0) device_wakeUpInterval = interval;
                            device_config_initialized = true;
                        }
                    }
                }

                bool ok = send_batched_data();
                if (ok) {
                    batch_store_clear();
                } else {
                    ESP_LOGW(TAG, "Retaining batch (%d readings) for retry.", batch_store_count());
                }

                if (!g_debug_mode_enabled) { // keep Wi-Fi up in debug if you want quick repeats
                    esp_wifi_disconnect();
                    esp_wifi_stop();
                    ESP_LOGI(TAG, "Wi-Fi stopped.");
                } else {
                    ESP_LOGI(TAG, "[SIM] Keeping Wi-Fi session (debug mode).");
                }
            } else {
                ESP_LOGE(TAG, "Wi-Fi connect failed; will retry next cycle.");
                // keep readings_count so we retry sending next cycle
            }
        } else {
            ESP_LOGI(TAG, "Batch incomplete (%d/%d).", batch_store_count(), device_readingsPerBatch);
        }

        // Simulated or real sleep
        simulate_or_sleep(device_wakeUpInterval);

        if (!g_debug_mode_enabled) {
            // real deep sleep never returns; loop not needed further
            break;
        }
        // Loop continues in debug mode to simulate wake cycle.
    }
}

void perform_sensor_reading() {
    if (batch_store_is_full()) {
        ESP_LOGW(TAG, "O lote de leituras está cheio. Descartando nova leitura.");
        return;
    }
    sensor_reading_t reading;
    esp_err_t err = sensor_read_once(g_debug_mode_enabled, &reading);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha na leitura do sensor (err=%s)", esp_err_to_name(err));
        return;
    }
    if (batch_store_add(&reading) == ESP_OK) {
        ESP_LOGI(TAG, "Leitura #%d armazenada: Temp=%.1fC, Hum=%.1f%%",
                 batch_store_count(), reading.temperature, reading.humidity);
    }
}

// Replace old send_batched_data()
bool send_batched_data() {
    static const char *DATA_TAG = "DATA_SENDER";
    char *json_payload = NULL;
    esp_err_t err_json = batch_store_build_json(&json_payload);
    if (err_json != ESP_OK) {
        ESP_LOGE(DATA_TAG, "Falha ao construir JSON: %s", esp_err_to_name(err_json));
        return false;
    }
    ESP_LOGI(DATA_TAG, "Payload JSON do lote gerado: %s", json_payload);

    esp_err_t err = rc_post_readings(json_payload);
    free(json_payload);

    if (err == ESP_OK) {
        ESP_LOGI(DATA_TAG, "Lote de leituras enviado com sucesso!");
        return true;
    } else {
        ESP_LOGE(DATA_TAG, "Falha ao enviar o lote de leituras: %s", esp_err_to_name(err));
        return false;
    }
}

void app_main(void)
{
    // esp_log_set_vprintf(custom_log_vprintf);

    // Log Levels
    esp_log_level_set("wifi", ESP_LOG_WARN);
    esp_log_level_set("LED_IND", ESP_LOG_DEBUG);

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Init Batch Store
    ESP_ERROR_CHECK(batch_store_init(false));

    // Initialize TCP/IP stack and event loop once
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Load debug mode state from NVS
    nvs_handle_t nvs;
    if (nvs_open("device", NVS_READONLY, &nvs) == ESP_OK) {
        uint8_t debug_flag = 0; // 0 = false, 1 = true
        if (nvs_get_u8(nvs, "debug", &debug_flag) == ESP_OK) {
            g_debug_mode_enabled = (debug_flag == 1);
        }
        nvs_close(nvs);
    }
    if (g_debug_mode_enabled) {
        ESP_LOGW(TAG, "!!! DEBUG MODE ENABLED: Sensor readings will be simulated. !!!");
    }

    // Tarefa para monitorar a ação de factory reset (segurar o botão BOOT por 5s)
    xTaskCreate(factory_reset_button_task, "factory_reset_btn", 4096, NULL, 3, NULL);

    // O boot flow tenta conectar ao Wi-Fi ou inicia o modo de provisionamento.
    bool setup_complete = wifi_boot_flow();

    if (setup_complete) {
        // Se a configuração estiver completa, inicia a lógica principal de coleta e envio.
        // Esta função não retornará, pois colocará o dispositivo para dormir.
        // collect_and_send_task();
        worker_role_set_debug(g_debug_mode_enabled);
        controller_role_set_debug(g_debug_mode_enabled);
        esp_err_t r = role_manager_start();
        if (r != ESP_OK) {
            ESP_LOGW(TAG, "Role manager returned (%s). Staying idle.", esp_err_to_name(r));
            while (1) { vTaskDelay(pdMS_TO_TICKS(10000)); }
        }
        return;
    } else {
        ESP_LOGW(TAG, "Modo de provisionamento iniciado.");
        // O dispositivo permanecerá acordado no modo de provisionamento.
        // --- FIX: Keep main task alive in provisioning mode ---
        while(1) {
            vTaskDelay(pdMS_TO_TICKS(10000));
        }
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

/* NEW: pre‑provision controller discovery helper (always runs in provisioning mode) */
static void run_pre_provision_controller_scan(int duration_ms) {
    if (duration_ms < 1000) duration_ms = 1000;
    if (duration_ms > 15000) duration_ms = 15000;
    ESP_LOGI(TAG, "Pre-provision: scanning for controllers (%d ms).", duration_ms);
    if (espnow_manager_init(false, NULL) != ESP_OK) {
        ESP_LOGW(TAG, "ESPNOW init failed; skipping controller scan.");
        return;
    }
    char *json = NULL;
    if (espnow_manager_collect_controllers(duration_ms, &json) == ESP_OK && json) {
        ESP_LOGI(TAG, "Controller discovery result: %s", json);
        nvs_handle_t nvs;
        if (nvs_open("device", NVS_READWRITE, &nvs) == ESP_OK) {
            nvs_set_str(nvs, "pre_ctrls", json);
            nvs_commit(nvs);
            nvs_close(nvs);
        }
        free(json);
    } else {
        ESP_LOGI(TAG, "No controllers discovered in pre-provision scan.");
        free(json);
    }
}

// --- REFACTORED: Boot flow orchestrator ---
static bool wifi_boot_flow(void) {
    char stage[32] = {0};
    nvs_handle_t nvs;
    if (nvs_open("device", NVS_READONLY, &nvs) == ESP_OK) {
        size_t len = sizeof(stage);
        nvs_get_str(nvs, "setup", stage, &len);
        nvs_close(nvs);
    }

    char role[16] = {0};
    if (nvs_open("device", NVS_READONLY, &nvs) == ESP_OK) {
        size_t rl = sizeof(role);
        nvs_get_str(nvs, "role", role, &rl);
        nvs_close(nvs);
    }

    if (strcmp(stage, "complete") == 0) {
        ESP_LOGI(TAG, "Setup complete (stage='%s'). Proceeding to main task.", stage);
        return true;
    }

    /* ALWAYS attempt a short controller discovery while in provisioning/factory_reset/new state */
    run_pre_provision_controller_scan(10000);

    ESP_LOGI(TAG, "Setup not complete (stage: '%s'). Starting provisioning server.", stage[0] ? stage : "new");
    start_softap_and_rest_server();
    return false;
}

// --- NEW: Start SoftAP with "ESP-{shortID}" and launch REST server ---
static void start_softap_and_rest_server(void) {
    // Create both AP and STA default interfaces so we can scan/connect while AP is up
    // It's safe to call these multiple times.
    esp_netif_create_default_wifi_ap();
    if (!s_sta_netif_created) {
        // Avoid creating STA netif if another module (e.g. wifi_manager) already did.
        if (esp_netif_get_handle_from_ifkey("WIFI_STA_DEF")) {
            ESP_LOGI(TAG, "STA netif already exists (detected). Reusing.");
            s_sta_netif_created = true;
        } else {
            esp_netif_create_default_wifi_sta();
            s_sta_netif_created = true;
        }
    }
    if (!s_wifi_inited) {
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        s_wifi_inited = true;
    }

    // Generate AP SSID "ESP-{MODEL}-{6 char from MAC}"
    uint8_t mac[6];
    char ssid[32];
    esp_efuse_mac_get_default(mac);
    snprintf(ssid, sizeof(ssid), "ESP-%s-%02X%02X%02X", get_esp_model(), mac[3], mac[4], mac[5]);

    wifi_config_t wifi_config = {
        .ap = {
            .ssid_len = strlen(ssid),
            .channel = 0, // auto
            .password = "",
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN,
        },
    };
    strlcpy((char *)wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "SoftAP SSID: %s @ ch. %d", ssid, wifi_config.ap.channel);

    // Start the REST server
    ESP_ERROR_CHECK(mount_www_fs());
    ESP_ERROR_CHECK(start_rest_server("/www"));

    // Start the captive DNS task
    xTaskCreate(&captive_dns_task, "captive_dns", 4096, NULL, 3, NULL);
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
        ESP_LOGI(TAG, "LittleFS mounted: total=%u KB, used=%u KB", (unsigned)(total/1024), (unsigned)(used/1024));
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

static void do_factory_reset(void) {
    ESP_LOGW(TAG, "Factory reset requested: erasing NVS and restarting...");
    ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_ERROR_CHECK(nvs_flash_init());
    set_setup_stage("factory_reset");
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_restart();
}

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
                if (((esp_timer_get_time() / 1000) - start_ms) >= BOOT_BTN_HOLD_RESET_MS) {
                    ESP_LOGW(TAG, "BOOT held for %d ms -> factory reset", BOOT_BTN_HOLD_RESET_MS);
                    do_factory_reset();
                }
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

const char* get_esp_model(void) {
#if CONFIG_IDF_TARGET_ESP32S3
    return "S3";
#elif CONFIG_IDF_TARGET_ESP32C3
    return "C3";
#elif CONFIG_IDF_TARGET_ESP32S2
    return "S2";
#elif CONFIG_IDF_TARGET_ESP32C2
    return "C2";
#elif CONFIG_IDF_TARGET_ESP32C6
    return "C6";
#elif CONFIG_IDF_TARGET_ESP32H2
    return "H2";
#elif CONFIG_IDF_TARGET_ESP32P4
    return "P4";
#elif CONFIG_IDF_TARGET_ESP32
    return "ESP32";
#else
    return "UNKNOWN";
#endif
}

bool app_get_debug_mode(void) {
    return g_debug_mode_enabled;
}
void app_set_debug_mode(bool enabled) {
    g_debug_mode_enabled = enabled;
    ESP_LOGW(TAG, "Debug mode %s", enabled ? "ENABLED" : "DISABLED");
}