/*
 * led_strip_controller.c
 *
 *  Created on: Jun 18, 2017
 *      Author: Mark
 */
#include "led_strip_controller.h"


bool led_strip_init( led_strip_handle_t* strip_handler)
{


}


void led_strip_set_all(led_strip_handle_t* strip_handler, colour_rgb_t colour)
{
	colour_rgb_t colours[strip_handler->num_of_leds];

	for (uint32_t i=0u; i < strip_handler->num_of_leds; i++)
	{
		colours[i] = colour;
	}
	if (strip_handler == NULL)
	{
		return;
	}
	if( strip_handler->set_colours != NULL)
	{
		strip_handler->set_colours(strip_handler->num_of_leds, colours);
	}


}
