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
#include "rest_client.h"
#include <stdarg.h>
#include "certs.h"

static bool g_debug_mode_enabled = false;

// Track one‑time creation/initialization of STA netif & Wi-Fi driver
static bool s_sta_netif_created = false;
static bool s_wifi_inited = false;
static bool s_wifi_started = false;

static int custom_log_vprintf(const char *format, va_list arg) {
    // Default color
    const char* color = LOG_RESET_COLOR;

    // Check for specific tags and assign colors
    if (strstr(format, "(REST_SERVER)")) {
        color = LOG_COLOR(LOG_COLOR_CYAN);
    } else if (strstr(format, "(REST_client)")) {
        color = LOG_COLOR(LOG_COLOR_YELLOW);
    } else if (strstr(format, "(MAIN_APP)")) {
        color = LOG_COLOR(LOG_COLOR_BLUE);
    }

    // Print with color, then the original formatted log message
    printf("%s", color);
    int ret = vprintf(format, arg);
    printf(LOG_RESET_COLOR); // Reset to default color
    return ret;
}

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

#define BOOT_BTN_GPIO           GPIO_NUM_0     // BOOT button (IO0)
#define BOOT_BTN_HOLD_RESET_MS  300           // segurar BOOT por 0.3 segundo para reset de fábrica
static void factory_reset_button_task(void *pvParameters);
static void do_factory_reset(void);

static const char *TAG = "MAIN_APP";
extern const char *winkels_root_ca_pem;

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
static bool connect_to_wifi(void);
static void start_softap_and_rest_server(void);
static esp_err_t mount_www_fs(void);
const char* get_esp_model(void);


// Estrutura para uma única leitura de sensor
typedef struct {
    float temperature;
    float humidity;
    time_t timestamp; // Usaremos time_t para o carimbo de data/hora
} sensor_reading_t;

// O número máximo de leituras que podemos armazenar.
// Cuidar limite de memória RTC (Baseline de 8KB para S3 e C3)
#define MAX_READINGS_IN_BATCH 50

// Variáveis que irão persistir durante o deep sleep
RTC_DATA_ATTR static int readings_count = 0; // Contador de leituras atuais no lote
RTC_DATA_ATTR static sensor_reading_t sensor_readings_batch[MAX_READINGS_IN_BATCH]; // Array para armazenar o lote
RTC_DATA_ATTR static bool time_is_set = false;
RTC_DATA_ATTR static bool device_config_initialized = false;
RTC_DATA_ATTR static int device_readingsPerBatch = 2;
RTC_DATA_ATTR static int device_wakeUpInterval   = 10;


void collect_and_send_task(void);
static void perform_sensor_reading(void);
static void send_batched_data(void);
static void go_to_sleep(uint64_t sleep_time_seconds);

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


