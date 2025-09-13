#include "indicator/led_indicator.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

// --- Includes específicos para cada alvo ---
#if CONFIG_IDF_TARGET_ESP32C3
#include "driver/ledc.h"
#else // Para S3 e outros com LED RGB
#include "led_strip.h"
#include "esp_idf_version.h"
#endif

// --- Configuração de GPIO (ajuste conforme sua placa) ---
#ifndef INDICATOR_GPIO
#if CONFIG_IDF_TARGET_ESP32S3
#define INDICATOR_GPIO          48
#elif CONFIG_IDF_TARGET_ESP32C3
#define INDICATOR_GPIO          8
#else
#define INDICATOR_GPIO          48 // Padrão para outros alvos
#endif
#endif

// --- Constantes de Padrões de LED (Unificadas) ---
// Padrão "Controller Pulse" (Respiração/Fade)
#define PULSE_PERIOD_MS         3000   // Ciclo completo de respiração
#define PULSE_STEP_MS           50     // Intervalo de atualização do brilho

// Padrão "Provisioning" (Blink Triplo)
#define PROV_BLINK_ON_MS       60
#define PROV_BLINK_OFF_MS      100
#define PROV_BLINK_BREAK_MS    1500
#define PROV_BLINK_COLOR_R     36
#define PROV_BLINK_COLOR_G     36
#define PROV_BLINK_COLOR_B     36

// Padrão "Discovery Pulse" (Respiração Rápida Vermelha)
#define DISCOVERY_PULSE_PERIOD_MS 1000   // Ciclo completo de respiração
#define DISCOVERY_PULSE_STEP_MS   50     // Intervalo de atualização do brilho
#define DISCOVERY_PULSE_COLOR_R   255
#define DISCOVERY_PULSE_COLOR_G   0
#define DISCOVERY_PULSE_COLOR_B   0

// Padrão "Worker Flash" (Flash Rápido de Debug)
#define WORKER_FLASH_MS         50
#define WORKER_FLASH_COLOR_R    127
#define WORKER_FLASH_COLOR_G    90
#define WORKER_FLASH_COLOR_B    0

// Flash de Sucesso
#define SUCCESS_FLASH_MS        200
#define SUCCESS_FLASH_PAUSE_MS  150
#define SUCCESS_FLASH_COLOR_R   0
#define SUCCESS_FLASH_COLOR_G   255
#define SUCCESS_FLASH_COLOR_B   0


static const char *TAG_IND = "LED_IND";
static bool s_inited = false;
static TaskHandle_t s_indicator_task = NULL; // Handle único para qualquer tarefa de LED em execução

/* ==========================================================================
 * CAMADA DE ABSTRAÇÃO DE HARDWARE (HAL)
 * Funções internas para controlar o hardware específico de cada chip.
 * ========================================================================== */

#if CONFIG_IDF_TARGET_ESP32C3
/* ------------------------- IMPLEMENTAÇÃO PARA ESP32-C3 (LEDC) ------------------------- */

#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL            LEDC_CHANNEL_0
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT // Resolução de 13 bits (0-8191)
#define LEDC_FREQUENCY          (5000) // 5 kHz
#define LEDC_MAX_DUTY           ((1 << LEDC_DUTY_RES) - 1)

static void _hw_led_init(void) {
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz = LEDC_FREQUENCY,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL,
        .timer_sel = LEDC_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = INDICATOR_GPIO,
        .duty = 0,
        .hpoint = 0,
        .flags.output_invert = 1 // Inverte para LEDs active-low (comum em devkits)
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    ESP_LOGD(TAG_IND, "Init (C3 LEDC) GPIO=%d", INDICATOR_GPIO);
}

