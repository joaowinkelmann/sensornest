#include "indicator/led_indicator.h"
#include "esp_log.h"
#include "led_strip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include "esp_idf_version.h"

#if CONFIG_IDF_TARGET_ESP32C3
#include "driver/gpio.h"
#include "driver/ledc.h"
/* Some IDF versions/targets may only define LEDC_LOW_SPEED_MODE.
 * Provide a fallback so LEDC_HIGH_SPEED_MODE compiles even if absent. */
#ifndef LEDC_HIGH_SPEED_MODE
#define LEDC_HIGH_SPEED_MODE LEDC_LOW_SPEED_MODE
#endif
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
#if !CONFIG_IDF_TARGET_ESP32C3
static led_strip_handle_t s_strip = NULL;
#endif
static bool s_inited = false;
static TaskHandle_t s_pulse_task = NULL;

#if CONFIG_IDF_TARGET_ESP32C3
// LEDC config for single-color PWM on C3 Super Mini
static const ledc_channel_t s_ledc_chan = LEDC_CHANNEL_0;
static const ledc_timer_t   s_ledc_timer = LEDC_TIMER_0;
static const int s_ledc_freq_hz = 500; // PWM frequency
static const ledc_mode_t s_ledc_mode = LEDC_HIGH_SPEED_MODE;
static const ledc_timer_bit_t s_ledc_resolution = LEDC_TIMER_8_BIT;
#endif

// One-shot worker flash task (non-blocking caller)
static void worker_flash_task(void *arg) {
    const uint32_t on_ms = (uint32_t)(uintptr_t)arg;
#if CONFIG_IDF_TARGET_ESP32C3
    // Simple on/off flash using LEDC (single-color)
    if (!s_inited) { vTaskDelete(NULL); return; }
    uint32_t max_duty = (1 << s_ledc_resolution) - 1;
    ledc_set_duty(s_ledc_mode, s_ledc_chan, max_duty);
    ledc_update_duty(s_ledc_mode, s_ledc_chan);
    vTaskDelay(pdMS_TO_TICKS(on_ms));
    ledc_set_duty(s_ledc_mode, s_ledc_chan, 0);
    ledc_update_duty(s_ledc_mode, s_ledc_chan);
#else
    // Yellow flash (R+G) for RGB strips
    led_indicator_set(255, 180, 0);
    vTaskDelay(pdMS_TO_TICKS(on_ms));
    led_indicator_off();
#endif
    vTaskDelete(NULL);
}

static void apply_color(uint8_t r, uint8_t g, uint8_t b) {
    if (!s_inited) return;
#if CONFIG_IDF_TARGET_ESP32C3
    // For single-color LED, use the max of RGB as brightness
    uint8_t bright = r;
    if (g > bright) bright = g;
    if (b > bright) bright = b;
    uint32_t duty = (uint32_t)bright * ((1 << s_ledc_resolution) - 1) / 255;
    ledc_set_duty(s_ledc_mode, s_ledc_chan, duty);
    ledc_update_duty(s_ledc_mode, s_ledc_chan);
#else
    led_strip_set_pixel(s_strip, 0, r, g, b);
    led_strip_refresh(s_strip);
#endif
}

esp_err_t led_indicator_init(void) {
    if (s_inited) return ESP_OK;

#if CONFIG_IDF_TARGET_ESP32C3
    // Configure GPIO and LEDC for single-color LED
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << INDICATOR_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    ledc_timer_config_t ledc_timer = {
        .speed_mode       = s_ledc_mode,
        .duty_resolution  = s_ledc_resolution,
        .timer_num        = s_ledc_timer,
        .freq_hz          = s_ledc_freq_hz,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    esp_err_t err = ledc_timer_config(&ledc_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_IND, "ledc_timer_config failed: %s", esp_err_to_name(err));
        return err;
    }
    ledc_channel_config_t ledc_chan = {
        .gpio_num   = INDICATOR_GPIO,
        .speed_mode = s_ledc_mode,
        .channel    = s_ledc_chan,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = s_ledc_timer,
        .duty       = 0
    };
    err = ledc_channel_config(&ledc_chan);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_IND, "ledc_channel_config failed: %s", esp_err_to_name(err));
        return err;
    }
    s_inited = true;
    // ensure LED off
    led_indicator_off();
    ESP_LOGI(TAG_IND, "Initialized single-color indicator (GPIO=%d).", INDICATOR_GPIO);
    return ESP_OK;
#else
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
#endif
}

void led_indicator_set(uint8_t r, uint8_t g, uint8_t b) { apply_color(r,g,b); }
void led_indicator_off(void) { apply_color(0,0,0); }

void led_indicator_flash_worker_debug(void) {
    // Start short one-shot task to flash the worker LED without blocking caller.
    // 150 ms is a typical brief flash duration; adjust if desired.
    const uint32_t flash_ms = 150;
    // Task stack and priority are small since work is minimal.
    if (xTaskCreate(worker_flash_task, "led_wflash", 1024, (void *)(uintptr_t)flash_ms, 2, NULL) != pdPASS) {
        // Fallback: do a blocking flash if task creation failed.
#if CONFIG_IDF_TARGET_ESP32C3
        uint32_t max_duty = (1 << s_ledc_resolution) - 1;
        ledc_set_duty(s_ledc_mode, s_ledc_chan, max_duty);
        ledc_update_duty(s_ledc_mode, s_ledc_chan);
        vTaskDelay(pdMS_TO_TICKS(flash_ms));
        ledc_set_duty(s_ledc_mode, s_ledc_chan, 0);
        ledc_update_duty(s_ledc_mode, s_ledc_chan);
#else
        led_indicator_set(255, 180, 0);
        vTaskDelay(pdMS_TO_TICKS(flash_ms));
        led_indicator_off();
#endif
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