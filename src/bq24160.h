
#ifndef BQ24160_H
#define BQ24160_H

#include <stdint.h>

void bq24160_init(void);

uint16_t bq24160_read_voltage(void);

#endif /* BQ24160_H */
