/*
 * pindefs.hpp
 *
 *  Created on: Mar 15, 2013
 *      Author: walmis
 */

#ifndef PINDEFS_HPP_
#define PINDEFS_HPP_

#include <xpcc/architecture.hpp>


GPIO__INPUT(progPin, 1, 22);

GPIO__OUTPUT(usbConnPin, 1, 18);

/// MCP4151 definitions ////

GPIO__OUTPUT(pot_cs, 2, 1);


//// LCD Definitions ///////

GPIO__OUTPUT(lcd_e, 1, 9);
GPIO__OUTPUT(lcd_rs, 1, 14);
GPIO__OUTPUT(lcd_rw, 1, 10);

GPIO__IO(lcd_d4, 1, 8);
GPIO__IO(lcd_d5, 1, 4);
GPIO__IO(lcd_d6, 1, 1);
GPIO__IO(lcd_d7, 1, 0);

typedef xpcc::gpio::Nibble<lcd_d7, lcd_d6, lcd_d5, lcd_d4> lcd_data;

//// Buttons //////
GPIO__INPUT(btn_down, 1, 19);
GPIO__INPUT(btn_left, 1, 23);
GPIO__INPUT(btn_right, 1, 20);
GPIO__INPUT(btn_up, 1, 22);
typedef xpcc::gpio::Nibble<btn_right, btn_left, btn_up, btn_down> buttonsNibble;

//sd card chip select
GPIO__OUTPUT(sdCs, 0, 16);

GPIO__IO(_psuEn, 4, 28);
typedef xpcc::gpio::OpenDrain<_psuEn> psuEn;

GPIO__OUTPUT(dischargerEn, 0, 6);

GPIO__OUTPUT(fanEn, 2, 3);

GPIO__INPUT(tempPwm, 2, 6);

#endif /* PINDEFS_HPP_ */
