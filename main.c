/*
 * main.c
 *
 *  Created on: 7 May, 2014
 *      Author: prageeth
 */

/*
 io.h provides lots of handy constants
 delay.h provides _delay_ms and _delay_us functions
 */
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <avr/eeprom.h>
#include "irmp.h"
#include "timer0.h"
#include "uptime.h"

void eeprom_write_int16(uint8_t *addr1, uint8_t *addr2, uint16_t val);
void eeprom_write_int32(uint8_t *addr1, uint8_t *addr2, uint8_t *addr3,
		uint8_t *addr4, uint32_t val);
uint16_t eeprom_read_int16(uint8_t *addr1, uint8_t *addr2);
uint32_t eeprom_read_int32(uint8_t *addr1, uint8_t *addr2, uint8_t *addr3,
		uint8_t *addr4);

// IR PROTOCOLs
enum {
	RC5_MUTE = 0x000d,
	RC5_TV_AV = 0x000b,
	RC5_VOL_DOWN = 0x0011,
	RC5_VOL_UP = 0x0010,
	RC5_POWER = 0x000c,
	NEC_VOL_DOWN = 0x000c,
	NEC_VOL_UP = 0x000a,
	NEC_PREV = 0x0009,
	NEC_NEXT = 0x0006,
	NEC_CENTER = 0x005c,
	NEC_MENU = 0x0003,
	NEC_PLAY_PAUSE = 0x005f
};

uint64_t ir_millis = 0;
uint64_t last_ir_millis = 0;
uint64_t blink_millis = 0;
uint8_t blink = 0;
uint8_t ir_cont = 0;
/*
 program entry-point
 */
