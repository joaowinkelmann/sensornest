/*
 * Expor rotas REST para o front-end usar internamente
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
#include "security/certs.h"
#include "net/rest_client.h"
#include "app/app_state.h"
#include "esp_http_client.h"
#include "net/espnow_manager.h"
#include "net/wifi_manager.h"

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "REST_SERVER";

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)
#define SCRATCH_BUFSIZE (10240)
#define MAX_URI_HANDLERS 32

// Move definição de struct antes de usar seu tipo em protótipo
typedef struct rest_server_context {
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE];
} rest_server_context_t;

// --- Protótipos das Funções ---
static inline void rs_add_close_hdr(httpd_req_t *req);
static bool rs_read_body(httpd_req_t *req, char *buf, size_t cap);
static esp_err_t rs_set_content_type_from_file(httpd_req_t *req, const char *filepath);
static esp_err_t rs_rest_common_get_handler(httpd_req_t *req);
static esp_err_t rs_system_info_get_handler(httpd_req_t *req);
static esp_err_t rs_device_role_clear_post_handler(httpd_req_t *req);
static void nvs_set_setup_stage(const char *stage);
static esp_err_t rs_device_role_post_handler(httpd_req_t *req);
static esp_err_t rs_user_creds_post_handler(httpd_req_t *req);
static esp_err_t rs_user_register_post_handler(httpd_req_t *req);
static esp_err_t rs_device_debug_post_handler(httpd_req_t *req);
static esp_err_t rs_setup_status_get_handler(httpd_req_t *req);
static esp_err_t rs_wifi_provision_post_handler(httpd_req_t *req);
static esp_err_t rs_wifi_retry_post_handler(httpd_req_t *req);
static esp_err_t rs_wifi_scan_get_handler(httpd_req_t *req);
static esp_err_t rs_wifi_status_get_handler(httpd_req_t *req);
static esp_err_t rs_probe_generate_204_handler(httpd_req_t *req);
static esp_err_t rs_probe_hotspot_detect_handler(httpd_req_t *req);
static esp_err_t rs_cloud_device_get_handler(httpd_req_t *req);
static esp_err_t rs_cloud_meshes_get_handler(httpd_req_t *req);
static esp_err_t rs_cloud_device_register_post_handler(httpd_req_t *req);
static esp_err_t rs_cloud_mesh_create_post_handler(httpd_req_t *req);
static esp_err_t rs_cors_options_handler(httpd_req_t *req);
static void rs_register_captive_handlers(httpd_handle_t server, rest_server_context_t *ctx);
static void wifi_test_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static esp_err_t rs_setup_complete_post_handler(httpd_req_t *req);
static esp_err_t rs_controllers_scan_get_handler(httpd_req_t *req);
static esp_err_t rs_worker_bind_post_handler(httpd_req_t *req);

#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

static bool rs_read_body(httpd_req_t *req, char *buf, size_t cap) {
    int total = req->content_len;
    ESP_LOGI(TAG, "rs_read_body chamada: uri=%s content_len=%d cap=%zu",
             req->uri ? req->uri : "(null)", total, cap);
    if (total <= 0 || total >= (int)cap) {
        ESP_LOGE(TAG, "Tamanho do corpo inválido: %d (max %zu)", total, cap - 1);
        return false;
    }
    int off = 0;
    while (off < total) {
        int r = httpd_req_recv(req, buf + off, total - off);
        if (r <= 0) {
            ESP_LOGE(TAG, "Falha ao ler o corpo, recv retornou %d", r);
            return false;
        }
        off += r;
    }
    buf[total] = 0;

    // Loga um pequeno trecho seguro para ajudar a depurar payloads (evita logs gigantes)
    int snip = total < 128 ? total : 128;
    char snippet[129]; memcpy(snippet, buf, snip); snippet[snip] = 0;
    ESP_LOGI(TAG, "rs_read_body: lidos %d bytes, snippet='%s'%s",
             total, snippet, (total > snip ? "..." : ""));

    return true;
}

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

/* Envia resposta HTTP com o conteúdo do arquivo solicitado */
static esp_err_t rs_rest_common_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];

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

    rs_set_content_type_from_file(req, filepath);
    rs_add_close_hdr(req);   // <--- garante que o cliente não mantenha o socket vivo

    char *chunk = rest_context->scratch;
    ssize_t read_bytes;
    do {
        /* Le o arquivo em partes para o buffer de rascunho */
        read_bytes = fread(chunk, 1, SCRATCH_BUFSIZE, fd);
        if (read_bytes == -1) {
            ESP_LOGE(TAG, "Falha ao ler arquivo : %s", filepath);
        } else if (read_bytes > 0) {
            /* Envia o conteúdo do buffer como chunk da resposta HTTP */
            if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
                // --- Adiciona um pequeno atraso no EAGAIN para permitir que o cliente acompanhe ---
                if (httpd_get_global_transport_ctx(req->handle) && errno == EAGAIN) {
                    vTaskDelay(pdMS_TO_TICKS(50)); // segura por 50ms
                    if (httpd_resp_send_chunk(req, chunk, read_bytes) == ESP_OK) {
                        continue; // Retry succeeded
                    }
                }
                fclose(fd);
                ESP_LOGE(TAG, "Falhou ao enviar arquivo");
                /* Aborta o envio do arquivo */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Responde com erro interno 500 */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Falhou ao enviar arquivo");
                return ESP_FAIL;
            }
        }
    } while (read_bytes > 0);
    /* Fecha arquivo após completar envio */
    fclose(fd);
    ESP_LOGI(TAG, "Envio realizado com sucesso: %s", filepath);
    /* Responde com um chunk vazio para sinalizar fim da resposta HTTP */
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/* Handler simples para obter informações do chipset */
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

static void wifi_test_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        ESP_LOGI(TAG, "Teste de conexão Wi-Fi falhou.");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "Teste de conexão Wi-Fi realizado com sucesso.");
    }
}

