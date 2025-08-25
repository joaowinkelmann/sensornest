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
#include "esp_sleep.h"
#include "rest_client.h"
#include <stdarg.h>
#include "certs.h"

static int custom_log_vprintf(const char *format, va_list arg) {
    // Default color
    const char* color = LOG_RESET_COLOR;

    // Check for specific tags and assign colors
    if (strstr(format, "(REST_SERVER)")) {
        color = LOG_COLOR(LOG_COLOR_CYAN);
    } else if (strstr(format, "(REST_client)")) {
        color = LOG_COLOR(LOG_COLOR_YELLOW);
    } else if (strstr(format, "(MAIN_APP)")) {
        color = LOG_COLOR(LOG_COLOR_GREEN);
    }

    // Print with color, then the original formatted log message
    printf("%s", color);
    int ret = vprintf(format, arg);
    printf(LOG_RESET_COLOR); // Reset to default color
    return ret;
}
// --- END NEW ---

// Constants
#define us_PER_SECOND 1000000ULL  /* Conversion factor for micro seconds to seconds */

// Pin config
#define DHT_TYPE DHT_TYPE_DHT11
#define DHT_GPIO 4

// --- WiFi connection logic (declarations moved up for visibility) ---
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

// Factory reset (só funciona ao usar o botão RESET, e então segurar BOOT por 5s)
#define BOOT_BTN_GPIO           GPIO_NUM_0     // BOOT button (IO0)
#define RESET_HOLD_MS           5000           // hold BOOT for 5s to reset
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
void send_sensor_data();
void authenticated_post_task(void *pvParameters);
static void set_setup_stage(const char *stage);
extern void captive_dns_task(void *pv); // from main/captive_dns.c

// --- NEW: prototypes for boot flow and REST server ---
esp_err_t start_rest_server(const char *base_path); // from rest_server.c
static bool wifi_boot_flow(void);
static bool load_wifi_credentials(char *out_ssid, size_t ssid_len, char *out_pass, size_t pass_len);
static bool connect_sta_with(const char *ssid, const char *pass);
static void start_softap_and_rest_server(void);
static esp_err_t mount_www_fs(void);
const char* get_esp_model(void);








////////////////////////////// REFACTOR DEEP SLEEP START //////////////////////////////

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


// --- NOVO: Protótipos para a nova lógica ---
void collect_and_send_task(void);
static void perform_sensor_reading(void);
static void send_batched_data(void);
static void go_to_sleep(uint64_t sleep_time_seconds);

static void go_to_sleep(uint64_t sleep_time_seconds) {
    ESP_LOGI(TAG, "Entrando em deep sleep por %llu segundos.", sleep_time_seconds);
    
    // Desliga o Wi-Fi para economizar energia
    esp_wifi_stop();

    // Configura o timer de despertar
    esp_sleep_enable_timer_wakeup(sleep_time_seconds * us_PER_SECOND);
    
    // Inicia o deep sleep
    esp_deep_sleep_start();
}


void collect_and_send_task(void) {
    ESP_LOGI(TAG, "--- Iniciando Tarefa de Coleta e Envio ---");
    ESP_LOGI(TAG, "Leituras coletadas até agora no lote atual: %d", readings_count);

    // Variáveis para configuração do dispositivo (com valores padrão)
    int readingsPerBatch = 5;
    int wakeUpInterval = 300; // 5 minutos

    if (!time_is_set) {
        ESP_LOGI(TAG, "Sincronizando a hora com o NTP...");
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, "pool.ntp.org");
        esp_sntp_init();
        setenv("TZ", "GMT+3", 1);
        tzset();
        int retry = 0;
        while (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < 15) {
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
        if (retry < 15) {
            time_is_set = true;
            ESP_LOGI(TAG, "Hora sincronizada com sucesso.");
        } else {
            ESP_LOGE(TAG, "Falha ao sincronizar a hora. Usando hora do sistema.");
            // Em um caso real, poderíamos tentar reiniciar ou dormir por mais tempo.
        }
    } else {
        ESP_LOGI(TAG, "A hora já está sincronizada. Pulando a etapa de NTP.");
    }

    perform_sensor_reading();

    const char *user_email = "test@example.com";
    const char *user_pass  = "password123";
    if (rc_login(user_email, user_pass) == ESP_OK) {
        char *device_json = NULL;
        if (rc_get_device_by_mac(NULL, &device_json) == ESP_OK && device_json) {
            rc_extract_device_config(device_json, &readingsPerBatch, &wakeUpInterval);
            ESP_LOGI(TAG, "Configuração do dispositivo: wakeUpInterval=%d, readingsPerBatch=%d", wakeUpInterval, readingsPerBatch);
            free(device_json);
        } else {
            ESP_LOGW(TAG, "Falha ao obter configuração do dispositivo. Usando padrões.");
        }
    } else {
        ESP_LOGE(TAG, "Falha na autenticação. Usando padrões e tentando enviar mesmo assim.");
    }

    // Passo 4: Decidir se o lote está cheio
    if (readings_count >= readingsPerBatch) {
        ESP_LOGI(TAG, "Lote de leituras completo (%d/%d). Enviando para o servidor.", readings_count, readingsPerBatch);
        send_batched_data(); // Envia todos os dados acumulados
        readings_count = 0; // Zera o contador para o próximo lote
    } else {
        ESP_LOGI(TAG, "Lote incompleto (%d/%d). Voltando a dormir.", readings_count, readingsPerBatch);
    }
    
    // Passo 5: Voltar a dormir
    go_to_sleep(wakeUpInterval);
}

