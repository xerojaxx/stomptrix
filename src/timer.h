/*
 * timer.h
 *
 *  Created on: Jul 2, 2017
 *      Author: Mark
 */

#ifndef SRC_TIMER_H_
#define SRC_TIMER_H_

#include <stdbool.h>
#include <stdint.h>

typedef void(*timer_callback_t)(void);

void timer_register_10_ms_callback(timer_callback_t cb);
void timer_10ms_tick(void);


#endif /* SRC_TIMER_H_ */
