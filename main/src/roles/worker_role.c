#include "roles/worker_role.h"
#include "esp_log.h"
#include "sensor/sensor.h"
#include "net/espnow_manager.h"
#include "storage/batch_store.h"
#include "net/rest_client.h"
#include "esp_sntp.h"
#include "nvs.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_sleep.h"
#include "roles/role_manager.h"
#include "net/wifi_manager.h"
#include "indicator/led_indicator.h"

// Declaração antecipada (reusando código temporariamente do sensornest.c até mover totalmente)
extern bool send_batched_data(void);
extern void perform_sensor_reading(void);
extern void simulate_or_sleep(uint64_t secs);

#define TIME_SYNC_PREFIX "TIME:"
#define ACK_PREFIX "ACK:"
#define BIND_PREFIX "BIND:"   // Anuncio de vinculo Worker -> Controller

static const char *TAG_WR = "WORKER_ROLE";

static bool s_debug = false;
// ALTERADO: volatile pois é setado de outra task (thread do ESPNOW manager)
static volatile bool s_ack_received = false; // Flag para rastrear recebimento do ACK

extern RTC_DATA_ATTR bool time_is_set;
extern RTC_DATA_ATTR bool device_config_initialized;
extern RTC_DATA_ATTR int device_readingsPerBatch;
extern RTC_DATA_ATTR int device_wakeUpInterval;

/* Hooks implementados no sensornest.c que ainda reutilizamos */
extern bool send_batched_data(void);          // será internalizado depois
extern void perform_sensor_reading(void);     // mesmo
extern void simulate_or_sleep(uint64_t secs); // mesmo

void worker_role_set_debug(bool enabled) { s_debug = enabled; }

static void worker_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGI(TAG_WR, "SUCESSO: Lote ESPNOW enviado para o controlador (" MACSTR "). Aguardando por ACK...", MAC2STR(mac_addr));
    } else {
        ESP_LOGW(TAG_WR, "Envio ESPNOW falhou para " MACSTR "; mantendo lote.", MAC2STR(mac_addr));
        // Opcionalmente, setar uma flag aqui para parar de esperar um ACK que nunca virá
    }
}

static void worker_recv_cb(const espnow_mgr_event_recv_cb_t *recv_info) {
    // Checa por nova mensagem de ACK do controlador: ACK:<timestamp>:<batch>:<interval>
    if (recv_info->data_len > strlen(ACK_PREFIX) && strncmp((const char*)recv_info->data, ACK_PREFIX, strlen(ACK_PREFIX)) == 0) {
        long long ts_ll = 0;
        int batch = 0, interval = 0;
        const char *body = (const char*)recv_info->data + strlen(ACK_PREFIX);
        if (sscanf(body, "%lld:%d:%d", &ts_ll, &batch, &interval) == 3) {
            if (ts_ll > 0) {
                struct timeval tv = { .tv_sec = (time_t)ts_ll };
                settimeofday(&tv, NULL);
                time_is_set = true;
            }
            if (batch > 0) device_readingsPerBatch = batch;
            if (interval > 0) device_wakeUpInterval = interval;
            device_config_initialized = true;
            ESP_LOGI(TAG_WR, "SUCESSO: ACK recebido do controlador. Hora definida, config atualizada (lote=%d, intervalo=%d).",
                     device_readingsPerBatch, device_wakeUpInterval);
            s_ack_received = true; // Seta flag no ACK com sucesso
            
            // Pisca verde para indicar comunicação com sucesso
            if (s_debug) {
                led_indicator_flash_success();
            }
        }
        return;
    }

    // Checa mensagem legada de sincronia de tempo do controlador
    if (recv_info->data_len > strlen(TIME_SYNC_PREFIX) && strncmp((const char*)recv_info->data, TIME_SYNC_PREFIX, strlen(TIME_SYNC_PREFIX)) == 0) {
        const char *ts_str = (const char*)recv_info->data + strlen(TIME_SYNC_PREFIX);
        time_t ts = atoll(ts_str);
        if (ts > 0) {
            struct timeval tv = { .tv_sec = ts };
            settimeofday(&tv, NULL);
            time_is_set = true;
            ESP_LOGI(TAG_WR, "Tempo sincronizado do controlador: %s", ts_str);
        }
    }
}

