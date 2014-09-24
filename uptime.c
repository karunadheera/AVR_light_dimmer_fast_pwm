/*
 wiring.c - Partial implementation of the Wiring API for the ATmega8.
 Part of Arduino - http://www.arduino.cc/

 Copyright (c) 2005-2006 David A. Mellis

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General
 Public License along with this library; if not, write to the
 Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 Boston, MA  02111-1307  USA

 $Id$
 */

#include <avr/io.h>
#include <inttypes.h>
#include <avr/interrupt.h>
#include "uptime.h"

/* some vars */
uint64_t _millis = 0;
uint16_t _8000us = 0;

/* interrupts routines */
// timer overflow occur every 0.256 ms
ISR(TIM0_OVF_vect) {
  _8000us += 2048;
  while (_8000us > 1000) {
    _millis++;
    _8000us -= 1000;
  }
}

// safe access to millis counter
uint64_t millis(void) {
  uint64_t m;
  cli();
  m = _millis;
  sei();
  return m;
}

void init_uptime(void) {
	/* interrup setup */
	  // prescale timer0 to 1/8th the clock rate
	  // overflow timer0 every 0.256 ms
	  TCCR0B |= (1<<CS01);
	  // enable timer overflow interrupt
	  TIMSK  |= 1<<TOIE0;

	  // Enable global interrupts
	  sei();
}
