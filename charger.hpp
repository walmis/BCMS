/*
 * charger.hpp
 *
 *  Created on: Oct 22, 2013
 *      Author: walmis
 */

#ifndef CHARGER_HPP_
#define CHARGER_HPP_

#include <xpcc/math/filter.hpp>
#include <xpcc/architecture.hpp>
#include "pindefs.hpp"
#include "Filters.h"

#include "mcp415x.hpp"

using namespace xpcc;
using namespace xpcc::lpc17;


class Charger : TickerTask {
public:

	Charger() : pid(0.07, 0.4, 0.0, 2000.0, 148.0) {
		enabled = false;
		currentSetpoint = 0;
		constantVoltage = false;
		cutoffVoltage = 3.60;
	}
	void init() {
		psuEn::setOutput(false);
		fanEn::setOutput(false);
		voltagePot.init();
	}

	void handleTick() override {
		static PeriodicTimer<> t(100);

		if(t.isExpired()) {

			if (enabled) {

				if(battery.getVoltage() >= cutoffVoltage) {
					constantVoltage = true;
				}

				if (!constantVoltage) {
					float relativeError = 1
							- battery.getCurrent() / currentSetpoint;

					if (relativeError < 0 || relativeError > 0.1) {

						pid.update(currentSetpoint - battery.getCurrent(),
								false);

						XPCC_LOG_DEBUG.printf("pid value %.5f\n",
								pid.getValue());

						setValue(100 + (int) roundf(pid.getValue()));
					}
				}
			}

			//XPCC_LOG_DEBUG .printf("set %.3f cyr %.3f\n", currentSetpoint, battery.getCurrent());
			//
			//XPCC_LOG_DEBUG. printf("last err: %.5f\n", pid.getLastError());
			//XPCC_LOG_DEBUG. printf("last errsum: %.5f\n", pid.getErrorSum());

			//

		}
	}

	void enable(bool en = true) {
		pid.reset();
		if(en) {
			psuEn::set();
			fanEn::set();
		} else {
			psuEn::reset();
			fanEn::reset();
		}
		enabled = en;
		constantVoltage = false;

	}

	void setCurrent(float current) {
		currentSetpoint = current;
		//pid.reset();
	}

	void setValue(uint16_t value) {
		voltagePot.setValue(value);
	}


//private:
	bool enabled;

	MCP4151<lpc17::SpiMaster1, pot_cs> voltagePot;
	float cutoffVoltage;

	float currentSetpoint;

	Pid<float, 1> pid;

	bool constantVoltage;

};


#endif /* CHARGER_HPP_ */
