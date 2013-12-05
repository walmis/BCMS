/*
 * CycleManager.hpp
 *
 *  Created on: Nov 11, 2013
 *      Author: walmis
 */

#ifndef CYCLEMANAGER_HPP_
#define CYCLEMANAGER_HPP_

#include <xpcc/architecture.hpp>
#include <xpcc/driver/storage/fat.hpp>

#include "charger.hpp"
#include "battery.hpp"
#include "discharger.hpp"

#include <string>

using namespace xpcc;

extern Charger charger;
extern Discharger discharger;
extern Battery battery;
extern Hd44780<lcd_e, lcd_rw, lcd_rs, lcd_data> display;

static const uint16_t updateTimeout = 5000;

class StringStream : public IOStream, IODevice {
public:
	StringStream(char* buffer, int len) :
		IOStream(*static_cast<IODevice*>(this))
	{
		length = len;
		buf = buffer;
		clear();
	}

	void clear() {
		memset(buf, 0, length);
		num = 0;
	}

	char* str() {
		return buf;
	}

private:
	void write(char c) override {
		if(num >= length)
			return;

		buf[num++] = c;
	}
	bool read(char& c) override {
		return false;
	}

	void flush() override {}

	uint16_t num;

	char* buf;
	int length;
};

class CycleManager : TickerTask {
public:
	enum CycleType {
		CHARGE,
		DISCHARGE,
		DISABLED
	};

	CycleManager() : data_update_timeout(updateTimeout) {
		cycle = DISABLED;
		active = false;
	}


	void start() {




	}

	void stop() {
		if(active) {

			active = false;

			discharger.enable(false);
			charger.enable(false);

			file.close();
		}
	}

	void startCycle(CycleType type) {

		fat::FileInfo finfo;
		if(!finfo.stat("/battery_data")) {
			fat::FileSystem::mkdir("/battery_data");
		}

		char buf[64];
		StringStream name(buf, 64);

		int last_cycle;

		if(type == CHARGE) {
			last_cycle = getLastCycle(CHARGE)+1;
			name.printf("/battery_data/charge_%d.csv", last_cycle);
		} else {
			last_cycle = getLastCycle(DISCHARGE)+1;
			name.printf("/battery_data/discharge_%d.csv", last_cycle);
		}

		if(file.open(name.str(), "w") == FR_OK) {
			XPCC_LOG_DEBUG .printf("file opened successfully\n");

			file.printf("Starting %s cycle %d\n", (type==CHARGE)?"charge":"discharge", last_cycle);
			fat::FileInfo finfo;
			if(!finfo.stat("/battery_data")) {
				fat::FileSystem::mkdir("/battery_data");
			}
			if(type == CHARGE) {
				file << "Time (s), Voltage (V), Current(A)\r\n";
			} else
			if(type == DISCHARGE) {
				file << "Time (s), Voltage (V), Current(A), Temperature (C)\r\n";
			}
		}

		if(type == CHARGE) {
			charger.setCurrent(15);

			discharger.enable(false);

			charger.enable(true);
		} else
		if(type == DISCHARGE) {
			charger.enable(false);

			discharger.enable(true);
			discharger.setOutput(40.0);
		}


		cycleNumber = last_cycle;
		cycle = type;
		active = true;
		cycle_start_time = Clock::now();
		data_update_timeout.restart(updateTimeout);

	}

	CycleType currentCycle() {
		return cycle;
	}

	bool isActive() {
		return active;
	}


private:

	void handleTick() override {

		if(active && data_update_timeout.isExpired()) {
			if(file.isOpened()) {
				XPCC_LOG_DEBUG .printf("write\n");

				if(cycle == CHARGE) {
					file.printf("%d, %.3f, %.2f\r\n",
							(Clock::now() - cycle_start_time).getTime() / 1000,
							battery.getVoltage(), battery.getCurrent());

				} else
				if(cycle == DISCHARGE) {
					file.printf("%d, %.3f, %.2f, %.1f\r\n",
							(Clock::now() - cycle_start_time).getTime() / 1000,
							battery.getVoltage(), discharger.getCurrent(),
							discharger.getTemperature());
				}
			}
		}

		static PeriodicTimer<> displayRefresh(500);
		if(displayRefresh.isExpired()) {

			display.setCursor(0,0);

			if(battery.getVoltage() < 0.2) {
				display.printf("Connect    \nBattery        ");
			} else

			if(!active){
				display.printf("Idle  %.1fC  \nVoltage %.3fV ",
						discharger.getTemperature(),
						battery.getVoltage());
			} else {

				if(cycle == CHARGE) {
					display.printf("Charge #%03d\n", cycleNumber);
					display.printf("%.3fV %.1fA  ", battery.getVoltage(),
							battery.getCurrent());


				} else
				if(cycle == DISCHARGE) {

					display.printf("Dischg #%03d\n", cycleNumber);
					display.printf("%.3fV %.1fA %dC", battery.getVoltage(),
							discharger.getCurrent(),
							(int)roundf(discharger.getTemperature()));

				}


			}



			//		display.setCursor(0, 0);
			//		display.printf("%.3fV %.2fA ", battery.getVoltage(), battery.getCurrent());
			//
			//		display.setCursor(0, 1);
			//		display.printf("%.1fA %.1fC", discharger.getCurrent(),
			//				discharger.getTemperature());




		}


	}


	fat::File file;

	Timestamp cycle_start_time;
	PeriodicTimer<> data_update_timeout;
	CycleType cycle;
	bool active;
	int cycleNumber;

	int getLastCycle(CycleType type) {
		fat::Directory dir;
		fat::FileInfo f;

		int currentCycle = -1;
		int number = -1;

		if(dir.open("/battery_data") == FR_OK){
			while(dir.readDir(f) == FR_OK && !f.eod()) {
				int n;
				if(type == CHARGE) {
					n = sscanf(f.getName(), "charge_%d.csv", &number);
				} else {
					n = sscanf(f.getName(), "discharge_%d.csv", &number);
				}

				if(n) {
					if(number > currentCycle)
						currentCycle = number;
				}
			}
		}

		return currentCycle;
	}

};


#endif /* CYCLEMANAGER_HPP_ */
