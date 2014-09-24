/*
 * uptime.h
 *
 *  Created on: 29 Apr, 2014
 *      Author: prageeth
 */

#ifndef UPTIME_H_
#define UPTIME_H_


#include <avr/io.h>
#include <inttypes.h>
#include <util/delay.h>
#include <avr/interrupt.h>

void init_uptime(void);
uint64_t millis(void);

#endif /* UPTIME_H_ */