static esp_err_t test_and_save_wifi_credentials(const char *ssid, const char *pass) {
    s_wifi_event_group = xEventGroupCreate();

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_test_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_test_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {0};
    strlcpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_connect());

    ESP_LOGI(TAG, "Testando conexão Wi-Fi no SSID: %s", ssid);

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            pdMS_TO_TICKS(15000)); // Timeout de 15 segundos

    // Remove handlers e limpa
    esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip);
    esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id);
    vEventGroupDelete(s_wifi_event_group);
    s_wifi_event_group = NULL;

    // Se não conectou, para a tentativa; senão mantem STA ligado
    if (!(bits & WIFI_CONNECTED_BIT)) {
        esp_wifi_disconnect();
    }

    if (bits & WIFI_CONNECTED_BIT) {
        // Conexão bem sucedida, salva credenciais
        nvs_handle_t nvs;
        esp_err_t err = nvs_open("wifi", NVS_READWRITE, &nvs);
        if (err != ESP_OK) return err;
        err = nvs_set_str(nvs, "ssid", ssid);
        if (err == ESP_OK) err = nvs_set_str(nvs, "pass", pass);
        if (err == ESP_OK) err = nvs_commit(nvs);
        nvs_close(nvs);
        return err;
    } else {
        return ESP_ERR_INVALID_STATE;
    }
}

/* --- POST /api/v1/device/role/clear --- */
static esp_err_t rs_device_role_clear_post_handler(httpd_req_t *req) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("device", NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "erro ao abrir nvs");
        return ESP_FAIL;
    }

    // Evita limpar se o setup já estiver completo
    char stage[32] = {0};
    size_t len = sizeof(stage);
    nvs_get_str(nvs, "setup", stage, &len);
    if (strcmp(stage, "complete") == 0) {
        nvs_close(nvs);
        httpd_resp_set_status(req, "409 Conflict");
        httpd_resp_sendstr(req, "{\"error\":\"setup_complete_cannot_clear_role\"}");
        return ESP_OK;
    }

    // Apaga papeis e estágio de setup para voltar ao inicio
    nvs_erase_key(nvs, "role");
    nvs_erase_key(nvs, "setup");
    err = nvs_commit(nvs);
    nvs_close(nvs);

    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "nvs write failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Papel do dispositivo e estágio de setup limpos.");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\",\"role_cleared\":true}");
    return ESP_OK;
}

// Setter para o estágio de configuração no NVS
static void nvs_set_setup_stage(const char *stage) {
    nvs_handle_t nvs;
    if (nvs_open("device", NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_str(nvs, "setup", stage);
        nvs_commit(nvs);
        nvs_close(nvs);
    }
}

/* --- POST /api/v1/device/role --- */
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


    char role_buf[16] = {0};
    strlcpy(role_buf, role, sizeof(role_buf));

    nvs_handle_t nvs;
    esp_err_t err = nvs_open("device", NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "erro ao abrir nvs");
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

    ESP_LOGI(TAG, "Papel do dispositivo definido para: %s", role_buf);

    if (strcmp(role, "WORKER")==0) {
        nvs_set_setup_stage("worker_role_set");  // usa auxiliar (era handle inválido)
        ESP_LOGI(TAG, "Role worker selecionado; pulando provisionamento Wi-Fi/Cloud.");
    } else {
        nvs_set_setup_stage("role_set");
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\",\"role_saved\":true}");
    return ESP_OK;
}

