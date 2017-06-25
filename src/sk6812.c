/*
 * sk6812.c - https://www.pololu.com/file/0J1233/sk6812_datasheet.pdf
 *
 *  Created on: Jun 17, 2017
 *      Author: Mark
 */
#include "sk6812.h"

#include <string.h>
#include "em_cmu.h"
#include "em_timer.h"
#include "em_gpio.h"

#define SK_TIMER TIMER1
#define SK_TIMER_CC_CH 0u

#define SK6812_BIT_PERIOD_NS 5000u //1250u
#define SK6812_BIT_0_NS 300u
#define SK6812_BIT_1_NS 600u
#define SK6812_RESET_PERIOD_NS 100000u

#define MAX_LEDS 50u

static colour_rgb_t colours_to_set[MAX_LEDS] = {0u};

static uint32_t total_leds_to_set;
static uint8_t current_bit_to_write;
static uint8_t current_led_to_write;

static bool sending_last_bit = false;
static uint16_t ticks_per_bit_period;
static uint16_t cc_ticks_for_bit_0;
static uint16_t cc_ticks_for_bit_1;
static uint16_t reset_period_ticks;

void TIMER1_IRQHandler(void)
{
	TIMER_IntClear(SK_TIMER, TIMER_IF_CC0 | TIMER_IF_OF); // Clear interrupt source


	if (sending_last_bit)
	{
		sending_last_bit = false;
		TIMER_Enable(SK_TIMER, false);
		TIMER_IntDisable(SK_TIMER, TIMER_IF_CC0);
		SK_TIMER->ROUTEPEN &= ~TIMER_ROUTEPEN_CC0PEN;
		return;
	}

	current_bit_to_write--;
	bool next_bit = (colours_to_set[current_led_to_write] & (1 << current_bit_to_write)) > 0u;
	TIMER_CompareSet(SK_TIMER, SK_TIMER_CC_CH, next_bit ? cc_ticks_for_bit_1 : cc_ticks_for_bit_0);

	if (current_bit_to_write == 0u)
	{
		if (current_led_to_write == (total_leds_to_set-1u))
		{
			sending_last_bit = true;
		}
		else
		{
			// Start next LED data
			current_led_to_write++;
			current_bit_to_write = 24u;
		}
	}
}

// 1.25us per bit. PWM 0.3us high time for low bit, 0.6us for high bit.

void sk6812_init()
{
#define NS_IN_A_SECOND (1000000000uL)

	uint32_t clk_speed_hz = CMU_ClockFreqGet(cmuClock_TIMER1);

	uint32_t ns_per_tick = (NS_IN_A_SECOND / clk_speed_hz);
	ticks_per_bit_period = SK6812_BIT_PERIOD_NS / ns_per_tick;

	cc_ticks_for_bit_0 = SK6812_BIT_0_NS / ns_per_tick;
	cc_ticks_for_bit_1 = SK6812_BIT_1_NS / ns_per_tick;

	reset_period_ticks = SK6812_RESET_PERIOD_NS / ns_per_tick;

	CMU_ClockEnable(cmuClock_TIMER1, true);
	CMU_ClockEnable(cmuClock_GPIO, true);
	GPIO_PinModeSet(gpioPortF, 2, gpioModePushPull, 0);

//	static volatile uint32_t delay = 100000u;
//	while(--delay>0u){}

	TIMER_Init_TypeDef timer_settings = TIMER_INIT_DEFAULT;
	timer_settings.enable = false;
	//timer_settings.
	TIMER_Init(SK_TIMER, &timer_settings);
	NVIC_EnableIRQ(TIMER1_IRQn);
	//NVIC_SetPriority(TIMER1_IRQn, 0u);

	TIMER_TopSet(SK_TIMER, ticks_per_bit_period);
	SK_TIMER->ROUTELOC0 |= TIMER_ROUTELOC0_CC0LOC_LOC26;

	// Setup Timer Channel Configuration for PWM
	TIMER_InitCC_TypeDef timerCCInit = {
		.eventCtrl  = timerEventEveryEdge,    // This value will be ignored since we aren't using input capture
		.edge       = timerEdgeNone,          // This value will be ignored since we aren't using input capture
		.prsSel     = timerPRSSELCh0,         // This value will be ignored since we aren't using PRS
		.cufoa      = timerOutputActionNone,  // No action on underflow (up-count mode)
		.cofoa      = timerOutputActionSet,   // On overflow, we want the output to go high, but in PWM mode this should happen automatically
		.cmoa       = timerOutputActionClear, // On compare match, we want output to clear, but in PWM mode this should happen automatically
		.mode       = timerCCModeCompare,         // Set timer channel to run in PWM mode
		.filter     = false,                  // Not using input, so don't need a filter
		.prsInput   = false,                  // Not using PRS
		.coist      = false,                  // Initial state for PWM is high when timer is enabled
		.outInvert  = false,                  // non-inverted output
	};
	TIMER_InitCC(SK_TIMER, SK_TIMER_CC_CH, &timerCCInit);
}

int sk6812_set_colours(uint16_t num_of_leds, colour_rgb_t led_colour_values[])
{
	if (num_of_leds > MAX_LEDS)
	{
		return ERROR_TRYING_TO_SET_TOO_MANY_LEDS;
	}

	memcpy(colours_to_set, led_colour_values, sizeof(colour_rgb_t)*num_of_leds);
	total_leds_to_set = num_of_leds;
	current_bit_to_write = 23u;
	current_led_to_write = 0u;

	TIMER_CounterSet(SK_TIMER, 0u);
	TIMER_IntEnable(SK_TIMER, TIMER_IF_CC0);

	bool next_bit = (colours_to_set[current_led_to_write] & (1 << current_bit_to_write)) > 0u;
	TIMER_CompareSet(SK_TIMER, SK_TIMER_CC_CH, next_bit ? cc_ticks_for_bit_1 : cc_ticks_for_bit_0);

	SK_TIMER->ROUTEPEN |= TIMER_ROUTEPEN_CC0PEN;
	TIMER_Enable(SK_TIMER, true);

	return 0;
}
