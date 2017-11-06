/*
 * led_strip_controller.c
 *
 *  Created on: Jun 18, 2017
 *      Author: Mark
 */
#include "led_strip_controller.h"

#include "timer.h"

#define PULSE_INTENSITY_STEP_SIZE 255u
#define PULSE_INTENSITY_MAX 255uL

enum{
	state_static = 0u,
	state_pulsing,
	state_sliding,
}strip_animation_state;

static uint16_t pulsing_intensity = 0u;
static uint8_t cur_slide_led_index = 0u;
static enum{brightness_increasing = 0u, brightness_decreasing}brightness_direction = brightness_increasing;
static led_strip_handle_t* cur_led_strip = NULL;
static colour_rgb_t slide_colour = 0x00000000u;
static bool single_not_continuous = true;



static colour_rgb_t led_apply_brightness_factor(colour_rgb_t slide_colour, uint8_t pulsing_intensity)
{
	colour_rgb_t adjusted_colour = 0u;
	uint16_t green = (((slide_colour >> 16u) & 0xFFuL) * (uint16_t)pulsing_intensity) / PULSE_INTENSITY_MAX;
	uint16_t red = (((slide_colour >> 8u) & 0xFFuL) * (uint16_t)pulsing_intensity) / PULSE_INTENSITY_MAX;
	uint16_t blue = (((slide_colour) & 0xFFuL) * (uint16_t)pulsing_intensity) / PULSE_INTENSITY_MAX;

	if(green > PULSE_INTENSITY_MAX)green = PULSE_INTENSITY_MAX;
	if(red > PULSE_INTENSITY_MAX)red = PULSE_INTENSITY_MAX;
	if(blue > PULSE_INTENSITY_MAX)blue = PULSE_INTENSITY_MAX;

	adjusted_colour = (green << 16u) | (red << 8u) | (blue);

	return adjusted_colour;
}

static bool slide_animation_process(void)
{
	if(cur_slide_led_index < cur_led_strip->num_of_leds)
	{
		if(brightness_direction == brightness_increasing)
		{
			pulsing_intensity += PULSE_INTENSITY_STEP_SIZE;
			if(pulsing_intensity <= PULSE_INTENSITY_MAX)
			{
				//
			}
			else
			{
				brightness_direction = brightness_decreasing;
			}
		}
		else
		{
			pulsing_intensity -= PULSE_INTENSITY_STEP_SIZE;
			if(pulsing_intensity > PULSE_INTENSITY_STEP_SIZE)
			{
				//
			}
			else
			{
				brightness_direction = brightness_increasing;
				cur_slide_led_index++;
			}
		}

		colour_rgb_t colours[cur_led_strip->num_of_leds];

		for (uint32_t i=0u; i < cur_led_strip->num_of_leds; i++)
		{
			if(i == cur_slide_led_index)
			{
				colours[i] = led_apply_brightness_factor(slide_colour, pulsing_intensity);//slide_colour;//
			}
			else
			{
				colours[i] = 0x00000000u;
			}
		}

		if( cur_led_strip->set_colours != NULL)
		{
			cur_led_strip->set_colours(cur_led_strip->num_of_leds, colours);
		}

	}
	else
	{
		// All done.
		return true;
	}


	return false;
}

static void on_10ms_tick(void)
{
	switch(strip_animation_state)
	{
	case state_static:
		break;
	case state_pulsing:
		break;
	case state_sliding:
		if(slide_animation_process())
		{
			if(single_not_continuous)
			{
				strip_animation_state = state_static;
			}
			else
			{
				cur_slide_led_index = 0u;
			}
		}
		break;
	default:
		break;
	}
}

bool led_strip_init( led_strip_handle_t* strip_handler)
{
	timer_register_10_ms_callback(on_10ms_tick);

	return true;
}

void led_strip_slide_animation(led_strip_handle_t* strip_handler, colour_rgb_t colour, uint8_t single)
{
	if (strip_handler == NULL)
	{
		return;
	}
	cur_led_strip = strip_handler;
	strip_animation_state = state_sliding;
	brightness_direction = brightness_increasing;
	cur_slide_led_index = 0u;
	slide_colour = colour;
	single_not_continuous = (single == 1u);
}

bool led_strip_set_all(led_strip_handle_t* strip_handler, colour_rgb_t colour)
{
	colour_rgb_t colours[strip_handler->num_of_leds];

	for (uint32_t i=0u; i < strip_handler->num_of_leds; i++)
	{
		colours[i] = colour;
	}
	if (strip_handler == NULL)
	{
		return false;
	}
	if( strip_handler->set_colours != NULL)
	{
		return (strip_handler->set_colours(strip_handler->num_of_leds, colours) == 0u);
	}
}