/* --- POST /api/v1/provision/user --- */
static esp_err_t rs_user_creds_post_handler(httpd_req_t *req) {
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    if (!rs_read_body(req, buf, SCRATCH_BUFSIZE)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid body");
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid json");
        return ESP_FAIL;
    }

    cJSON *j_email = cJSON_GetObjectItem(root, "email");
    cJSON *j_pass = cJSON_GetObjectItem(root, "password");

    if (!cJSON_IsString(j_email) || !cJSON_IsString(j_pass)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Email and password are required");
        return ESP_FAIL;
    }

    // Copia email para buffer local antes que seja liberado
    char email_buf[128];
    strlcpy(email_buf, j_email->valuestring, sizeof(email_buf));

    // Valida se o login fornecido é válido
    esp_err_t login_err = rc_login(j_email->valuestring, j_pass->valuestring, false);

    if (login_err != ESP_OK) {
        cJSON_Delete(root);
        httpd_resp_set_type(req, "application/json");
        if (login_err == ESP_ERR_INVALID_STATE) {
            httpd_resp_set_status(req, "401 Unauthorized");
            httpd_resp_sendstr(req, "{\"error\":\"invalid_credentials\"}");
        } else if (login_err == ESP_ERR_HTTP_CONNECT || login_err == ESP_ERR_HTTP_EAGAIN) {
            httpd_resp_set_status(req, "503 Service Unavailable");
            httpd_resp_sendstr(req, "{\"error\":\"cloud_unreachable\"}");
        } else {
            httpd_resp_set_status(req, "502 Bad Gateway");
            httpd_resp_sendstr(req, "{\"error\":\"cloud_error\"}");
        }
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Credenciais de usuário validadas e salvas para email: %s", email_buf);
    nvs_set_setup_stage("user_saved"); // Avança estágio de setup

    // Após login com sucesso, tenta obter informações do dispositivo
    char *device_json = NULL;
    esp_err_t derr = rc_get_device_by_mac(NULL, &device_json);
    httpd_resp_set_type(req, "application/json");

    if (derr == ESP_OK && device_json) {
        // Constrói o JSON de resposta com segurança
        cJSON *resp_root = cJSON_CreateObject();
        if (resp_root) {
            cJSON_AddStringToObject(resp_root, "status", "ok");
            cJSON *device_obj = cJSON_Parse(device_json);
            if (device_obj) {
                // Persiste nome do dispositivo da nuvem no NVS para o beacon do controlador usar
                cJSON *j_name = cJSON_GetObjectItem(device_obj, "name");
                if (j_name && cJSON_IsString(j_name) && j_name->valuestring[0]) {
                    nvs_handle_t nvs;
                    if (nvs_open("device", NVS_READWRITE, &nvs) == ESP_OK) {
                        nvs_set_str(nvs, "name", j_name->valuestring);
                        nvs_commit(nvs);
                        nvs_close(nvs);
                        // Se o beacon ESPNOW já estiver rodando, atualiza payload imediatamente
                        espnow_manager_set_controller_name(j_name->valuestring);
                        ESP_LOGI(TAG, "Nome do dispositivo persistido da nuvem no NVS: %s", j_name->valuestring);
                    }
                }
                cJSON_AddItemToObject(resp_root, "device", device_obj);
            }
            char *resp_str = cJSON_PrintUnformatted(resp_root);
            if (resp_str) {
                httpd_resp_sendstr(req, resp_str);
                free(resp_str);
            }
            cJSON_Delete(resp_root);
        }
        free(device_json);
    } else {
        httpd_resp_sendstr(req, "{\"status\":\"ok\",\"device\":null}");
        if (device_json) free(device_json);
    }
    cJSON_Delete(root);
    return ESP_OK;
}

/* --- POST /api/v1/device/debug --- */
static esp_err_t rs_device_debug_post_handler(httpd_req_t *req) {
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    if (!rs_read_body(req, buf, SCRATCH_BUFSIZE)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid body");
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid json");
        return ESP_FAIL;
    }

    const cJSON *j_enabled = cJSON_GetObjectItem(root, "enabled");
    if (!cJSON_IsBool(j_enabled)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "boolean 'enabled' field required");
        return ESP_FAIL;
    }

    uint8_t debug_flag = cJSON_IsTrue(j_enabled) ? 1 : 0;

    nvs_handle_t nvs;
    esp_err_t err = nvs_open("device", NVS_READWRITE, &nvs);
    if (err == ESP_OK) {
        err = nvs_set_u8(nvs, "debug", debug_flag);
        if (err == ESP_OK) err = nvs_commit(nvs);
        nvs_close(nvs);
    }
    cJSON_Delete(root);

    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "nvs write failed");
        return ESP_FAIL;
    }

    // Aplica em tempo de execução sem reiniciar
    app_set_debug_mode(debug_flag == 1);

    httpd_resp_set_type(req, "application/json");
    // Mantem campo 'restarting' (false) para compatibilidade com frontend
    httpd_resp_sendstr(req, debug_flag ?
        "{\"status\":\"ok\",\"restarting\":false,\"debug\":true}" :
        "{\"status\":\"ok\",\"restarting\":false,\"debug\":false}");
    return ESP_OK;
}

/* --- GET /api/v1/setup/status --- */
static esp_err_t rs_setup_status_get_handler(httpd_req_t *req) {
    char role[16] = {0};
    char stage[32] = {0};
    bool has_wifi = false;
    bool connected = false;
    char ip_str[16] = {0};
    char ssid[33] = {0};

    cJSON *root = cJSON_CreateObject();

    // Carrega role e estágio do NVS
    nvs_handle_t nvs;
    if (nvs_open("device", NVS_READONLY, &nvs) == ESP_OK) {
        size_t len = sizeof(role);
        nvs_get_str(nvs, "role", role, &len);
        len = sizeof(stage);
        nvs_get_str(nvs, "setup", stage, &len);
        uint8_t debug_flag = 0;
        nvs_get_u8(nvs, "debug", &debug_flag);
        nvs_close(nvs);
        cJSON_AddBoolToObject(root, "debug", debug_flag == 1);
    }

    // Checa se credenciais Wi-Fi existem
    nvs_handle_t nvs_wifi;
    if (nvs_open("wifi", NVS_READONLY, &nvs_wifi) == ESP_OK) {
        size_t ssz = sizeof(ssid);
        if (nvs_get_str(nvs_wifi, "ssid", ssid, &ssz) == ESP_OK) {
            has_wifi = true;
        }
        nvs_close(nvs_wifi);
        ssid[0] = '\0'; // não expõe o SSID salvo aqui
    }

    // Informações de conexão
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


    cJSON_AddStringToObject(root, "role", role[0] ? role : NULL);
    cJSON_AddStringToObject(root, "stage", stage[0] ? stage : NULL);
    cJSON_AddBoolToObject(root, "has_wifi", has_wifi);
    cJSON_AddBoolToObject(root, "connected", connected);
    if (connected) {
        cJSON_AddStringToObject(root, "ssid", ssid);
        cJSON_AddStringToObject(root, "ip", ip_str);
    }

    const char *json = cJSON_PrintUnformatted(root);

    ESP_LOGI(TAG, "Status setup: role=%s, stage=%s, tem_wifi=%d, conectado=%d, ssid=%s, ip=%s",
             role[0] ? role : "N/A", stage[0] ? stage : "N/A", has_wifi, connected, ssid, ip_str);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);
    free((void *)json);
    cJSON_Delete(root);
    return ESP_OK;
}