static void simulate_or_sleep(uint64_t seconds) {
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

        if (readings_count >= device_readingsPerBatch) {
            ESP_LOGI(TAG, "Batch full (%d/%d). Connecting to Wi-Fi to send data.", readings_count, device_readingsPerBatch);

            if (connect_to_wifi()) {
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

                send_batched_data();
                readings_count = 0;

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
            ESP_LOGI(TAG, "Batch incomplete (%d/%d).", readings_count, device_readingsPerBatch);
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

static void perform_sensor_reading() {
    if (readings_count >= MAX_READINGS_IN_BATCH) {
        ESP_LOGW(TAG, "O lote de leituras está cheio. Descartando nova leitura.");
        return;
    }

    if (g_debug_mode_enabled) {
        ESP_LOGI(TAG, "Simulating sensor reading (DEBUG MODE)");
        sensor_readings_batch[readings_count].temperature = 20.0f + (float)(esp_random() % 100) / 10.0f;
        sensor_readings_batch[readings_count].humidity = 40.0f + (float)(esp_random() % 200) / 10.0f;
        time(&sensor_readings_batch[readings_count].timestamp);

        ESP_LOGI(TAG, "Leitura #%d armazenada (simulada): Temp=%.1fC, Hum=%.1f%%",
                 readings_count + 1,
                 sensor_readings_batch[readings_count].temperature,
                 sensor_readings_batch[readings_count].humidity);

        readings_count++;
        return;
    }

    int16_t humidity = 0, temperature = 0;
    
    // Reinicializa o GPIO após acordar novamente
    gpio_set_direction(DHT_GPIO, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_set_pull_mode(DHT_GPIO, GPIO_PULLUP_ONLY);

    esp_err_t res = ESP_FAIL;
    for (int retry = 0; retry < 3; retry++) {
        vTaskDelay(pdMS_TO_TICKS(DHT_PULLUP_MS));
        res = dht_read_data(DHT_TYPE, DHT_GPIO, &humidity, &temperature);
        if (res == ESP_OK) {
            break;
        }
        ESP_LOGW(TAG, "Falha na leitura do DHT (tentativa %d): %s", retry + 1, esp_err_to_name(res));
    }

    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Falha na leitura do DHT: %s", esp_err_to_name(res));
        // Poderíamos armazenar um valor de erro, como -999, se quiséssemos.
        // Por enquanto, apenas não incrementamos o contador.
        return;
    }

    // Armazena os dados no nosso array na memória RTC
    sensor_readings_batch[readings_count].temperature = (float)temperature / 10.0f;
    sensor_readings_batch[readings_count].humidity = (float)humidity / 10.0f;
    time(&sensor_readings_batch[readings_count].timestamp); // Pega o timestamp atual

    ESP_LOGI(TAG, "Leitura #%d armazenada: Temp=%.1fC, Hum=%.1f%%", 
             readings_count + 1, 
             sensor_readings_batch[readings_count].temperature, 
             sensor_readings_batch[readings_count].humidity);
    
    // Incrementa o contador de leituras
    readings_count++;
}

// --- NOVO: Função para construir o JSON com o lote de dados e enviá-lo ---
static void send_batched_data() {
    static const char *DATA_TAG = "DATA_SENDER";
    char mac_str[18];
    uint8_t mac_addr[6];
    esp_efuse_mac_get_default(mac_addr);
    sprintf(mac_str, "%02X:%02X:%02X:%02X:%02X:%02X", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);

    // Usando cJSON para construir o payload de forma segura
    cJSON *root = cJSON_CreateObject();
    cJSON *data_array = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "data", data_array);

    for (int i = 0; i < readings_count; i++) {
        cJSON *reading_obj = cJSON_CreateObject();
        
        char time_buffer[30];
        struct tm timeinfo;
        gmtime_r(&sensor_readings_batch[i].timestamp, &timeinfo);
        strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);

        cJSON_AddStringToObject(reading_obj, "mac", mac_str);
        cJSON_AddNumberToObject(reading_obj, "temp", sensor_readings_batch[i].temperature);
        cJSON_AddNumberToObject(reading_obj, "hum", sensor_readings_batch[i].humidity);
        cJSON_AddStringToObject(reading_obj, "readingTime", time_buffer);
        
        cJSON_AddItemToArray(data_array, reading_obj);
    }

    char *json_payload = cJSON_Print(root);
    ESP_LOGI(DATA_TAG, "Payload JSON do lote gerado: %s", json_payload);

    esp_err_t err = rc_post_readings(json_payload);
    if (err == ESP_OK) {
        ESP_LOGI(DATA_TAG, "Lote de leituras enviado com sucesso!");
    } else {
        ESP_LOGE(DATA_TAG, "Falha ao enviar o lote de leituras: %s", esp_err_to_name(err));
        // Aqui, poderíamos implementar uma lógica de nova tentativa ou salvar os dados em flash.
    }

    // Liberar memória alocada pelo cJSON
    cJSON_Delete(root);
    free(json_payload);
}

void app_main(void)
{
    esp_log_set_vprintf(custom_log_vprintf);

    // Log Levels
    esp_log_level_set("wifi", ESP_LOG_WARN);

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

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
        collect_and_send_task();
    } else {
        ESP_LOGW(TAG, "Modo de provisionamento iniciado. Conecte-se ao AP e acesse http://192.168.4.1/");
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

// --- REFACTORED: Boot flow orchestrator ---
static bool wifi_boot_flow(void) {
    char stage[32] = {0};
    nvs_handle_t nvs;
    if (nvs_open("device", NVS_READONLY, &nvs) == ESP_OK) {
        size_t len = sizeof(stage);
        nvs_get_str(nvs, "setup", stage, &len);
        nvs_close(nvs);
    }

    // If setup is fully complete, just return true to proceed to main task.
    // We will NOT connect to Wi-Fi here anymore.
    if (strcmp(stage, "complete") == 0) {
        ESP_LOGI(TAG, "Setup complete (stage='%s'). Proceeding to main task.", stage);
        return true;
    }

    // If setup is not complete, start provisioning server
    ESP_LOGI(TAG, "Setup not complete (stage: '%s'). Starting provisioning server.", stage[0] ? stage : "new");
    start_softap_and_rest_server();
    return false; // Stay in provisioning mode
}

// --- NEW: Helper function to connect to Wi-Fi on demand ---
static bool connect_to_wifi(void) {
    // Fast path: already connected?
    wifi_ap_record_t ap_info;
    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta_netif &&
        esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        esp_netif_ip_info_t ip;
        if (esp_netif_get_ip_info(sta_netif, &ip) == ESP_OK && ip.ip.addr != 0) {
            // Already connected with valid IP
            return true;
        }
    }

    char ssid[33] = {0};
    char pass[65] = {0};
    if (!load_wifi_credentials(ssid, sizeof(ssid), pass, sizeof(pass))) {
        ESP_LOGE(TAG, "No Wi-Fi credentials found to connect.");
        return false;
    }

    // Ensure STA netif exists (idempotent)
    if (!sta_netif) {
        sta_netif = esp_netif_create_default_wifi_sta(); // creates only once
        if (!sta_netif) {
            ESP_LOGE(TAG, "Failed to create default STA netif");
            return false;
        }
    }

    // Initialize Wi-Fi driver only once
    if (!s_wifi_inited) {
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        s_wifi_inited = true;
    }

    wifi_config_t wifi_config = {0};
    strlcpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    // Start Wi-Fi driver once
    if (!s_wifi_started) {
        ESP_ERROR_CHECK(esp_wifi_start());
        s_wifi_started = true;
    }

    // Event group per attempt
    s_wifi_event_group = xEventGroupCreate();
    if (!s_wifi_event_group) {
        ESP_LOGE(TAG, "Failed to create event group");
        return false;
    }

    esp_event_handler_instance_t instance_any_id = NULL, instance_got_ip = NULL;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

    ESP_ERROR_CHECK(esp_wifi_connect());

    ESP_LOGI(TAG, "Waiting for connection...");
    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(15000));

    esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip);
    esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id);
    vEventGroupDelete(s_wifi_event_group);
    s_wifi_event_group = NULL;

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WiFi Connected.");
        return true;
    }

    ESP_LOGE(TAG, "Connection failed.");
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

// --- NEW: Start SoftAP with "ESP-{shortID}" and launch REST server ---
static void start_softap_and_rest_server(void) {
    ESP_LOGW(TAG, "Starting SoftAP for provisioning...");

    // Create both AP and STA default interfaces so we can scan/connect while AP is up
    // It's safe to call these multiple times.
    esp_netif_create_default_wifi_ap();
    if (!s_sta_netif_created) {
        esp_netif_create_default_wifi_sta();
        s_sta_netif_created = true;
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
            .channel = 1,
            .password = "", // Open network
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN,
        },
    };
    strlcpy((char *)wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGW(TAG, "SoftAP SSID: %s, open auth. DHCP at 192.168.4.1", ssid);

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