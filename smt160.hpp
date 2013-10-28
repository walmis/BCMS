/*
 * smt160.hpp
 *
 *  Created on: Oct 22, 2013
 *      Author: walmis
 */

#ifndef SMT160_HPP_
#define SMT160_HPP_

#include <xpcc/architecture.hpp>
#include "pindefs.hpp"
#include "Filters.h"

using namespace xpcc;
using namespace xpcc::lpc17;



template <typename Pin>
class SMT160 : TickerTask {
public:
	void init() {
		Pin::setInput();

		PWM::initTimer(1);

		PWM::configureCapture(0,
				(PWM::CaptureFlags::FALLING_EDGE
						| PWM::CaptureFlags::RISING_EDGE
						| PWM::CaptureFlags::INT_ON_CAPTURE));

		tl = 0;
		th = 0;
		temp = 0;
		Pinsel::setFunc(2, 6, 1);

		PWM::counterEnable();

	}

	void handleTick() override {
		static PeriodicTimer<> t(250);
		if(t.isExpired()) {

			//enable PCAP0[0]
			Pinsel::setFunc(2, 6, 1);

			tl = 0;
			th = 0;
		}
	}

	void handleInterrupt(int irqn) override {

		if(irqn == PWM1_IRQn) {


			if(PWM::getIntStatus(PWM::IntType::PWM_INTSTAT_CAP0)) {

				if(Pin::read() == 0) {
					tl = PWM::getCaptureValue(0);
				} else {

					if(tl > th && tl != 0 && th != 0) {
						thigh = tl - th;

						th = PWM::getCaptureValue(0);

						tlow = th - tl;

						float duty = (float)thigh / (tlow + thigh);
						temp << (400/47.0)*(25*duty - 8);

						//disable PCAP1[0]
						Pinsel::setFunc(2, 6, 0);
					} else {
						th = PWM::getCaptureValue(0);
					}

				}

				PWM::clearIntPending(PWM::IntType::PWM_INTSTAT_CAP0);
			}

		}
	}

	float readTemperature() {
		return temp.getValue();
	}

private:

	IIRFilter<float, 10> temp;

	volatile int32_t thigh, tlow;
	volatile uint32_t tl = 0, th = 0;

};


#endif /* SMT160_HPP_ */