static void _hw_set_brightness(uint8_t brightness) {
    if (!s_inited) return;
    // Converte brilho (0-255) para o duty cycle do LEDC (0-8191)
    uint32_t duty = (uint32_t)((float)brightness / 255.0f * (float)LEDC_MAX_DUTY);
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

static void _hw_set_color(uint8_t r, uint8_t g, uint8_t b) {
    // Para um LED de cor única, qualquer cor não-preta significa "ligado no brilho máximo"
    _hw_set_brightness((r | g | b) ? 255 : 0);
}

#else
/* ------------------------- IMPLEMENTAÇÃO PARA ESP32-S3 (LED_STRIP) ------------------------- */

#define INDICATOR_MAX_LEDS 1
static led_strip_handle_t s_strip = NULL;
static SemaphoreHandle_t s_strip_mutex = NULL; // Add mutex for thread safety

static void _hw_led_init(void) {
    if (s_strip) return; // Já inicializou
    
    // Create mutex if not already created
    if (!s_strip_mutex) {
        s_strip_mutex = xSemaphoreCreateMutex();
    }
    
    led_strip_config_t strip_config = {
        .strip_gpio_num = INDICATOR_GPIO,
        .max_leds = INDICATOR_MAX_LEDS,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        .flags.invert_out = false,
#endif
    };
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .mem_block_symbols = 0,
        .flags.with_dma = false
    };
    
    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &s_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_IND, "Failed to create LED strip: %s", esp_err_to_name(ret));
        return;
    }
    
    led_strip_clear(s_strip);
    ESP_LOGD(TAG_IND, "Init (S3 RMT) GPIO=%d", INDICATOR_GPIO);
}

