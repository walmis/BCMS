/*
 * discharger.hpp
 *
 *  Created on: Oct 19, 2013
 *      Author: walmis
 */

#ifndef DISCHARGER_HPP_
#define DISCHARGER_HPP_

#include <xpcc/architecture.hpp>
#include "pindefs.hpp"
#include "Filters.h"
#include "smt160.hpp"
#include "battery.hpp"

using namespace xpcc;
using namespace xpcc::lpc17;

#define DISCHARGE_CURRENT_CH 7

class Discharger : TickerTask {
public:
	Discharger() {

		//enable dac
		DAC::init();
		DAC::setValue(0);

		//configure pins
		dischargerEn::setOutput(false);

		//enable AD0[7] on pin 0.2
		Pinsel::setFunc(0, 2, 2);


	}

	void init() {
		ADC::enableChannel(DISCHARGE_CURRENT_CH);
		temperature.init();
	}

	void handleTick() override {

		static GlitchFilter currentFilter;

		static PeriodicTimer<> t(1);

		static PeriodicTimer<> chkVoltage(500);

		if (t.isExpired()) {
			int16_t ival = ADC::getData(DISCHARGE_CURRENT_CH);

			ival = currentFilter.push(ival);
			if (ival < 0)
				ival = 0;

			const float shuntResistance = 0.05;
			float i = (3.3 / 4096) * ival / shuntResistance
					* (1 + (calibrationCoefficient / 1000));

			current.push(i);

		}
		if(chkVoltage.isExpired()) {
			if(battery.getVoltage() < cutoffVoltage) {
				complete = true;
				enable(false);
			}
		}

	}

	float getCurrent() {
		return current.getValue();
	}

	void setCutoff(float voltage) {
		cutoffVoltage = voltage;
	}

	void enable(bool en) {
		dischargerEn::set(en);
		fanEn::set(en);
		DAC::setValue(0);
		enabled = en;
		if(en)
			complete = false;
	}

	bool isEnabled() {
		return enabled;
	}

	bool isDischargeComplete() {
		return complete;
	}

	void setOutput(float currentA) {
		//0.5V = 10A
			//1V = 20A
		const float AperStep = 3.3 / 1024 / 0.05;
		uint16_t output = (int)((currentA) / AperStep);

		DAC::setValue(output);
		XPCC_LOG_DEBUG .printf("output %d\n", output);

	}

	float getTemperature() {
		return temperature.readTemperature();
	}

	uint8_t dischargeCurrent = 40;

private:
	SMT160<tempPwm> temperature;
	Average<float, 3000> current;

	bool complete;
	bool enabled;

	float cutoffVoltage = 2.4;
	float calibrationCoefficient = 0;


};



#endif /* DISCHARGER_HPP_ */
