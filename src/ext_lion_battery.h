
#ifndef SRC_EXT_LION_BATTERY_H_
#define SRC_EXT_LION_BATTERY_H_


#include <stdint.h>

uint16_t battery_get_millvolts(void);
void lion_battery_init(void);
uint8_t battery_get_percentage(void);

#endif /* SRC_EXT_LION_BATTERY_H_ */