int main(void) {
	/*
	 Starting values for red, green and blue
	 */
	uint8_t pwm_val, power;

	/*
	 Port B Data Direction Register (controls the mode of all pins within port B)
	 DDRB is 8 bits: [unused:unused:DDB5:DDB4:DDB3:DDB2:DDB1:DDB0]
	 1<<DDB4: sets bit DDB4 (data-direction, port B, pin 4), which puts PB4 (port B, pin 4) in output mode
	 1<<DDB1: sets bit DDB1 (data-direction, port B, pin 1), which puts PB1 (port B, pin 1) in output mode
	 1<<DDB0: sets bit DDB0 (data-direction, port B, pin 0), which puts PB0 (port B, pin 0) in output mode
	 */
	DDRB = 1 << DDB4;

	DDRB |= 1 << PB0;

	/*
	 Control Register A for Timer/Counter-0 (Timer/Counter-0 is configured using two registers: A and B)
	 TCCR0A is 8 bits: [COM0A1:COM0A0:COM0B1:COM0B0:unused:unused:WGM01:WGM00]
	 2<<COM0A0: sets bits COM0A0 and COM0A1, which (in Fast PWM mode) clears OC0A on compare-match, and sets OC0A at BOTTOM
	 2<<COM0B0: sets bits COM0B0 and COM0B1, which (in Fast PWM mode) clears OC0B on compare-match, and sets OC0B at BOTTOM
	 3<<WGM00: sets bits WGM00 and WGM01, which (when combined with WGM02 from TCCR0B below) enables Fast PWM mode
	 */
//	TCCR0A = 2 << COM0A0 | 2 << COM0B0 | 3 << WGM00;
	TCCR0A = 2 << COM0B0 | 3 << WGM00;

	/*
	 Control Register B for Timer/Counter-0 (Timer/Counter-0 is configured using two registers: A and B)
	 TCCR0B is 8 bits: [FOC0A:FOC0B:unused:unused:WGM02:CS02:CS01:CS00]
	 0<<WGM02: bit WGM02 remains clear, which (when combined with WGM00 and WGM01 from TCCR0A above) enables Fast PWM mode
	 1<<CS00: sets bits CS01 (leaving CS01 and CS02 clear), which tells Timer/Counter-0 to not use a prescalar
	 */
	TCCR0B = 0 << WGM02 | 1 << CS00;

	/*
	 Control Register for Timer/Counter-1 (Timer/Counter-1 is configured with just one register: this one)
	 TCCR1 is 8 bits: [CTC1:PWM1A:COM1A1:COM1A0:CS13:CS12:CS11:CS10]
	 0<<PWM1A: bit PWM1A remains clear, which prevents Timer/Counter-1 from using pin OC1A (which is shared with OC0B)
	 0<<COM1A0: bits COM1A0 and COM1A1 remain clear, which also prevents Timer/Counter-1 from using pin OC1A (see PWM1A above)
	 1<<CS10: sets bit CS11 which tells Timer/Counter-1  to not use a prescalar
	 */
	TCCR1 = 0 << PWM1A | 0 << COM1A0 | 1 << CS10;

	/*
	 General Control Register for Timer/Counter-1 (this is for Timer/Counter-1 and is a poorly named register)
	 GTCCR is 8 bits: [TSM:PWM1B:COM1B1:COM1B0:FOC1B:FOC1A:PSR1:PSR0]
	 1<<PWM1B: sets bit PWM1B which enables the use of OC1B (since we disabled using OC1A in TCCR1)
	 2<<COM1B0: sets bit COM1B1 and leaves COM1B0 clear, which (when in PWM mode) clears OC1B on compare-match, and sets at BOTTOM
	 */
	GTCCR = 1 << PWM1B | 2 << COM1B0;

	IRMP_DATA irmp_data;
	irmp_init();                                              // initialize irmp
	timer1_init();                                          // initialize timer1
	init_uptime();
	sei ();

	power = eeprom_read_byte((uint8_t*) 0);
	if (power == 0xff) {
		power = 0x01;
	}
	pwm_val = eeprom_read_byte((uint8_t*) 1);
	if (eeprom_read_byte((uint8_t*) 2) == 0xff && pwm_val == 0xff) {
		pwm_val = 0x01;
		eeprom_write_byte((uint8_t*) 2, 0x01);
	}
	uint8_t t_pwm_val = pwm_val;
	uint8_t t_power = power;

	/*
	 loop forever
	 */
	for (;;) {
		if (irmp_get_data(&irmp_data)) {
			PORTB |= (1 << PB0);
			switch (irmp_data.protocol) {
			case IRMP_RC5_PROTOCOL:
				switch (irmp_data.command) {
				case RC5_POWER:
					if (irmp_data.flags == 0) {
						power = power == 0x00 ? 0x01 : 0x00;
						if (power == 0x01 && pwm_val == 0x00) {
							pwm_val = 0x01;
						}
					}
					break;
				case RC5_VOL_UP:
					if (power == 0x01 && pwm_val < 0xff) {
						if (0xff - pwm_val < 0x01 + ir_cont) {
							pwm_val = 0xff;
						} else {
							pwm_val += (0x01 + ir_cont);
						}
					}
					break;
				case RC5_VOL_DOWN:
					if (power == 0x01 && pwm_val > 0x00) {
						if (pwm_val < 0x01 + ir_cont) {
							pwm_val = 0;
						} else {
							pwm_val -= (0x01 + ir_cont);
						}
					}
					break;
				}
				break;
			case IRMP_APPLE_PROTOCOL:
				switch (irmp_data.command) {
				case NEC_CENTER:
					if (irmp_data.flags == 0) {
						power = power == 0x00 ? 0x01 : 0x00;
						if (power == 0x01 && pwm_val == 0x00) {
							pwm_val = 0x01;
						}
					}
					break;
				case NEC_VOL_UP:
					if (power == 0x01 && pwm_val < 0xff) {
						if (0xff - pwm_val < 0x01 + ir_cont) {
							pwm_val = 0xff;
						} else {
							pwm_val += (0x01 + ir_cont);
						}
					}
					break;
				case NEC_VOL_DOWN:
					if (power == 0x01 && pwm_val > 0x00) {
						if (pwm_val < 0x01 + ir_cont) {
							pwm_val = 0;
						} else {
							pwm_val -= (0x01 + ir_cont);
						}
					}
					break;
				case NEC_NEXT:
					if (power == 0x01 && pwm_val < 0xff) {
						if (0xff - pwm_val < 0x01 + ir_cont) {
							pwm_val = 0xff;
						} else {
							pwm_val += (0x01 + ir_cont);
						}
					}
					break;
				case NEC_PREV:
					if (power == 0x01 && pwm_val > 0x00) {
						if (pwm_val < 0x01 + ir_cont) {
							pwm_val = 0;
						} else {
							pwm_val -= (0x01 + ir_cont);
						}
					}
					break;
				}
				break;
			}
			if (ir_millis > 0 && irmp_data.flags == 1) {
				ir_cont += 0x01;
			}
			ir_millis = millis();
			last_ir_millis = ir_millis;
		} else {
			if (ir_millis > 0 && millis() - ir_millis > 200) {
				PORTB &= ~(1 << PB0);
				ir_millis = 0;
				ir_cont = 0x00;
			}
		}

		if (power == 0x01) {
			OCR1B = pwm_val;
		} else {
			OCR1B = 0;
		}
		if ((millis() - last_ir_millis) > 60000) {
			if (t_pwm_val != pwm_val) {
				t_pwm_val = pwm_val;
				eeprom_write_byte((uint8_t*) 1, pwm_val);
				blink = 1;
			}
			if (t_power != power) {
				t_power = power;
				eeprom_write_byte((uint8_t*) 0, power);
				blink = 1;
			}
		}
		if (blink > 0 && millis() - blink_millis > 40) {
			PORTB = PORTB ^ (1 << PB0);
			blink += 1;
			if (blink == 16) {
				blink = 0;
			}
			blink_millis = millis();
		}
		if (blink == 0 && blink_millis > 0) {
			PORTB &= ~(1 << PB0);
			blink_millis = 0;
		}
	}
}

void eeprom_write_int16(uint8_t *addr1, uint8_t *addr2, uint16_t val) {
	eeprom_write_byte(addr1, (uint8_t) (val & 0xff));
	eeprom_write_byte(addr2, (uint8_t) ((val & 0xff00) >> 8));
}

void eeprom_write_int32(uint8_t *addr1, uint8_t *addr2, uint8_t *addr3,
		uint8_t *addr4, uint32_t val) {
	eeprom_write_int16(addr1, addr2, (uint16_t) (val & 0xffff));
	eeprom_write_int16(addr2, addr3, (uint16_t) ((val & 0xffff0000) >> 16));
}

uint16_t eeprom_read_int16(uint8_t *addr1, uint8_t *addr2) {
	uint16_t val;
	val = eeprom_read_byte(addr1);
	val |= ((uint16_t) eeprom_read_byte(addr2) << 8);
	return val;
}

uint32_t eeprom_read_int32(uint8_t *addr1, uint8_t *addr2, uint8_t *addr3,
		uint8_t *addr4) {
	uint32_t val;
	val = eeprom_read_int16(addr1, addr2);
	val |= ((uint32_t) eeprom_read_int16(addr3, addr4) << 16);
	return val;
}
