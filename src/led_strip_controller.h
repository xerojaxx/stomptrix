/*
 * led_strip_controller.h
 *
 *  Created on: Jun 18, 2017
 *      Author: Mark
 */

#ifndef SRC_LED_STRIP_CONTROLLER_H_
#define SRC_LED_STRIP_CONTROLLER_H_

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define ERROR_TRYING_TO_SET_TOO_MANY_LEDS -1
#define ERROR_BUSY_SETTING_LEDS -2

typedef uint32_t colour_rgb_t;

typedef int (*led_strip_driver_set_colours_t)(uint16_t num_of_leds, colour_rgb_t led_colour_values[]);



typedef struct
{
	uint16_t num_of_leds;
	led_strip_driver_set_colours_t set_colours;

}led_strip_handle_t;



bool led_strip_init( led_strip_handle_t* strip_handler);
void led_strip_slide_animation(led_strip_handle_t* strip_handler, colour_rgb_t colour, uint8_t single);
bool led_strip_set_all(led_strip_handle_t* strip_handler, colour_rgb_t colour);

#endif /* SRC_LED_STRIP_CONTROLLER_H_ */