// --- NOVO: Função para ler o sensor e armazenar na memória RTC ---
static void perform_sensor_reading() {
    if (readings_count >= MAX_READINGS_IN_BATCH) {
        ESP_LOGW(TAG, "O lote de leituras está cheio. Descartando nova leitura.");
        return;
    }

    int16_t humidity = 0, temperature = 0;
    
    // Reinicializa o GPIO após acordar novamente
    gpio_set_direction(DHT_GPIO, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_set_pull_mode(DHT_GPIO, GPIO_PULLUP_ONLY);

    esp_err_t res = ESP_FAIL;
    for (int retry = 0; retry < 3; retry++) {
        // Delay antes de puxar os dados
        vTaskDelay(pdMS_TO_TICKS(2000));
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


////////////////////////////// REFACTOR DEEP SLEEP END //////////////////////////////








// --- Main Application Entry Point ---
void app_main(void)
{
    esp_log_set_vprintf(custom_log_vprintf);

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Tarefa para monitorar a ação de factory reset (segurar o botão BOOT por 5s)
    xTaskCreate(factory_reset_button_task, "factory_reset_btn", 4096, NULL, 3, NULL);

    // O boot flow tenta conectar ao Wi-Fi ou inicia o modo de provisionamento.
    bool connected = wifi_boot_flow();

    if (connected) {
        // Se conectado, inicia a lógica principal de coleta e envio.
        // Esta função não retornará, pois colocará o dispositivo para dormir.
        collect_and_send_task();
    } else {
        ESP_LOGW(TAG, "Modo de provisionamento iniciado. Conecte-se ao AP e acesse http://192.168.4.1/");
        // O dispositivo permanecerá acordado no modo de provisionamento.
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

    const char *user_email = "test@example.com";
    const char *user_pass  = "password123";
    esp_err_t auth_status = rc_login(user_email, user_pass);

    // Step 2: If authentication was successful, fetch device config and start data loop
    if (auth_status == ESP_OK) {
        ESP_LOGI(TAG, "Authentication successful, fetching device configuration...");
        char *device_json = NULL;
        esp_err_t err = rc_get_device_by_mac(NULL, &device_json);

        if (err == ESP_OK && device_json) {
            ESP_LOGI(TAG, "Device config fetched: %s", device_json);
            int readingsPerBatch = 1; // Default
            int wakeUpInterval = 300; // Default (5 minutes)
            
            rc_extract_device_config(device_json, &readingsPerBatch, &wakeUpInterval);
            ESP_LOGI(TAG, "Device parameters: wakeUpInterval=%d, readingsPerBatch=%d", wakeUpInterval, readingsPerBatch);
            free(device_json);

            // Main operational loop: send data, then wait/sleep.
            while (1) {
                send_sensor_data();
                ESP_LOGI(TAG, "Next sensor reading in %d seconds...", wakeUpInterval);
                // For a prototype, we use vTaskDelay. For power saving, this would be replaced
                // with esp_deep_sleep_start() and RTC logic to wake up.
                vTaskDelay(pdMS_TO_TICKS(wakeUpInterval * 1000));
            }
        } else {
            ESP_LOGE(TAG, "Failed to get device config: %s. Using defaults and sending once.", esp_err_to_name(err));
            // Fallback for unregistered devices: send data once.
            send_sensor_data();
        }
    } else {
        ESP_LOGE(TAG, "Authentication failed. Aborting task.");
    }

    ESP_LOGI(TAG, "--- Authenticated POST Task Finished ---");
    vTaskDelete(NULL);
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

    // Initialize GPIO for DHT
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << DHT_GPIO,
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    // Add a small delay before reading
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Read DHT11 sensor with retry
    int16_t humidity = 0;
    int16_t temperature = 0;
    esp_err_t res = ESP_FAIL;
    
    for (int retry = 0; retry < 3; retry++) {
        res = dht_read_data(DHT_TYPE, DHT_GPIO, &humidity, &temperature);
        if (res == ESP_OK) {
            break;
        }
        ESP_LOGW(DATA_TAG, "DHT read attempt %d failed: %s", retry + 1, esp_err_to_name(res));
        vTaskDelay(pdMS_TO_TICKS(2000)); // Wait 2 seconds between attempts
    }

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

    // OLD direct HTTP code removed; now delegate to rest_client
    esp_err_t err = rc_post_readings(json_payload);
    if (err == ESP_OK) {
        ESP_LOGI(DATA_TAG, "Readings POST success");
    } else {
        ESP_LOGE(DATA_TAG, "Readings POST failed: %s", esp_err_to_name(err));
    }
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

    // Generate AP SSID "ESP-{MODEL}-{6 char from MAC}"
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    char ap_ssid[32];
    const char* model = get_esp_model();
    snprintf(ap_ssid, sizeof(ap_ssid), "ESP-%s-%02X%02X%02X", model, mac[3], mac[4], mac[5]);

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