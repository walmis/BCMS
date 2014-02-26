/*
 * battery.hpp
 *
 *  Created on: Oct 22, 2013
 *      Author: walmis
 */

#ifndef BATTERY_HPP_
#define BATTERY_HPP_

#include <xpcc/architecture.hpp>
#include "pindefs.hpp"
#include "Filters.h"

using namespace xpcc;
using namespace xpcc::lpc17;

#define DISCHARGE_CURRENT_CH 7

class Battery : TickerTask {

public:

public:
	enum {
		CHRG_CURRENT_CH = 2,
		VOLTAGE_CH = 6,
	};

	const uint16_t zero_current_val = 48;

	float staticResistance = 0.01184;

	const float coef = 2.890 / 1770;
	const float icoef = 30.6 / 1842;

	float voltageCalibCoef = 4;
	float currentCalibCoef = 20;

	Battery() {


	}

	GlitchFilter medianFiltcurrent;
	GlitchFilter medianFiltvoltage;

	IIRFilter<float, 120> voltageFiltered;
	IIRFilter<float, 25> currentFiltered;
	//Average<float, 500> currentFiltered;
	//Average<float, 3000> voltageFiltered;

	void init() {
		Pinsel::setFunc(0, 25, 1);
		Pinsel::setFunc(0, 3, 2);

		ADC::enableChannel(2);
		ADC::enableChannel(6);
	}

	void handleTick() override {
		static PeriodicTimer<> t(1);

		if(t.isExpired()) {
			int16_t iraw = ADC::getData(CHRG_CURRENT_CH);
			int16_t vraw = ADC::getData(VOLTAGE_CH);

			int16_t ival = medianFiltcurrent.push(iraw);
			int16_t vval = medianFiltvoltage.push(vraw);

			//XPCC_LOG_DEBUG .printf("raw current: %04d\n", ival);

			ival -= zero_current_val;
			if(ival < 0) ival = 0;

			currentFiltered.push(ival);
			voltageFiltered.push(vval);

		}

	}

	float getCurrent() {
		return currentFiltered.getValue() * icoef * (1 + currentCalibCoef / 1000);
	}

	float getVoltage() {
		return (voltageFiltered.getValue() * coef * (1 + voltageCalibCoef / 1000))
				- staticResistance * getCurrent();
	}

};

extern Battery battery;

#endif /* BATTERY_HPP_ */