/* Handler de provisionamento - salva credenciais Wi-Fi */
static esp_err_t rs_wifi_provision_post_handler(httpd_req_t *req)
{
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    if (!rs_read_body(req, buf, SCRATCH_BUFSIZE)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid body");
        return ESP_FAIL;
    }
    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid json");
        return ESP_FAIL;
    }
    const cJSON *j_ssid = cJSON_GetObjectItem(root, "ssid");
    const cJSON *j_pass = cJSON_GetObjectItem(root, "password");

    const char *ssid_log = (j_ssid && cJSON_IsString(j_ssid)) ? j_ssid->valuestring : "(null)";
    const char *pass_log = (j_pass && cJSON_IsString(j_pass) && j_pass->valuestring[0] != 0) ? j_pass->valuestring : "(empty)";
    ESP_LOGI(TAG, "Dados de provisionamento Wi-Fi recebidos: SSID='%s' SENHA='%s'", ssid_log, pass_log);

    if (!cJSON_IsString(j_ssid) || !cJSON_IsString(j_pass)) {
        ESP_LOGE(TAG, "Dados de provisionamento Wi-Fi inválidos: %s", buf);
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "ssid/password required");
        return ESP_FAIL;
    }

    // --- REFATORADO: Testa conexão antes de salvar ---
    esp_err_t err = test_and_save_wifi_credentials(j_ssid->valuestring, j_pass->valuestring);
    cJSON_Delete(root);

    if (err == ESP_OK) {
        nvs_set_setup_stage("wifi_saved");
        ESP_LOGI(TAG, "Credenciais Wi-Fi testadas e salvas com sucesso.");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"status\":\"saved\"}");
        return ESP_OK;
    } else if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Teste de conexão Wi-Fi falhou. Credenciais não salvas.");
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_sendstr(req, "{\"error\":\"invalid_credentials\"}");
        return ESP_OK; // Return OK to send custom 401 response
    } else {
        ESP_LOGE(TAG, "Failed to save credentials to NVS: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "nvs_write_failed");
        return ESP_FAIL;
    }
}

/* Handler de provisionamento, tenta reconexão usando credenciais salvas */
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
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "credenciais inválidas");
        return ESP_FAIL;
    }

    // Mantem o AP ligado enquanto conecta via STA
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    esp_wifi_start();
    esp_wifi_connect();

    nvs_set_setup_stage("connecting");

    ESP_LOGI(TAG, "Tentando reconexão Wi-Fi com SSID: %s", cfg.sta.ssid);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"connecting\"}");
    return ESP_OK;
}

static esp_err_t rs_wifi_scan_get_handler(httpd_req_t *req) {
    // Primeiro tenta resultado de cache pré-busca
    char *cached = NULL;
    if (wifi_manager_get_cached_scan(&cached) == ESP_OK && cached) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, cached);
        free(cached);
        ESP_LOGI(TAG, "Servindo resultados de scan Wi-Fi do cache");
        return ESP_OK;
    }
    if (cached) free(cached);

    // Fallback para escanear imediatamente (comportamento existente)
    // Garante que estamos em APSTA para scan rodar sem derrubar o AP
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

    ESP_LOGI(TAG, "Scan Wi-Fi encontrou %d APs", ap_num);

    const char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);
    free((void *)json);
    cJSON_Delete(root);
    return ESP_OK;
}

/* --- Wi-Fi status (GET /api/v1/wifi/status) --- */
static esp_err_t rs_wifi_status_get_handler(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();
    bool connected = false;

    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta_netif) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK && ip_info.ip.addr != 0) {
            // Temos um IP válido, então estamos totalmente conectados.
            connected = true;

            // Pega info do AP, que deve ser confiável.
            wifi_ap_record_t ap_info;
            if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
                cJSON_AddStringToObject(root, "ssid", (const char *)ap_info.ssid);
                cJSON_AddNumberToObject(root, "rssi", ap_info.rssi);
            }

            char ip_str[16];
            snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
            cJSON_AddStringToObject(root, "ip", ip_str);
        }
    }

    cJSON_AddBoolToObject(root, "connected", connected);

    httpd_resp_set_type(req, "application/json");
    const char *json = cJSON_PrintUnformatted(root);
    httpd_resp_sendstr(req, json);
    free((void *)json);
    cJSON_Delete(root);
    return ESP_OK;
}

/* Adiciona handlers para fazer OS mobile disparar UI de captive portal */
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

// --- GET /api/v1/cloud/device?mac=AA:BB:CC:DD:EE:FF ---
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

// --- GET /api/v1/cloud/meshes ---
static esp_err_t rs_cloud_meshes_get_handler(httpd_req_t *req) {
    char *meshes_json = NULL;
    esp_err_t err = rc_get_meshes(&meshes_json);
    httpd_resp_set_type(req, "application/json");

    if (err == ESP_OK && meshes_json) {
        httpd_resp_sendstr(req, meshes_json);
        free(meshes_json);
        return ESP_OK;
    }
    if (err == ESP_ERR_INVALID_STATE) { // Não logado
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_sendstr(req, "{\"error\":\"unauthorized\"}");
        return ESP_OK;
    }

    httpd_resp_set_status(req, "502 Bad Gateway");
    httpd_resp_sendstr(req, "{\"error\":\"cloud_error_fetching_meshes\"}");
    return ESP_OK;
}

/* --- POST /api/v1/cloud/meshes/create --- */
static esp_err_t rs_cloud_mesh_create_post_handler(httpd_req_t *req) {
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

    cJSON *j_name = cJSON_GetObjectItem(root, "name");
    cJSON *j_lat = cJSON_GetObjectItem(root, "lat");
    cJSON *j_lon = cJSON_GetObjectItem(root, "lon");

    if (!cJSON_IsString(j_name) || !j_name->valuestring[0] ||
        !cJSON_IsNumber(j_lat) || !cJSON_IsNumber(j_lon)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "name, lat, and lon are required");
        return ESP_FAIL;
    }

    const char *name = j_name->valuestring;
    double lat = j_lat->valuedouble;
    double lon = j_lon->valuedouble;

    esp_err_t err = rc_create_mesh(name, lat, lon);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    if (err == ESP_OK) {
        httpd_resp_sendstr(req, "{\"status\":\"ok\",\"created\":true}");
        return ESP_OK;
    }
    if (err == ESP_ERR_INVALID_STATE) {
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_sendstr(req, "{\"error\":\"unauthorized\"}");
        return ESP_OK;
    }
    if (err == ESP_ERR_INVALID_ARG) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"error\":\"invalid_payload\"}");
        return ESP_OK;
    }
    httpd_resp_set_status(req, "502 Bad Gateway");
    httpd_resp_sendstr(req, "{\"error\":\"cloud_error_creating_mesh\"}");
    return ESP_OK;
}

