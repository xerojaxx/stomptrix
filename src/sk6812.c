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
#include "dmadrv.h"
#include "em_ldma.h"

#define SK_TIMER TIMER1
#define SK_TIMER_CC_CH 0u
#define DMA_CHANNEL_TIMER 0

// 1.25us per bit. PWM 0.3us high time for low bit, 0.6us for high bit.
#define SK6812_BIT_PERIOD_NS 1250u
#define SK6812_BIT_0_NS 300u
#define SK6812_BIT_1_NS 600u
#define SK6812_RESET_PERIOD_NS 50000u

#define MAX_LEDS 60u


static uint16_t ticks_per_bit_period;
static uint16_t cc_ticks_for_bit_0;
static uint16_t cc_ticks_for_bit_1;
static uint16_t reset_period_ticks;

static uint16_t total_bits_to_write;
static uint16_t current_bit_to_write;
static uint8_t cc_timings_buffer[(MAX_LEDS+1u)*3u*8u];
static volatile bool busy = false;

static unsigned int dma_ch_id;

bool timer_dma_callback( unsigned int channel,
						   unsigned int sequenceNo,
						   void *userParam )
{
	TIMER_Enable(SK_TIMER, false);
	SK_TIMER->ROUTEPEN &= ~TIMER_ROUTEPEN_CC0PEN;
	busy = false;
	return false;
}

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

	TIMER_Init_TypeDef timer_settings = TIMER_INIT_DEFAULT;
	timer_settings.enable = false;
	timer_settings.dmaClrAct = true;
	//timer_settings.
	TIMER_Init(SK_TIMER, &timer_settings);
	NVIC_EnableIRQ(TIMER1_IRQn);

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

	(void)DMADRV_AllocateChannel( &dma_ch_id, NULL );

}

int sk6812_set_colours(uint16_t num_of_leds, colour_rgb_t led_colour_values[])
{
	if (num_of_leds > MAX_LEDS)
	{
		return ERROR_TRYING_TO_SET_TOO_MANY_LEDS;
	}
	if(busy)
	{
		return ERROR_BUSY_SETTING_LEDS;
	}
	busy = true;
	total_bits_to_write = (num_of_leds) * 3u * 8u;
	current_bit_to_write = 24u;
	uint8_t current_led_to_write = 0u;

	colour_rgb_t temp;
	for(uint16_t i=0; i<num_of_leds; i++)
	{
		temp = led_colour_values[i];
		led_colour_values[i] = (temp & 0x000000FF) |
				((temp & 0x00FF0000)>>8u) |
				((temp & 0x0000FF00)<<8u);
	}

	// Pre-load all the CC timings into a massive buffer.
	for(uint16_t i=0u; i<(total_bits_to_write+1); i++)
	{
		if(i >= total_bits_to_write)
		{
			// Setup Zeros after the end because timer is not stopped in time.
			cc_timings_buffer[i] = cc_ticks_for_bit_0;
		}
		else
		{
			bool next_bit = (led_colour_values[current_led_to_write] & (1 << (current_bit_to_write-1u))) > 0u;
			cc_timings_buffer[i] = next_bit ? cc_ticks_for_bit_1 : cc_ticks_for_bit_0;
		}
		current_bit_to_write--;
		if (current_bit_to_write == 0u)
		{
			current_bit_to_write = 24u;
			current_led_to_write++;
		}
	}

	if(DMADRV_MemoryPeripheral( dma_ch_id,
							 dmadrvPeripheralSignal_TIMER1_UFOF,
							 &SK_TIMER->CC[SK_TIMER_CC_CH].CCV,
							 &cc_timings_buffer[0u],
							 true,
							 (int)(total_bits_to_write+1),
							 dmadrvDataSize1,
							 timer_dma_callback,
							 NULL ) != ECODE_OK)
	{
		busy = false;
		return 0;
	}

	TIMER_CompareSet(SK_TIMER, SK_TIMER_CC_CH, cc_timings_buffer[0]);
	TIMER_CounterSet(SK_TIMER, 0u);

	SK_TIMER->ROUTEPEN |= TIMER_ROUTEPEN_CC0PEN;
	TIMER_Enable(SK_TIMER, true);

	return 0;
}