static void _hw_set_color(uint8_t r, uint8_t g, uint8_t b) {
    if (!s_inited || !s_strip || !s_strip_mutex) return;
    
    // Take mutex with timeout to prevent blocking
    if (xSemaphoreTake(s_strip_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        esp_err_t ret = led_strip_set_pixel(s_strip, 0, r, g, b);
        if (ret == ESP_OK) {
            led_strip_refresh(s_strip);
        } else {
            ESP_LOGW(TAG_IND, "LED set failed: %s", esp_err_to_name(ret));
        }
        xSemaphoreGive(s_strip_mutex);
    } else {
        ESP_LOGW(TAG_IND, "Failed to acquire LED mutex, skipping color update");
    }
}

static void _hw_set_brightness(uint8_t brightness) {
    // Aplica o brilho a uma cor base (azul suave) para o efeito de respiração
    const float base_r = 0.0f;
    const float base_g = 18.0f;
    const float base_b = 96.0f;
    float mag = (float)brightness / 255.0f;

    uint8_t r = (uint8_t)(base_r * mag);
    uint8_t g = (uint8_t)(base_g * mag);
    uint8_t b = (uint8_t)(base_b * mag);
    _hw_set_color(r, g, b);
}

#endif

// --- Funções HAL Comuns ---
static void _hw_led_off(void) {
    _hw_set_brightness(0);
}

/* ==========================================================================
 * LÓGICA E TAREFAS DE CONTROLE DE LED (COMUM A TODOS OS CHIPS)
 * ========================================================================== */

// --- Funções Genéricas de Início/Parada de Tarefas ---
static void led_indicator_stop_task(void) {
    if (!s_indicator_task) return;
    TaskHandle_t h = s_indicator_task;
    s_indicator_task = NULL; // Previne race condition
    vTaskDelete(h);
    _hw_led_off();
    ESP_LOGI(TAG_IND, "Indicador de tarefa parado.");
}

static void led_indicator_start_task(TaskFunction_t task_func, const char *task_name) {
    if (s_indicator_task) {
        ESP_LOGD(TAG_IND, "Parando tarefa de LED anterior antes de iniciar a nova.");
        led_indicator_stop_task();
    }
    // Increase stack size from 2048 to 4096 to prevent overflow
    if (xTaskCreate(task_func, task_name, 4096, NULL, 2, &s_indicator_task) != pdPASS) {
        ESP_LOGE(TAG_IND, "Falha ao criar a tarefa %s", task_name);
        s_indicator_task = NULL;
    } else {
        ESP_LOGI(TAG_IND, "Tarefa de indicador '%s' iniciada.", task_name);
    }
}

// --- Tarefas de Padrão de LED ---

/**
 * @brief Tarefa para o efeito de respiração/fade (Controller Pulse).
 * Usa uma curva de cosseno para um fade suave de 0 a 255.
 */
static void pulse_task(void *arg) {
    uint32_t t = 0;
    while (true) {
        float phase = (float)(t % PULSE_PERIOD_MS) / (float)PULSE_PERIOD_MS; // Fase de 0.0 a 1.0
        float mag = 0.5f - 0.5f * cosf(2.0f * (float)M_PI * phase); // Magnitude de 0.0 a 1.0
        uint8_t brightness = (uint8_t)(255.0f * mag);

        _hw_set_brightness(brightness);

        vTaskDelay(pdMS_TO_TICKS(PULSE_STEP_MS));
        t = (t + PULSE_STEP_MS) % PULSE_PERIOD_MS;
    }
}

/**
 * @brief Tarefa para o efeito de respiração/fade vermelho (Discovery Pulse).
 * Usa uma curva de cosseno para um fade suave.
 */
static void discovery_pulse_task(void *arg) {
    uint32_t t = 0;
    while (true) {
        float phase = (float)(t % DISCOVERY_PULSE_PERIOD_MS) / (float)DISCOVERY_PULSE_PERIOD_MS;
        float mag = 0.5f - 0.5f * cosf(2.0f * (float)M_PI * phase); // Magnitude de 0.0 a 1.0
        uint8_t brightness = (uint8_t)(255.0f * mag);

        #if CONFIG_IDF_TARGET_ESP32C3
            _hw_set_brightness(brightness);
        #else
            uint8_t r = (uint8_t)((float)DISCOVERY_PULSE_COLOR_R * mag);
            uint8_t g = (uint8_t)((float)DISCOVERY_PULSE_COLOR_G * mag);
            uint8_t b = (uint8_t)((float)DISCOVERY_PULSE_COLOR_B * mag);
            _hw_set_color(r, g, b);
        #endif

        vTaskDelay(pdMS_TO_TICKS(DISCOVERY_PULSE_STEP_MS));
        t = (t + DISCOVERY_PULSE_STEP_MS) % DISCOVERY_PULSE_PERIOD_MS;
    }
}

/**
 * @brief Tarefa para o padrão de provisionamento (blink duplo).
 */
static void provisioning_blink_task(void *arg) {
    while (true) {
        _hw_set_color(PROV_BLINK_COLOR_R, PROV_BLINK_COLOR_G, PROV_BLINK_COLOR_B);
        vTaskDelay(pdMS_TO_TICKS(PROV_BLINK_ON_MS));
        _hw_led_off();
        vTaskDelay(pdMS_TO_TICKS(PROV_BLINK_OFF_MS));
        
        _hw_set_color(PROV_BLINK_COLOR_R, PROV_BLINK_COLOR_G, PROV_BLINK_COLOR_B);
        vTaskDelay(pdMS_TO_TICKS(PROV_BLINK_ON_MS));
        _hw_led_off();
        vTaskDelay(pdMS_TO_TICKS(PROV_BLINK_OFF_MS));

        _hw_set_color(PROV_BLINK_COLOR_R, PROV_BLINK_COLOR_G, PROV_BLINK_COLOR_B);
        vTaskDelay(pdMS_TO_TICKS(PROV_BLINK_ON_MS));
        _hw_led_off();
        vTaskDelay(pdMS_TO_TICKS(PROV_BLINK_BREAK_MS));
    }
}

/**
 * @brief Tarefa one-shot para o flash de debug do worker.
 */
static void worker_flash_task(void *arg) {
    ESP_LOGD(TAG_IND, "worker_flash_task start");
    #if CONFIG_IDF_TARGET_ESP32C3
        _hw_set_brightness(255);
    #else
        _hw_set_color(WORKER_FLASH_COLOR_R, WORKER_FLASH_COLOR_G, WORKER_FLASH_COLOR_B);
    #endif

    vTaskDelay(pdMS_TO_TICKS(WORKER_FLASH_MS));
    _hw_led_off();
    
    ESP_LOGD(TAG_IND, "worker_flash_task done");
    vTaskDelete(NULL);
}

/**
 * @brief Tarefa one-shot para o flash de sucesso (verde).
 */
static void success_flash_task(void *arg) {
    ESP_LOGD(TAG_IND, "success_flash_task start");
    
    vTaskDelay(pdMS_TO_TICKS(20));
    
    // --- First Flash ---
    #if CONFIG_IDF_TARGET_ESP32C3
        _hw_set_brightness(255);
    #else
        _hw_set_color(SUCCESS_FLASH_COLOR_R, SUCCESS_FLASH_COLOR_G, SUCCESS_FLASH_COLOR_B);
    #endif
    vTaskDelay(pdMS_TO_TICKS(SUCCESS_FLASH_MS));
    _hw_led_off();

    vTaskDelay(pdMS_TO_TICKS(SUCCESS_FLASH_PAUSE_MS));

    #if CONFIG_IDF_TARGET_ESP32C3
        _hw_set_brightness(255);
    #else
        _hw_set_color(SUCCESS_FLASH_COLOR_R, SUCCESS_FLASH_COLOR_G, SUCCESS_FLASH_COLOR_B);
    #endif
    vTaskDelay(pdMS_TO_TICKS(SUCCESS_FLASH_MS));
    _hw_led_off();
    
    ESP_LOGD(TAG_IND, "success_flash_task done");
    vTaskDelete(NULL);
}


/* ==========================================================================
 * API PÚBLICA
 * ========================================================================== */

esp_err_t led_indicator_init(void) {
    if (s_inited) return ESP_OK;
    _hw_led_init();
    s_inited = true;
    _hw_led_off();
    // ESP_LOGI(TAG_IND, "Indicador de LED inicializado para %s.", CONFIG_IDF_TARGET);
    ESP_LOGI(TAG_IND, "Indicador de LED inicializado.");
    return ESP_OK;
}

void led_indicator_set(uint8_t r, uint8_t g, uint8_t b) {
    if (!s_inited) led_indicator_init();
    led_indicator_stop_task(); // Para qualquer padrão em execução
    _hw_set_color(r, g, b);
    ESP_LOGD(TAG_IND, "set r=%u g=%u b=%u", r, g, b);
}

void led_indicator_off(void) {
    if (!s_inited) led_indicator_init();
    led_indicator_stop_task(); // Para qualquer padrão em execução
    _hw_led_off();
    ESP_LOGD(TAG_IND, "off");
}

void led_indicator_flash_worker_debug(void) {
    if (!s_inited) led_indicator_init();
    if (xTaskCreate(worker_flash_task, "led_wflash", 2048, NULL, 3, NULL) != pdPASS) {
        ESP_LOGW(TAG_IND, "Falha ao criar tarefa de flash, executando de forma bloqueante");
        #if CONFIG_IDF_TARGET_ESP32C3
            _hw_set_brightness(255);
        #else
            _hw_set_color(WORKER_FLASH_COLOR_R, WORKER_FLASH_COLOR_G, WORKER_FLASH_COLOR_B);
        #endif
        vTaskDelay(pdMS_TO_TICKS(WORKER_FLASH_MS));
        _hw_led_off();
    }
}

void led_indicator_flash_success(void) {
    if (!s_inited) led_indicator_init();
    
    // Create task with higher stack and priority to ensure it runs
    if (xTaskCreate(success_flash_task, "led_success", 3072, NULL, 4, NULL) != pdPASS) {
        ESP_LOGW(TAG_IND, "Failed to create success flash task, trying direct execution");
        // Fallback to direct execution if task creation fails
        #if CONFIG_IDF_TARGET_ESP32C3
            _hw_set_brightness(255);
        #else
            _hw_set_color(SUCCESS_FLASH_COLOR_R, SUCCESS_FLASH_COLOR_G, SUCCESS_FLASH_COLOR_B);
        #endif
        vTaskDelay(pdMS_TO_TICKS(SUCCESS_FLASH_MS));
        _hw_led_off();
    }
}

void led_indicator_controller_pulse_start(void) {
    if (!s_inited) led_indicator_init();
    led_indicator_start_task(pulse_task, "led_pulse");
}

void led_indicator_controller_pulse_stop(void) {
    led_indicator_stop_task();
}

void led_indicator_provisioning_start(void) {
    if (!s_inited) led_indicator_init();
    led_indicator_start_task(provisioning_blink_task, "led_prov_blink");
}

void led_indicator_provisioning_stop(void) {
    led_indicator_stop_task();
}

void led_indicator_discovery_pulse_start(void) {
    if (!s_inited) led_indicator_init();
    led_indicator_start_task(discovery_pulse_task, "led_disco_pulse");
}

void led_indicator_discovery_pulse_stop(void) {
    led_indicator_stop_task();
}