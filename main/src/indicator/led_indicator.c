#include "indicator/led_indicator.h"
#include "esp_log.h"
#include "led_strip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include "esp_idf_version.h"

#if CONFIG_IDF_TARGET_ESP32C3
#include "driver/gpio.h"
#endif

// Target-dependent indicator GPIO. Adjust if your board uses a different pin.
#ifndef INDICATOR_GPIO
#if CONFIG_IDF_TARGET_ESP32S3
#define INDICATOR_GPIO          48    // typical S3 devkit RGB pin
#elif CONFIG_IDF_TARGET_ESP32C3
#define INDICATOR_GPIO          8     // common C3 Super Mini on-board LED (change if yours differs)
#else
#define INDICATOR_GPIO          48
#endif
#endif

#define INDICATOR_MAX_LEDS      1
#define PULSE_PERIOD_MS         3000   /* full breathe cycle */
#define PULSE_STEP_MS           60

static const char *TAG_IND = "LED_IND";
static bool s_inited = false;
static TaskHandle_t s_pulse_task = NULL;

#if CONFIG_IDF_TARGET_ESP32C3
/* --------------------------------------------------------------------------
 * ESP32C3 REFACTORED SIMPLE GPIO INDICATOR (GPIO 8 active LOW)
 * -------------------------------------------------------------------------- */
#ifndef INDICATOR_GPIO
#define INDICATOR_GPIO 8
#endif
#define INDICATOR_ACTIVE_LOW 1
#define CTRL_BLINK_ON_MS   800
#define CTRL_BLINK_OFF_MS  800
#define WORKER_FLASH_MS    120

static inline void ind_drive(bool on) {
#if INDICATOR_ACTIVE_LOW
    gpio_set_level(INDICATOR_GPIO, on ? 0 : 1);
#else
    gpio_set_level(INDICATOR_GPIO, on ? 1 : 0);
#endif
}

static void ensure_inited_gpio(void) {
    if (s_inited) return;
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << INDICATOR_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io);
    ind_drive(false); // OFF
    s_inited = true;
    ESP_LOGI(TAG_IND, "Init (C3 simple mode) GPIO=%d active_low=%d", INDICATOR_GPIO, INDICATOR_ACTIVE_LOW);
}

/* Worker flash: brief ON then OFF (non-blocking) */
static void worker_flash_task(void *arg) {
    uint32_t dur = (uint32_t)(uintptr_t)arg;
    ESP_LOGD(TAG_IND, "worker_flash_task start dur=%u", (unsigned)dur);
    ensure_inited_gpio();
    ind_drive(true);
    vTaskDelay(pdMS_TO_TICKS(dur));
    ind_drive(false);
    ESP_LOGD(TAG_IND, "worker_flash_task done");
    vTaskDelete(NULL);
}

/* Controller blink task: slow periodic flash while running role */
static void controller_blink_task(void *arg) {
    ESP_LOGI(TAG_IND, "controller_blink_task start (slow flash)");
    ensure_inited_gpio();
    bool on = false;
    while (true) {
        on = !on;
        ind_drive(on);
        vTaskDelay(pdMS_TO_TICKS(on ? CTRL_BLINK_ON_MS : CTRL_BLINK_OFF_MS));
    }
}

esp_err_t led_indicator_init(void) {
    ensure_inited_gpio();
    return ESP_OK;
}

void led_indicator_set(uint8_t r, uint8_t g, uint8_t b) {
    /* Any non-zero => ON */
    ensure_inited_gpio();
    bool on = (r | g | b) != 0;
    ind_drive(on);
    ESP_LOGI(TAG_IND, "set r=%u g=%u b=%u -> %s", r,g,b,on?"ON":"OFF");
}

void led_indicator_off(void) {
    ensure_inited_gpio();
    ind_drive(false);
    ESP_LOGI(TAG_IND, "off");
}

void led_indicator_flash_worker_debug(void) {
    ensure_inited_gpio();
    // Increased stack to avoid stack-protection/vfprintf panics on C3
    if (xTaskCreate(worker_flash_task, "led_wflash", 2048, (void*)(uintptr_t)WORKER_FLASH_MS, 2, NULL) != pdPASS) {
        ESP_LOGW(TAG_IND, "flash task create failed, doing inline flash");
        ind_drive(true);
        vTaskDelay(pdMS_TO_TICKS(WORKER_FLASH_MS));
        ind_drive(false);
    } else {
        ESP_LOGI(TAG_IND, "flash task spawned");
    }
}

void led_indicator_controller_pulse_start(void) {
    ensure_inited_gpio();
    if (s_pulse_task) {
        ESP_LOGI(TAG_IND, "controller blink already running");
        return;
    }
    // Increase stack to 2K for safe logging / runtime on C3
    if (xTaskCreate(controller_blink_task, "led_ctrl_blink", 2048, NULL, 1, &s_pulse_task) != pdPASS) {
        ESP_LOGE(TAG_IND, "controller blink task create failed");
        s_pulse_task = NULL;
    } else {
        ESP_LOGI(TAG_IND, "controller blink started");
    }
}