static esp_err_t rs_cloud_device_register_post_handler(httpd_req_t *req) {
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

    cJSON *j_name  = cJSON_GetObjectItem(root, "name");
    cJSON *j_role  = cJSON_GetObjectItem(root, "role");
    cJSON *j_mesh  = cJSON_GetObjectItem(root, "meshId");
    cJSON *j_batch = cJSON_GetObjectItem(root, "readingsPerBatch");
    cJSON *j_wake  = cJSON_GetObjectItem(root, "wakeUpInterval");
    cJSON *j_x     = cJSON_GetObjectItem(root, "x");
    cJSON *j_y     = cJSON_GetObjectItem(root, "y");

    if (!cJSON_IsString(j_name) || !j_name->valuestring[0] ||
        !cJSON_IsString(j_role) || !j_role->valuestring[0] ||
        !cJSON_IsString(j_mesh) || !j_mesh->valuestring[0] ||
        !cJSON_IsNumber(j_batch) || !cJSON_IsNumber(j_wake)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing required fields");
        return ESP_FAIL;
    }

    const char *role = j_role->valuestring;
    if (strcmp(role, "CONTROLLER") && strcmp(role, "WORKER")) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid role");
        return ESP_FAIL;
    }

    int x = (j_x && cJSON_IsNumber(j_x)) ? j_x->valueint : 0;
    int y = (j_y && cJSON_IsNumber(j_y)) ? j_y->valueint : 0;

    // Copia papel antes de deletar cJSON para evitar UAF
    char role_copy[16] = {0};
    strlcpy(role_copy, j_role->valuestring, sizeof(role_copy));
    // char name_copy[64] = {0};
    // strlcpy(name_copy, j_name->valuestring, sizeof(name_copy));

    esp_err_t err = rc_register_device(
        j_name->valuestring,
        role,
        j_mesh->valuestring,
        j_batch->valueint,
        j_wake->valueint,
        x,
        y
    );

    cJSON_Delete(root);
    httpd_resp_set_type(req, "application/json");

    if (err == ESP_OK) {
        // Salva papel na NVS
        nvs_handle_t nvs;
        if (nvs_open("device", NVS_READWRITE, &nvs) == ESP_OK) {
            nvs_set_str(nvs, "role", role_copy);
            nvs_commit(nvs);
            nvs_close(nvs);
        }
        nvs_set_setup_stage("device_registered");
        httpd_resp_sendstr(req, "{\"status\":\"ok\",\"registered\":true}");
        return ESP_OK;
    }
    if (err == ESP_ERR_INVALID_STATE) {
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_sendstr(req, "{\"error\":\"unauthorized\"}");
        return ESP_OK;
    }
    if (err == ESP_ERR_INVALID_ARG) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"error\":\"invalid_payload\"}");
        return ESP_OK;
    }
    httpd_resp_set_status(req, "502 Bad Gateway");
    httpd_resp_sendstr(req, "{\"error\":\"cloud_error_registering_device\"}");
    return ESP_OK;
}

// --- POST /api/v1/cloud/device/update ---
static esp_err_t rs_cloud_device_update_post_handler(httpd_req_t *req) {
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
    cJSON *j_name     = cJSON_GetObjectItem(root, "name");
    cJSON *j_batch    = cJSON_GetObjectItem(root, "readingsPerBatch");
    cJSON *j_interval = cJSON_GetObjectItem(root, "wakeUpInterval");
    cJSON *j_mesh     = cJSON_GetObjectItem(root, "meshId");
    cJSON *j_role     = cJSON_GetObjectItem(root, "role");

    bool have_field = false;
    const char *name = NULL, *meshId = NULL, *role = NULL;
    int batch = -1, interval = -1;

    if (j_name && cJSON_IsString(j_name) && j_name->valuestring[0]) {
        name = j_name->valuestring; have_field = true;
    }
    if (j_mesh && cJSON_IsString(j_mesh) && j_mesh->valuestring[0]) {
        meshId = j_mesh->valuestring; have_field = true;
    }
    if (j_role && cJSON_IsString(j_role) && j_role->valuestring[0]) {
        role = j_role->valuestring; have_field = true;
    }
    if (j_batch && cJSON_IsNumber(j_batch) && j_batch->valueint > 0) {
        batch = j_batch->valueint; have_field = true;
    }
    if (j_interval && cJSON_IsNumber(j_interval) && j_interval->valueint > 0) {
        interval = j_interval->valueint; have_field = true;
    }
    if (!have_field) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "no updatable fields");
        return ESP_FAIL;
    }

    esp_err_t err = rc_update_device(name, batch, interval, meshId, role);
    // Se role atualizado localmente, persiste no NVS
    if (err == ESP_OK && role && role[0]) {
        if (strcmp(role, "CONTROLLER") == 0 || strcmp(role, "WORKER") == 0) {
            nvs_handle_t nvs;
            if (nvs_open("device", NVS_READWRITE, &nvs) == ESP_OK) {
                nvs_set_str(nvs, "role", role);
                // Persiste nome se fornecido
                if (name && name[0]) {
                    nvs_set_str(nvs, "name", name);
                    // atualiza nome beacon imediatamente
                    espnow_manager_set_controller_name(name);
                }
                nvs_commit(nvs);
                nvs_close(nvs);
            }
        }
    }
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    if (err == ESP_OK) {
        if (meshId && meshId[0]) nvs_set_setup_stage("mesh_selected");
        httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
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
    httpd_resp_sendstr(req, "{\"error\":\"cloud_error_updating_device\"}");
    return ESP_OK;
}