static void ensure_time(void) {
    if (time_is_set) return;
    ESP_LOGI(TAG_WR, "Sincronizando tempo com o SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
    int retry = 0;
    while (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < 15) {
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    if (retry < 15) { time_is_set = true; ESP_LOGI(TAG_WR, "Hora sincronizada com sucesso."); }
    else ESP_LOGW(TAG_WR, "Falha na sincronização do horário.");
}

static void fetch_remote_config(void) {
    if (device_config_initialized) return;
    char user_email[128], user_pass[128];
    if (rc_load_creds(user_email, sizeof(user_email), user_pass, sizeof(user_pass)) &&
        rc_login(user_email, user_pass, true) == ESP_OK) {
        char *device_json = NULL;
        if (rc_get_device_by_mac(NULL, &device_json) == ESP_OK && device_json) {
            int batch = -1, interval = -1;
            rc_extract_device_config(device_json, &batch, &interval);
            free(device_json);
            if (batch > 0) device_readingsPerBatch = batch;
            if (interval > 0) device_wakeUpInterval = interval;
            if (interval > 0) device_wakeUpInterval = interval;
            device_config_initialized = true;
            ESP_LOGI(TAG_WR, "Configuração remota aplicada: readingsPerBatch=%d wakeUpInterval=%d",
                     device_readingsPerBatch, device_wakeUpInterval);
        }
    }
}

static void load_controller_binding(void) {
    nvs_handle_t nvs;
    if (nvs_open("device", NVS_READONLY, &nvs) == ESP_OK) {
        char mac_str[24]={0};
        size_t l = sizeof(mac_str);
        if (nvs_get_str(nvs,"controller_mac", mac_str, &l) == ESP_OK && mac_str[0]) {
            int v[6];
            if (sscanf(mac_str,"%x:%x:%x:%x:%x:%x",&v[0],&v[1],&v[2],&v[3],&v[4],&v[5])==6) {
                uint8_t mac[6];
                for (int i=0;i<6;i++) mac[i]=(uint8_t)v[i];
                espnow_manager_set_controller_mac(mac);
                ESP_LOGI(TAG_WR, "Carregado MAC controlador %s para unicast.", mac_str);
            }
        }
        uint8_t ch = 0;
        if (nvs_get_u8(nvs, "controller_ch", &ch) == ESP_OK && ch > 0) {
            espnow_manager_set_fixed_channel(ch);
            ESP_LOGI(TAG_WR, "Carregado canal controlador %d.", ch);
        }
        char name[33]={0}; l=sizeof(name);
        if (nvs_get_str(nvs,"name", name, &l) == ESP_OK) {
            ESP_LOGI(TAG_WR, "Nome do Worker: %s", name);
        }
        nvs_close(nvs);
    }
}

esp_err_t worker_role_start(void) {
    ESP_LOGI(TAG_WR, "Role Worker iniciando (debug=%d).", s_debug);

    espnow_manager_init();
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR); // Habilita modo long-range para conectar com SoftAP do Controlador
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(48)); // 20 dBm potência maxima
    espnow_manager_register_callbacks(worker_send_cb, worker_recv_cb);
    espnow_manager_config_role(false);
    espnow_manager_enable_power_saving(); // Habilita economia de energia para worker

    // pega potencia wi-fi
    int8_t power = 0;
    esp_wifi_get_max_tx_power(&power);
    
    ESP_LOGI(TAG_WR, "ESP-NOW iniciado com potência em %d (0.25 dBm).", power);

    load_controller_binding();

    // anuncia vinculo para Controlador para que ele registre este Worker na nuvem
    do {
        nvs_handle_t nvs;
        if (nvs_open("device", NVS_READONLY, &nvs) == ESP_OK) {
            char name[33] = {0};
            size_t l = sizeof(name);
            if (nvs_get_str(nvs, "name", name, &l) == ESP_OK && name[0]) {
                char payload[64];
                int len = snprintf(payload, sizeof(payload), BIND_PREFIX "%s", name);
                if (len > 0) {
                    // envia algumas vezes para aumentar chance de recepção
                    for (int i = 0; i < 3; ++i) {
                        esp_err_t se = espnow_manager_send_to_controller(payload, len);
                        ESP_LOGI(TAG_WR, "Enviado anúncio de VINCULO (%d/3): %s (res=%s)", i+1, name, esp_err_to_name(se));
                        vTaskDelay(pdMS_TO_TICKS(200));
                    }
                }
            }
            nvs_close(nvs);
        }
    } while (0);

    /* Flash de identificação de debug */
    if (s_debug) {
        led_indicator_init();
    }

    /* Se modo debug estiver habilitado, mantem rodando em loop (sem deep sleep).
     * Em produção (não-debug) mantem comportamento single-shot que simula sleep
     * ou realmente entra em deep-sleep via simulate_or_sleep().
     */
    if (s_debug) {
        for (;;) {
            led_indicator_flash_worker_debug();
            perform_sensor_reading();

              if (batch_store_count() >= device_readingsPerBatch) {
                ESP_LOGI(TAG_WR, "Lote cheio (%d/%d). Enviando via ESPNOW (unicast preferido).",
                         batch_store_count(), device_readingsPerBatch);

                if (espnow_manager_is_initialized()) {
                    // Define canal para bater com controlador antes de enviar
                    uint8_t ch = espnow_manager_get_fixed_channel();
                    if (ch > 0) {
                        esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
                    }
                    char *json_payload = NULL;
                    if (batch_store_build_json(&json_payload) == ESP_OK && json_payload) {
                        size_t len = strlen(json_payload);
                        // ALTERADO: reseta flag ACK ANTES de enviar para evitar condição de corrida
                        s_ack_received = false;
                        esp_err_t se = espnow_manager_send_to_controller(json_payload, len);
                        if (se == ESP_OK) {
                            ESP_LOGI(TAG_WR, "SUCESSO: Lote ESPNOW enviado para controlador (%u bytes). Aguardando ACK...", (unsigned)len);
                            // Espera por ACK por até 2 segundos
                            for (int i = 0; i < 20; i++) {
                                if (s_ack_received) break;
                                vTaskDelay(pdMS_TO_TICKS(100));
                            }
                            if (s_ack_received) {
                                ESP_LOGI(TAG_WR, "ACK recebido. Limpando lote.");
                                batch_store_clear();
                            } else {
                                ESP_LOGW(TAG_WR, "ACK não recebido; mantendo lote.");
                            }
                        } else {
                            ESP_LOGW(TAG_WR, "Envio ESPNOW falhou (%s); mantendo lote.", esp_err_to_name(se));
                        }
                        free(json_payload);
                    } else {
                        ESP_LOGE(TAG_WR, "Falha ao construir JSON do lote; mantendo lote.");
                    }
                } else {
                    ESP_LOGI(TAG_WR, "ESPNOW indisponível; fallback para Wi-Fi/nuvem.");
                    if (wifi_manager_connect_saved(15000)) {
                        ensure_time();
                        fetch_remote_config();
                        bool ok = send_batched_data();
                        if (ok) batch_store_clear();
                    } else {
                        ESP_LOGW(TAG_WR, "Conexão Wi-Fi falhou; mantendo lote.");
                    }
                }
            } else {
                ESP_LOGI(TAG_WR, "Lote incompleto (não cheio) (%d/%d).",
                         batch_store_count(), device_readingsPerBatch);
            }

            // No modo debug espera usando vTaskDelay (sem deep sleep)
            vTaskDelay(pdMS_TO_TICKS(device_wakeUpInterval * 1000));
        }
    }

    // Comportamento de ciclo único (produção/deep-sleep) preservado
    perform_sensor_reading();
    if (s_debug) {
        vTaskDelay(pdMS_TO_TICKS(150));
        led_indicator_off();
    }

    if (batch_store_count() >= device_readingsPerBatch) {
        ESP_LOGI(TAG_WR, "Lote cheio (%d/%d). Enviando via ESPNOW (unicast preferido).",
                 batch_store_count(), device_readingsPerBatch);

        if (espnow_manager_is_initialized()) {
            // Define canal para bater com controlador antes de enviar
            uint8_t ch = espnow_manager_get_fixed_channel();
            if (ch > 0) {
                esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
            }
            char *json_payload = NULL;
            if (batch_store_build_json(&json_payload) == ESP_OK && json_payload) {
                size_t len = strlen(json_payload);
                // ALTERADO: reseta flag ACK ANTES de enviar (era depois -> race condition)
                s_ack_received = false;
                esp_err_t se = espnow_manager_send_to_controller(json_payload, len); // CHANGED earlier
                if (se == ESP_OK) {
                    ESP_LOGI(TAG_WR, "SUCESSO: Lote ESPNOW enviado para controlador (%u bytes). Aguardando ACK.", (unsigned)len);
                    // Espera ACK por até 2 segundos
                    for (int i = 0; i < 20; i++) {
                        if (s_ack_received) break;
                        vTaskDelay(pdMS_TO_TICKS(100));
                    }
                    if (s_ack_received) {
                        ESP_LOGI(TAG_WR, "ACK recebido. Limpando lote.");
                        batch_store_clear();
                    } else {
                        ESP_LOGW(TAG_WR, "ACK não recebido; mantendo lote.");
                    }
                } else {
                    ESP_LOGW(TAG_WR, "Envio ESPNOW falhou (%s); mantendo lote.", esp_err_to_name(se));
                }
                free(json_payload);
            } else {
                ESP_LOGE(TAG_WR, "Falha ao construir JSON do lote; mantendo lote.");
            }
        } else {
            ESP_LOGI(TAG_WR, "ESPNOW indisponível; fallback para Wi-Fi/nuvem.");
            if (wifi_manager_connect_saved(15000)) {
                ensure_time();
                fetch_remote_config();
                bool ok = send_batched_data();
                if (ok) batch_store_clear();
            } else {
                ESP_LOGW(TAG_WR, "Conexão Wi-Fi falhou; mantendo lote.");
            }
        }
    } else {
        ESP_LOGI(TAG_WR, "Lote incompleto (não cheio) (%d/%d).",
                 batch_store_count(), device_readingsPerBatch);
    }

    simulate_or_sleep(device_wakeUpInterval);
    return (s_debug ? ESP_OK : ESP_OK);
}