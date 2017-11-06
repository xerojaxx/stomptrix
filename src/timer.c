/*
 * timer.c
 *
 *  Created on: Jul 2, 2017
 *      Author: Mark
 */

#include "timer.h"
#include <stddef.h>

static timer_callback_t callback = NULL;
void timer_register_10_ms_callback(timer_callback_t cb)
{
	callback = cb;
}

void timer_10ms_tick(void)
{
	if(callback != NULL)
	{
		callback();
	}
}