static esp_err_t rs_cors_options_handler(httpd_req_t *req) {
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type, Authorization");
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
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
    config.stack_size = 8192; // Aumenta o tamanho da pilha para operações TLS

    /* --- sintonía de ciclo de vida do socket para mitigar ENFILE/EMFILE (erro 23 no accept) --- */
    config.lru_purge_enable = true;      // Reclama sessão menos usada se max sockets atingido
    config.enable_so_linger = true;      // Habilita SO_LINGER em sockets fechados
    config.linger_timeout = 0;           // 0 => envia RST no close (libera TIME_WAIT imediatamente). Use 1-2 se preferir FIN gracioso.
    // Opcionalmente aumente se tiver aumentado limites do lwIP:
    // config.max_open_sockets = 8;

    ESP_LOGI(TAG, "Iniciando servidor HTTP");
    ESP_GOTO_ON_ERROR(httpd_start(&server, &config), err, TAG, "Failed to start http server");

    wifi_manager_prefetch_scan(30000);

    /* --- REFATORADO: Registra um handler global CORS OPTIONS --- */
    /* Deve ser registrado primeiro para capturar todas requisições OPTIONS */
    httpd_uri_t cors_handler = {
        .uri = "/*",
        .method = HTTP_OPTIONS,
        .handler = rs_cors_options_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &cors_handler);


    /* Registra endpoints da API (caminhos específicos) */
    httpd_uri_t system_info_get_uri = {
        .uri = "/api/v1/system/info",
        .method = HTTP_GET,
        .handler = rs_system_info_get_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &system_info_get_uri);

    httpd_uri_t device_role_clear_post_uri = {
        .uri = "/api/v1/device/role/clear",
        .method = HTTP_POST,
        .handler = rs_device_role_clear_post_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &device_role_clear_post_uri);

    httpd_uri_t device_role_post_uri = {
        .uri = "/api/v1/device/role",
        .method = HTTP_POST,
        .handler = rs_device_role_post_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &device_role_post_uri);

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

    httpd_uri_t user_creds_post_uri = {
        .uri = "/api/v1/provision/user",
        .method = HTTP_POST,
        .handler = rs_user_creds_post_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &user_creds_post_uri);

    // endpoint de registro de usuário
    httpd_uri_t user_register_post_uri = {
        .uri = "/api/v1/provision/register",
        .method = HTTP_POST,
        .handler = rs_user_register_post_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &user_register_post_uri);

    httpd_uri_t device_debug_post_uri = {
        .uri = "/api/v1/device/debug",
        .method = HTTP_POST,
        .handler = rs_device_debug_post_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &device_debug_post_uri);

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

    // --- registros para integração com nuvem ---
    httpd_uri_t cloud_device_get_uri = {
        .uri = "/api/v1/cloud/device",
        .method = HTTP_GET,
        .handler = rs_cloud_device_get_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &cloud_device_get_uri);

    // Busca as meshes
    httpd_uri_t cloud_meshes_get_uri = {
        .uri = "/api/v1/cloud/meshes",
        .method = HTTP_GET,
        .handler = rs_cloud_meshes_get_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &cloud_meshes_get_uri);

    // Cria uma mesh
    httpd_uri_t cloud_mesh_create_post_uri = {
        .uri = "/api/v1/cloud/meshes/create",
        .method = HTTP_POST,
        .handler = rs_cloud_mesh_create_post_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &cloud_mesh_create_post_uri);

    httpd_uri_t cloud_device_register_post_uri = {
        .uri = "/api/v1/cloud/device/register",
        .method = HTTP_POST,
        .handler = rs_cloud_device_register_post_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &cloud_device_register_post_uri);


    // handler de atualização do dispositivo
    httpd_uri_t cloud_device_update_post_uri = {
        .uri = "/api/v1/cloud/device/update",
        .method = HTTP_POST,
        .handler = rs_cloud_device_update_post_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &cloud_device_update_post_uri);

    httpd_uri_t setup_complete_post_uri = {
        .uri = "/api/v1/setup/complete",
        .method = HTTP_POST,
        .handler = rs_setup_complete_post_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &setup_complete_post_uri);


    /* Registra handlers de captive portal (caminhos específicos) */
    rs_register_captive_handlers(server, rest_context);

    // Traz os controllers encontrados durante a escaneamento
    httpd_uri_t controllers_scan_get = {
        .uri      = "/api/v1/controllers/scan",
        .method   = HTTP_GET,
        .handler  = rs_controllers_scan_get_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &controllers_scan_get);

    httpd_uri_t worker_bind_post = {
        .uri      = "/api/v1/worker/bind",
        .method   = HTTP_POST,
        .handler  = rs_worker_bind_post_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &worker_bind_post);

    // fallback comum para o resto dos GETs
    httpd_uri_t common_get_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = rs_rest_common_get_handler,
        .user_ctx = rest_context
    };

    httpd_register_uri_handler(server, &common_get_uri);

    return ESP_OK;
err:
    if (rest_context) {
        free(rest_context);
    }
    return ret;
}

//  --- ADICIONADO declaracoes forward para handlers de captive probe & registrar ---
static esp_err_t rs_probe_generate_204_handler(httpd_req_t *req);
static esp_err_t rs_probe_hotspot_detect_handler(httpd_req_t *req);

// Registrar para corrigir erro de declaração implícita
static void rs_register_captive_handlers(httpd_handle_t server, rest_server_context_t *ctx) {
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

    // Captura genérica para alguns OEMs
    httpd_uri_t probe_fwlink = {
        .uri      = "/fwlink",
        .method   = HTTP_GET,
        .handler  = rs_probe_generate_204_handler,
        .user_ctx = ctx
    };
    httpd_register_uri_handler(server, &probe_fwlink);

    // O handler principal "/*" em start_rest_server agora cuidará dos redirects captive gerais.
}

static inline void rs_add_close_hdr(httpd_req_t *req) {
    httpd_resp_set_hdr(req, "Connection", "close");
}

