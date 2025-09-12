#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/* Initialize (idempotent). Safe to call multiple times. */
esp_err_t led_indicator_init(void);

/* Set solid color (r,g,b). */
void led_indicator_set(uint8_t r, uint8_t g, uint8_t b);

/* Turn off. */
void led_indicator_off(void);

/* Worker: brief flash (non-blocking helper expects caller to delay a little). */
void led_indicator_flash_worker_debug(void);

/* Controller: start / stop pulsing task (light blue). */
void led_indicator_controller_pulse_start(void);
void led_indicator_controller_pulse_stop(void);

/* Provisioning: start a double-flash indicator. */
void led_indicator_provisioning_start(void);

/* Provisioning: stop the double-flash indicator. */
void led_indicator_provisioning_stop(void);

/* Discovery: start a fast red pulsing indicator. */
void led_indicator_discovery_pulse_start(void);

/* Discovery: stop the fast red pulsing indicator. */
void led_indicator_discovery_pulse_stop(void);