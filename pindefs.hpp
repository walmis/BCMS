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


//// LCD Definitions ///////


GPIO__OUTPUT(lcd_e, 1, 9);
GPIO__OUTPUT(lcd_rs, 1, 14);
GPIO__OUTPUT(lcd_rw, 1, 10);

GPIO__IO(lcd_d4, 1, 8);
GPIO__IO(lcd_d5, 1, 4);
GPIO__IO(lcd_d6, 1, 1);
GPIO__IO(lcd_d7, 1, 0);


typedef xpcc::gpio::Nibble<lcd_d7, lcd_d6, lcd_d5, lcd_d4> lcd_data;

#endif /* PINDEFS_HPP_ */