/* --- POST /api/v1/setup/complete ---
 * Marca provisionamento como completo e reinicia dispositivo para tarefa principal começar ciclo de sensor. */
static esp_err_t rs_setup_complete_post_handler(httpd_req_t *req) {
    // Nenhum corpo necessário, mas consome se presente
    if (req->content_len > 0) {
        char *scratch = ((rest_server_context_t*)req->user_ctx)->scratch;
        if (!rs_read_body(req, scratch, SCRATCH_BUFSIZE)) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid body");
            return ESP_FAIL;
        }
    }
    nvs_set_setup_stage("complete");
    ESP_LOGI(TAG, "Setup marcado como completo. Reiniciando para começar ciclo de sensor.");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\",\"restarting\":true}");
    // Permite flush da resposta
    vTaskDelay(pdMS_TO_TICKS(150));
    esp_restart();
    return ESP_OK;
}

/* GET /api/v1/controllers/scan */
static esp_err_t rs_controllers_scan_get_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Requisição de scan de controladores recebida.");

    // Lê string de query (params opcionais: fast, ms, force)
    char query[128] = {0};
    bool fast = false;
    bool force = false;
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char fast_str[8];
        if (httpd_query_key_value(query, "fast", fast_str, sizeof(fast_str)) == ESP_OK) {
            fast = atoi(fast_str) == 1;
        }
        char force_str[8];
        if (httpd_query_key_value(query, "force", force_str, sizeof(force_str)) == ESP_OK) {
            force = atoi(force_str) == 1;
        }
    }

    // Se um scan fresco não foi explicitamente pedido, prefere resultados cacheados de pre-provisionamento
    // salvos pelo `wifi_boot_flow` (no sensornest.c) sob chave NVS "pre_ctrls".
    if (!force && !fast) {
        nvs_handle_t nvs;
        if (nvs_open("device", NVS_READONLY, &nvs) == ESP_OK) {
            size_t needed = 0;
            esp_err_t e = nvs_get_str(nvs, "pre_ctrls", NULL, &needed);
            if (e == ESP_OK && needed > 1 && needed < 4096) {
                char *buf = malloc(needed);
                if (buf && nvs_get_str(nvs, "pre_ctrls", buf, &needed) == ESP_OK) {
                    httpd_resp_set_type(req, "application/json");
                    httpd_resp_sendstr(req, buf);
                    free(buf);
                    nvs_close(nvs);
                    ESP_LOGI(TAG, "Servido lista de controladores pré-provisionamento cacheada (len=%u).", (unsigned)needed);
                    return ESP_OK;
                }
                free(buf);
            }
            nvs_close(nvs);
        }
        ESP_LOGI(TAG, "Sem lista de controladores cacheada ou cliente pediu scan fresco; procedendo com scan ativo.");
    }

    // Param de query  ?ms=5000
    int duration_ms = 5000;
    if (query[0] == 0) httpd_req_get_url_query_str(req, query, sizeof(query));
    if (query[0]) {
        char ms_str[16];
        if (httpd_query_key_value(query, "ms", ms_str, sizeof(ms_str)) == ESP_OK) {
            int v = atoi(ms_str);
            if (v >= 1000 && v <= 15000) duration_ms = v;
        }
    }

    if (!espnow_manager_is_initialized()) {
        ESP_LOGW(TAG, "ESPNOW não inicializado ainda no provisionamento worker; inicializando agora.");
        esp_err_t ie = espnow_manager_init();
        if (ie != ESP_OK) {
            ESP_LOGE(TAG, "Falha init ESPNOW no handler scan: %s", esp_err_to_name(ie));
        } else {
            ESP_LOGI(TAG, "Sucesso init ESPNOW no handler scan.");
        }
    } else {
        ESP_LOGD(TAG, "ESPNOW já inicializado, procedendo com scan.");
    }

    char *json = NULL;
    esp_err_t err = espnow_manager_collect_controllers(duration_ms, &json);
    httpd_resp_set_type(req, "application/json");
    if (err == ESP_OK && json) {
        ESP_LOGI(TAG, "Sucesso scan controladores (duração=%d) payload_len=%d", duration_ms, (int)strlen(json));
        httpd_resp_sendstr(req, json);
        free(json);
        return ESP_OK;
    }
    ESP_LOGW(TAG, "Scan controladores falhou (%s) (inited=%d) duração=%d",
             esp_err_to_name(err), espnow_manager_is_initialized(), duration_ms);
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_sendstr(req, "{\"error\":\"scan_failed\"}");
    return ESP_OK;
}

