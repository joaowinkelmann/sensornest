#include "indicator/led_indicator.h"
#include "esp_log.h"
#include "led_strip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include "esp_idf_version.h"

#define INDICATOR_GPIO          48
#define INDICATOR_MAX_LEDS      1
#define PULSE_PERIOD_MS         3000   /* full breathe cycle */
#define PULSE_STEP_MS           60

static const char *TAG_IND = "LED_IND";
static led_strip_handle_t s_strip = NULL;
static bool s_inited = false;
static TaskHandle_t s_pulse_task = NULL;

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
    /* Yellow flash (R+G). Caller can optionally delay ~150ms then turn off. */
    led_indicator_set(255, 180, 0);
}

static void pulse_task(void *arg) {
    /* Light blue base color (tint) scaled by sine respiration curve */
    // const float base_r = 80.0f;
    // const float base_g = 180.0f;
    // const float base_b = 255.0f;
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