void led_indicator_controller_pulse_stop(void) {
    if (!s_pulse_task) return;
    TaskHandle_t h = s_pulse_task;
    s_pulse_task = NULL;
    vTaskDelete(h);
    ind_drive(false);
    ESP_LOGI(TAG_IND, "controller blink stopped");
}

#else  /* ------------------ OTHER TARGETS (retain existing implementation) ------------------ */

 // LED strip handle used by non-ESP32C3 targets
 static led_strip_t *s_strip = NULL;

// One-shot worker flash task (non-blocking caller)
static void worker_flash_task(void *arg) {
    const uint32_t on_ms = (uint32_t)(uintptr_t)arg;
    // Yellow flash (R+G) for RGB strips
    led_indicator_set(255, 180, 0);
    vTaskDelay(pdMS_TO_TICKS(on_ms));
    led_indicator_off();
    vTaskDelete(NULL);
}

static void apply_color(uint8_t r, uint8_t g, uint8_t b) {
    if (!s_inited) return;
    led_strip_set_pixel(s_strip, 0, r, g, b);
    led_strip_refresh(s_strip);
}

esp_err_t led_indicator_init(void) {
    if (s_inited) return ESP_OK;

    led_strip_config_t strip_config = {
        .strip_gpio_num = INDICATOR_GPIO,
        .max_leds = INDICATOR_MAX_LEDS,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        .flags.invert_out = false,
#endif
    };
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 0
    };

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &s_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_IND, "led_strip_new_rmt_device failed: %s", esp_err_to_name(err));
        return err;
    }
    s_inited = true;
    led_indicator_off();
    ESP_LOGI(TAG_IND, "Initialized (GPIO=%d).", INDICATOR_GPIO);
    return ESP_OK;
}

void led_indicator_set(uint8_t r, uint8_t g, uint8_t b) { apply_color(r,g,b); }
void led_indicator_off(void) { apply_color(0,0,0); }

void led_indicator_flash_worker_debug(void) {
    // Start short one-shot task to flash the worker LED without blocking caller.
    const uint32_t flash_ms = 150;
    if (xTaskCreate(worker_flash_task, "led_wflash", 2048, (void *)(uintptr_t)flash_ms, 2, NULL) != pdPASS) {
        // Fallback: do a blocking flash if task creation failed.
        led_indicator_set(255, 180, 0);
        vTaskDelay(pdMS_TO_TICKS(flash_ms));
        led_indicator_off();
    }
}

static void pulse_task(void *arg) {
#if CONFIG_IDF_TARGET_ESP32C3
    /* For single-color LED on C3, use blue tint mapped to brightness */
    const float base_b = 96.0f;
    uint32_t t = 0;
    while (true) {
        float phase = (float)(t % PULSE_PERIOD_MS) / (float)PULSE_PERIOD_MS; // 0..1
        float mag = 0.5f - 0.5f * cosf(2.0f * (float)M_PI * phase);
        uint8_t brightness = (uint8_t)(base_b * mag);
        apply_color(brightness, brightness, brightness); // applies to LEDC as brightness
        vTaskDelay(pdMS_TO_TICKS(PULSE_STEP_MS));
        t += PULSE_STEP_MS;
    }
#else
    /* RGB breathe curve */
    const float base_r = 0.0f;
    const float base_g = 18.0f;
    const float base_b = 96.0f;
    uint32_t t = 0;

    while (true) {
        float phase = (float)(t % PULSE_PERIOD_MS) / (float)PULSE_PERIOD_MS; // 0..1
        /* Smooth breathe curve: 0.5 - 0.5*cos(2πx) => 0..1 */
        float mag = 0.5f - 0.5f * cosf(2.0f * (float)M_PI * phase);

        uint8_t r = (uint8_t)(base_r * mag);
        uint8_t g = (uint8_t)(base_g * mag);
        uint8_t b = (uint8_t)(base_b * mag);
        led_indicator_set(r,g,b);

        vTaskDelay(pdMS_TO_TICKS(PULSE_STEP_MS));
        t += PULSE_STEP_MS;
    }
#endif
}

void led_indicator_controller_pulse_start(void) {
    if (s_pulse_task) return;
    if (led_indicator_init() != ESP_OK) return;
    xTaskCreate(pulse_task, "led_pulse", 2048, NULL, 1, &s_pulse_task);
}

void led_indicator_controller_pulse_stop(void) {
    if (!s_pulse_task) return;
    TaskHandle_t h = s_pulse_task;
    s_pulse_task = NULL;
    vTaskDelete(h);
    led_indicator_off();
}

#endif /* CONFIG_IDF_TARGET_ESP32C3 */