/* POST /api/v1/worker/bind  body: { "name":"Worker1","controllerMac":"AA:BB:.."} */
static esp_err_t rs_worker_bind_post_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "rs_worker_bind_post_handler invocado: uri=%s method=%d content_len=%d",
             req->uri ? req->uri : "(null)", (int)req->method, req->content_len);
    
    char host_hdr[128] = {0}, ct_hdr[64] = {0}, ua_hdr[128] = {0};
    httpd_req_get_hdr_value_str(req, "Host", host_hdr, sizeof(host_hdr));
    httpd_req_get_hdr_value_str(req, "Content-Type", ct_hdr, sizeof(ct_hdr));
    httpd_req_get_hdr_value_str(req, "User-Agent", ua_hdr, sizeof(ua_hdr));
    ESP_LOGI(TAG, "Headers entrada: Host=%s Content-Type=%s UA=%s",
             host_hdr[0] ? host_hdr : "(none)", ct_hdr[0] ? ct_hdr : "(none)", ua_hdr[0] ? ua_hdr : "(none)");

    char *buf = ((rest_server_context_t*)req->user_ctx)->scratch;
    if (!rs_read_body(req, buf, SCRATCH_BUFSIZE)) {
        ESP_LOGW(TAG, "rs_worker_bind_post_handler: falha ao ler corpo");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid body"); return ESP_FAIL;
    }
    ESP_LOGI(TAG, "rs_worker_bind_post_handler: corpo lido com sucesso (%d bytes)", (int)strlen(buf));

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        ESP_LOGW(TAG, "rs_worker_bind_post_handler: corpo json ruim='%s'", buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad json"); return ESP_FAIL;
    }
    cJSON *j_name = cJSON_GetObjectItem(root,"name");
    cJSON *j_mac  = cJSON_GetObjectItem(root,"controllerMac");
    cJSON *j_ch = cJSON_GetObjectItem(root, "channel");

    if (!cJSON_IsString(j_name) || !j_name->valuestring[0] ||
        !cJSON_IsString(j_mac) || !j_mac->valuestring[0] || !cJSON_IsNumber(j_ch)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "mac and channel required");
        return ESP_FAIL;
    }

    uint8_t ch = j_ch->valueint;
    if (ch == 0 || ch > 13) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid channel");
        return ESP_FAIL;
    }

    nvs_handle_t nvs;
    esp_err_t err = nvs_open("device", NVS_READWRITE, &nvs);
    if (err == ESP_OK) {
        err = nvs_set_str(nvs, "controller_mac", j_mac->valuestring);
        if (err == ESP_OK) err = nvs_set_u8(nvs, "controller_ch", ch);
        if (err == ESP_OK) err = nvs_set_str(nvs, "name", j_name->valuestring);
        if (err == ESP_OK) err = nvs_set_str(nvs, "setup", "complete"); // Marca setup como completo
        if (err == ESP_OK) err = nvs_commit(nvs);
        nvs_close(nvs);
    } else {
        ESP_LOGW(TAG, "Falha ao abrir NVS para escrita: %s", esp_err_to_name(err));
    }

    cJSON_Delete(root);
    httpd_resp_set_type(req,"application/json");
    httpd_resp_sendstr(req,"{\"status\":\"ok\",\"restarting\":true}");
    vTaskDelay(pdMS_TO_TICKS(150));
    ESP_LOGI(TAG, "rs_worker_bind_post_handler: reiniciando agora");
    esp_restart();
    return ESP_OK;
}

/* --- POST /api/v1/provision/register ---
 * Registra novo usuário na nuvem, então loga e retorna estado inicial. */
static esp_err_t rs_user_register_post_handler(httpd_req_t *req) {
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    if (!rs_read_body(req, buf, SCRATCH_BUFSIZE)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid body");
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid json");
        return ESP_FAIL;
    }

    cJSON *j_email = cJSON_GetObjectItem(root, "email");
    cJSON *j_pass  = cJSON_GetObjectItem(root, "password");
    cJSON *j_name  = cJSON_GetObjectItem(root, "name");

    if (!cJSON_IsString(j_email) || !cJSON_IsString(j_pass) || !cJSON_IsString(j_name) ||
        !j_email->valuestring[0] || !j_pass->valuestring[0] || !j_name->valuestring[0]) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "email, password and name are required");
        return ESP_FAIL;
    }

    char email_buf[128] = {0};
    char pass_buf[128]  = {0};
    char name_buf[128]  = {0};
    strlcpy(email_buf, j_email->valuestring, sizeof(email_buf));
    strlcpy(pass_buf,  j_pass->valuestring,  sizeof(pass_buf));
    strlcpy(name_buf,  j_name->valuestring,  sizeof(name_buf));
    cJSON_Delete(root);

    esp_err_t err = rc_register_user(email_buf, pass_buf, name_buf);
    httpd_resp_set_type(req, "application/json");
    if (err == ESP_OK) {
        nvs_set_setup_stage("user_saved");

        // retorna dispositivo atual (mesmo comportamento de salvar credenciais)
        char *device_json = NULL;
        esp_err_t derr = rc_get_device_by_mac(NULL, &device_json);
        if (derr == ESP_OK && device_json) {
            cJSON *resp_root = cJSON_CreateObject();
            if (resp_root) {
                cJSON_AddStringToObject(resp_root, "status", "ok");
                cJSON *device_obj = cJSON_Parse(device_json);
                if (device_obj) {
                    // Persiste nome do dispositivo da nuvem para beacon do controlador se presente
                    cJSON *j_name_dev = cJSON_GetObjectItem(device_obj, "name");
                    if (j_name_dev && cJSON_IsString(j_name_dev) && j_name_dev->valuestring[0]) {
                        nvs_handle_t nvs;
                        if (nvs_open("device", NVS_READWRITE, &nvs) == ESP_OK) {
                            nvs_set_str(nvs, "name", j_name_dev->valuestring);
                            nvs_commit(nvs);
                            nvs_close(nvs);
                            espnow_manager_set_controller_name(j_name_dev->valuestring);
                        }
                    }
                    cJSON_AddItemToObject(resp_root, "device", device_obj);
                }
                char *resp_str = cJSON_PrintUnformatted(resp_root);
                if (resp_str) {
                    httpd_resp_sendstr(req, resp_str);
                    free(resp_str);
                }
                cJSON_Delete(resp_root);
            }
            free(device_json);
            return ESP_OK;
        }
        // Payload de sucesso mínimo padrão
        httpd_resp_sendstr(req, "{\"status\":\"ok\",\"registered\":true,\"loggedIn\":true}");
        return ESP_OK;
    }

    if (err == ESP_ERR_INVALID_ARG) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"error\":\"invalid_payload\"}");
        return ESP_OK;
    }
    if (err == ESP_ERR_INVALID_STATE) {
        // Created but login failed (token not stored)
        httpd_resp_set_status(req, "202 Accepted");
        httpd_resp_sendstr(req, "{\"status\":\"partial\",\"registered\":true,\"loggedIn\":false}");
        return ESP_OK;
    }

    httpd_resp_set_status(req, "502 Bad Gateway");
    httpd_resp_sendstr(req, "{\"error\":\"cloud_error_registering_user\"}");
    return ESP_OK;
}