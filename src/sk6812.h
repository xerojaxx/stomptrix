/*
 * sk6812.h
 *
 *  Created on: Jun 18, 2017
 *      Author: Mark
 */

#ifndef SRC_SK6812_H_
#define SRC_SK6812_H_

#include "led_strip_controller.h"


void sk6812_init();

int sk6812_set_colours(uint16_t num_of_leds, colour_rgb_t led_colour_values[]);

#endif /* SRC_SK6812_H_ */
