
/* hat-warp-energy-manager-brick
 * Copyright (C) 2021 Olaf Lüke <olaf@tinkerforge.com>
 *
 * led.c: Driver for RGB LED
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "led.h"

#include "configs/config_led.h"
#include "bricklib2/hal/ccu4_pwm/ccu4_pwm.h"
#include "bricklib2/hal/system_timer/system_timer.h"
#include "bricklib2/logging/logging.h"
#include "communication.h"
#include "bricklib2/utility/util_definitions.h"

#include "xmc_gpio.h"
#include "xmc_ccu4.h"

#include "cie1931.h"


LED led;

XMC_CCU4_SLICE_t *const slice[4] = {
	CCU41_CC40,
	CCU41_CC41,
	CCU41_CC42,
	CCU41_CC43,
};

// Changes period. This does not do shadow transfer, always call set_duty_cycle afterwards!
void led_ccu4_pwm_set_period(const uint8_t ccu4_slice_number, const uint16_t period_value) {
    XMC_CCU4_SLICE_SetTimerPeriodMatch(slice[ccu4_slice_number], period_value);
}

uint16_t led_ccu4_pwm_get_period(const uint8_t ccu4_slice_number) {
    return XMC_CCU4_SLICE_GetTimerPeriodMatch(slice[ccu4_slice_number]);
}

// Compare value is a value from 0 to period_value (^= 0 to 100% duty cycle)
void led_ccu4_pwm_set_duty_cycle(const uint8_t ccu4_slice_number, const uint16_t compare_value) {
	XMC_CCU4_SLICE_SetTimerCompareMatch(slice[ccu4_slice_number], compare_value);
    XMC_CCU4_EnableShadowTransfer(CCU41, (XMC_CCU4_SHADOW_TRANSFER_SLICE_0 << (ccu4_slice_number*4)) |
    		                             (XMC_CCU4_SHADOW_TRANSFER_PRESCALER_SLICE_0 << (ccu4_slice_number*4)));
}

uint16_t led_ccu4_pwm_get_duty_cycle(const uint8_t ccu4_slice_number) {
	return XMC_CCU4_SLICE_GetTimerCompareMatch(slice[ccu4_slice_number]);
}

// Period value is the amount of clock cycles per period
void led_ccu4_pwm_init(XMC_GPIO_PORT_t *const port, const uint8_t pin, const uint8_t ccu4_slice_number, const uint16_t period_value) {
	const XMC_CCU4_SLICE_COMPARE_CONFIG_t compare_config = {
		.timer_mode          = XMC_CCU4_SLICE_TIMER_COUNT_MODE_EA,
		.monoshot            = false,
		.shadow_xfer_clear   = 0,
		.dither_timer_period = 0,
		.dither_duty_cycle   = 0,
		.prescaler_mode      = XMC_CCU4_SLICE_PRESCALER_MODE_NORMAL,
		.mcm_enable          = 0,
		.prescaler_initval   = 0,
		.float_limit         = 0,
		.dither_limit        = 0,
		.passive_level       = XMC_CCU4_SLICE_OUTPUT_PASSIVE_LEVEL_LOW,
		.timer_concatenation = 0
	};

	const XMC_GPIO_CONFIG_t gpio_out_config	= {
		.mode                = XMC_GPIO_MODE_OUTPUT_PUSH_PULL_ALT9,
		.input_hysteresis    = XMC_GPIO_INPUT_HYSTERESIS_STANDARD,
		.output_level        = XMC_GPIO_OUTPUT_LEVEL_LOW,
	};

    XMC_CCU4_Init(CCU41, XMC_CCU4_SLICE_MCMS_ACTION_TRANSFER_PR_CR);
    XMC_CCU4_StartPrescaler(CCU41);
    XMC_CCU4_SLICE_CompareInit(slice[ccu4_slice_number], &compare_config);

    // Set the period and compare register values
    XMC_CCU4_SLICE_SetTimerPeriodMatch(slice[ccu4_slice_number], period_value);
    XMC_CCU4_SLICE_SetTimerCompareMatch(slice[ccu4_slice_number], 0);

    XMC_CCU4_EnableShadowTransfer(CCU41, (XMC_CCU4_SHADOW_TRANSFER_SLICE_0 << (ccu4_slice_number*4)) |
    		                             (XMC_CCU4_SHADOW_TRANSFER_PRESCALER_SLICE_0 << (ccu4_slice_number*4)));

    XMC_GPIO_Init(port, pin, &gpio_out_config);

    XMC_CCU4_EnableClock(CCU41, ccu4_slice_number);
    XMC_CCU4_SLICE_StartTimer(slice[ccu4_slice_number]);
}

void led_init(void) {
	const XMC_GPIO_CONFIG_t output_config = {
		.mode         = XMC_GPIO_MODE_OUTPUT_PUSH_PULL,
		.output_level = XMC_GPIO_OUTPUT_LEVEL_HIGH,
	};

	XMC_GPIO_Init(LED_R_PIN, &output_config);
	XMC_GPIO_Init(LED_G_PIN, &output_config);
	XMC_GPIO_Init(LED_B_PIN, &output_config);

	led_ccu4_pwm_init(LED_R_PIN, LED_R_CCU4_SLICE, LED_PERIOD_VALUE-1);
	led_ccu4_pwm_init(LED_G_PIN, LED_G_CCU4_SLICE, LED_PERIOD_VALUE-1);
	led_ccu4_pwm_init(LED_B_PIN, LED_B_CCU4_SLICE, LED_PERIOD_VALUE-1);

	memset(&led, 0, sizeof(LED));
	led.use_rgb = true;
}

void led_hsv_to_rgb(const uint16_t h, const uint8_t s, const uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b) {
    if(s == 0) {
        *r = v;
		*g = v;
		*b = v;
    } else {
        const uint8_t i = h / 60;
        const uint8_t p = (256*v - s*v) / 256;

        if(i & 1) {
            const int32_t q = (256*60*v - h*s*v + 60*s*v*i) / (256*60);
            switch(i) {
                case 1: *r = q; *g = v; *b = p; break;
                case 3: *r = p; *g = q; *b = v; break;
                case 5: *r = v; *g = p; *b = q; break;
            }
        } else {
            const int32_t t = (256*60*v + h*s*v - 60*s*v*(i+1)) / (256*60);
            switch(i) {
                case 0: *r = v; *g = t; *b = p; break;
                case 2: *r = p; *g = v; *b = t; break;
                case 4: *r = t; *g = p; *b = v; break;
            }
        }
    }
}

void led_tick(void) {
	static uint8_t last_r = 0;
	static uint8_t last_g = 0;
	static uint8_t last_b = 0;

	uint8_t set_r = 0;
	uint8_t set_g = 0;
	uint8_t set_b = 0;

	if(led.use_rgb) {
		set_r = led.r;
		set_g = led.g;
		set_b = led.b;
	} else {
		if(led.pattern != WARP_ENERGY_MANAGER_LED_PATTERN_OFF) {
			if(led.pattern == WARP_ENERGY_MANAGER_LED_PATTERN_ON) {
				led_hsv_to_rgb(led.hue, 255, 255, &set_r, &set_g, &set_b);
			} else if(led.pattern == WARP_ENERGY_MANAGER_LED_PATTERN_BLINKING) {
				/*if(led.blink_count >= led.blink_num) {
					if(system_timer_is_time_elapsed_ms(led.blink_last_time, LED_BLINK_DURATION_WAIT)) {
						led.blink_last_time = system_timer_get_ms();
						led.blink_count = 0;
					}
				} else */if(led.blink_on) {
					if(system_timer_is_time_elapsed_ms(led.blink_last_time, LED_BLINK_DURATION_ON)) {
						led.blink_last_time = system_timer_get_ms();
						led.blink_on = false;
						led.blink_count++;

						set_r = 0;
						set_g = 0;
						set_b = 0;
					} else {
						return;
					}
				} else {
					if(system_timer_is_time_elapsed_ms(led.blink_last_time, LED_BLINK_DURATION_OFF)) {
						led.blink_last_time = system_timer_get_ms();
						led.blink_on = true;

						led_hsv_to_rgb(led.hue, 255, 255, &set_r, &set_g, &set_b);
					} else {
						return;
					}
				}
			} else if(led.pattern == WARP_ENERGY_MANAGER_LED_PATTERN_BREATHING) {
				if(!system_timer_is_time_elapsed_ms(led.breathing_time, 5)) {
					return;
				}
				led.breathing_time = system_timer_get_ms();

				if(led.breathing_up) {
					led.breathing_index += 1;
				} else {
					led.breathing_index -= 1;
				}
				led.breathing_index = BETWEEN(0, led.breathing_index, 255);

				if(led.breathing_index == 0) {
					led.breathing_up = true;
				} else if(led.breathing_index == 255) {
					led.breathing_up = false;
				}
				led_hsv_to_rgb(led.hue, 255, led.breathing_index, &set_r, &set_g, &set_b);
			} else {
				logw("Unknown pattern: %d\n\r", led.pattern);
			}
		}
	}

	if(set_r != last_r) {
		led_ccu4_pwm_set_duty_cycle(LED_R_CCU4_SLICE, cie1931[set_r]);
		last_r = set_r;
	}
	if(set_g != last_g) {
		led_ccu4_pwm_set_duty_cycle(LED_G_CCU4_SLICE, cie1931[set_g]);
		last_g = set_g;
	}
	if(set_b != last_b) {
		led_ccu4_pwm_set_duty_cycle(LED_B_CCU4_SLICE, cie1931[set_b]);
		last_b = set_b;
	}
}