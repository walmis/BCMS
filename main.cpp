/*
 * main.cpp
 *
 *  Created on: Feb 28, 2013
 *      Author: walmis
 */
#include <lpc17xx_nvic.h>
#include <lpc17xx_uart.h>
#include <lpc17xx_pinsel.h>

#include <xpcc/architecture.hpp>
#include <xpcc/workflow.hpp>
#include <xpcc/debug.hpp>

#include <xpcc/driver/connectivity/usb/USBDevice.hpp>
#include <xpcc/driver/ui/display.hpp>

#include <xpcc/driver/ui/button_group.hpp>
#include <xpcc/math.hpp>
#include "Filters.h"

#include <malloc.h>

#include "discharger.hpp"
#include "battery.hpp"
#include "charger.hpp"
#include "smt160.hpp"
#include "sd/SDCard.h"


using namespace xpcc;
using namespace xpcc::lpc17;

const char fwversion[16] __attribute__((used, section(".fwversion"))) = "v0.1";

#include "pindefs.hpp"


Hd44780<lcd_e, lcd_rw, lcd_rs, lcd_data> display(16, 2);

Battery battery;
Discharger discharger;
Charger charger;

SDCard<lpc17::SpiMaster0, sdCs> sdCard;

class UARTDevice : public IODevice {

public:
	UARTDevice(int baud) {

		UART_CFG_Type cfg;
		UART_FIFO_CFG_Type fifo_cfg;

		UART_ConfigStructInit(&cfg);
		cfg.Baud_rate = baud;

		UART_Init(LPC_UART0, &cfg);

		PINSEL_CFG_Type PinCfg;

		PinCfg.Funcnum = 1;
		PinCfg.OpenDrain = 0;
		PinCfg.Pinmode = 0;
		PinCfg.Pinnum = 2;
		PinCfg.Portnum = 0;
		PINSEL_ConfigPin(&PinCfg);
		PinCfg.Pinnum = 3;
		PINSEL_ConfigPin(&PinCfg);

		UART_Init(LPC_UART0, &cfg);

		UART_FIFOConfigStructInit(&fifo_cfg);

		UART_FIFOConfig(LPC_UART0, &fifo_cfg);

		UART_TxCmd(LPC_UART0, ENABLE);

	}

	void
	write(char c) {
		while (!(LPC_UART0->LSR & UART_LSR_THRE)) {
		}

		UART_SendByte(LPC_UART0, c);
	};

	void
	flush(){};

	/// Read a single character
	bool read(char& c) {
		return false;
	}
};


void boot_jump( uint32_t address ){
   __asm("LDR SP, [R0]\n"
   "LDR PC, [R0, #4]");
}


xpcc::USBSerial device(0xffff);

xpcc::IOStream serial(device);

//UARTDevice uart(115200);
xpcc::log::Logger xpcc::log::debug(device);
//xpcc::log::Logger xpcc::log::debug(uart);


enum { r0, r1, r2, r3, r12, lr, pc, psr};

extern "C" void HardFault_Handler(void)
{
  asm volatile("MRS r0, MSP;"
		       "B Hard_Fault_Handler");
}

extern "C"
void Hard_Fault_Handler(uint32_t stack[]) {

	//register uint32_t* stack = (uint32_t*)__get_MSP();

	XPCC_LOG_DEBUG .printf("Hard Fault\n");

	XPCC_LOG_DEBUG .printf("r0  = 0x%08x\n", stack[r0]);
	XPCC_LOG_DEBUG .printf("r1  = 0x%08x\n", stack[r1]);
	XPCC_LOG_DEBUG .printf("r2  = 0x%08x\n", stack[r2]);
	XPCC_LOG_DEBUG .printf("r3  = 0x%08x\n", stack[r3]);
	XPCC_LOG_DEBUG .printf("r12 = 0x%08x\n", stack[r12]);
	XPCC_LOG_DEBUG .printf("lr  = 0x%08x\n", stack[lr]);
	XPCC_LOG_DEBUG .printf("pc  = 0x%08x\n", stack[pc]);
	XPCC_LOG_DEBUG .printf("psr = 0x%08x\n", stack[psr]);

	display.clear();
	display.setCursor(0, 0);
	display.printf("Fault\n");
	display.printf("pc=0x%08x", stack[pc]);

	while(1) {
		if(!progPin::read()) {
			for(int i = 0; i < 10000; i++) {}
			NVIC_SystemReset();
		}
	}
}



class ButtonTask : TickerTask {
public:
	ButtonTask() : buttons(0xFF) {

	};

	enum {
		BUTTON_LEFT = 1<<2,
		BUTTON_RIGHT = 1<<3,
		BUTTON_UP = 1<<1,
		BUTTON_DOWN = 1<<0
	};

	ButtonGroup<> buttons;

protected:
	void handleTick() override {
		static PeriodicTimer<> t(10);
		if(t.isExpired()) {
			buttons.update(buttonsNibble::read());
		}

		if(buttons.isPressed(BUTTON_LEFT)) {
			//XPCC_LOG_DEBUG .printf("dec %d\n", voltagePot.getValue());
		}
		if(buttons.isPressed(BUTTON_RIGHT)) {
			//XPCC_LOG_DEBUG .printf("inc %d\n", voltagePot.getValue());
		}

		if(buttons.isPressed(BUTTON_UP)) {
			static bool en = false;
			//psuEn::set(en);
			//en = !en;
			//XPCC_LOG_DEBUG .printf("%d psu toggle\n", _psuEn::read());
		}

		if(buttons.isPressed(BUTTON_DOWN)) {
			//dischargerEn::toggle();
			//fanEn::toggle();
			discharger.enable(true);
			discharger.setOutput(2); //5A
		}

	}


};

#define NOM(x) ((int)x)
#define DENOM(x, y) ( (int)((x - (int)x) * y) )




void sysTick() {
	LPC_WDT->WDFEED = 0xAA;
	LPC_WDT->WDFEED = 0x55;


	if(btn_left::read() == 0 && btn_up::read() == 0) {
		//NVIC_SystemReset();

		NVIC_DeInit();

		display.clear();
		display.setCursor(0, 0);
		display.printf("Bootloader\n");

		usbConnPin::setOutput(false);
		delay_ms(100);

		LPC_WDT->WDFEED = 0x56;

		//boot_jump(0);
	}
}

#define CMD(i, name) (strcmp(argv[i], name) == 0)

class Terminal : xpcc::TickerTask {
	char buffer[32];
	uint8_t pos = 0;
	void handleTick() override {

		if(device.read(buffer[pos])) {
			if(buffer[pos] == '\n') {
				//remove the newline character
				buffer[pos] = 0;
				parse();
				pos = 0;
				return;
			}
			pos++;
			pos &= 31;
		}
	}


	void handleCommand(uint8_t nargs, char* argv[]) {
		//XPCC_LOG_DEBUG .printf("nargs %d %s\n", nargs, argv[0]);

		if(CMD(0, "charger") && CMD(1, "set")) {
			uint16_t value = to_int(argv[2]);
			if(value < 255) {
				serial.printf("SET %d\n", value);
				charger.setCurrent(value);
			}
		} else

		if(CMD(0, "charger") && CMD(1, "v")) {
			uint16_t value = to_int(argv[2]);
			if(value < 255) {
				serial.printf("SETV %d\n", value);
				charger.setValue(value);
			}
		} else

		if(CMD(0, "discharger") && CMD(1, "enable")) {
			discharger.enable(true);
		} else

		if(CMD(0, "discharger") && CMD(1, "disable")) {
			discharger.enable(false);
		} else

		if(CMD(0, "discharger") && CMD(1, "set")) {
			int c = to_int(argv[2]);
			serial.printf("Set output %dA\n", c);
			discharger.setOutput(c);
		} else

		if(CMD(0, "charger") && CMD(1, "get")) {
			serial.printf("charger value %d\n", charger.voltagePot.getValue());
		} else
		if(CMD(0, "charger") && CMD(1, "enable")) {
			charger.enable();
		} else
		if(CMD(0, "charger") && CMD(1, "disable")) {
			charger.enable(false);
		} else

		if(CMD(0, "battery") && CMD(1, "icalib")) {
			if(nargs == 3) {
				serial.printf("Calibration set\n");
				battery.currentCalibCoef = to_int(argv[2]);


			} else {
				serial.printf("icalib %d\n", (int)battery.currentCalibCoef);
			}
		} else
		if(CMD(0, "battery") && CMD(1, "vcalib")) {
			serial.printf("Calibration set\n");
			battery.voltageCalibCoef = to_int(argv[2]);
		} else
		if(CMD(0, "battery") && CMD(1, "staticres")) {
			serial.printf("Static resistance :");

			if(nargs == 3) {
				//float a = atoff(argv[2]);

				//float a = roundf(battery.staticResistance);
				//serial << a;
				//serial << a << endl;
			} else {

				XPCC_LOG_DEBUG .printf("res %.5f\n", battery.staticResistance);
				//serial << battery.staticResistance << endl;
				//serial << sizeof(double) << endl;
				//serial << powf(battery.staticResistance, 2);
			}
		} else
			if(CMD(0, "sdtest")) {
				sdCard.disk_initialize();
			}
			else if(CMD(0, "sdread")) {
				uint8_t buffer[512];

				int status = sdCard.disk_read(buffer, 0);
				if(status) {
					XPCC_LOG_DEBUG .printf("read failed\n");
				}else{
					XPCC_LOG_DEBUG .dump_buffer(buffer, 512);
				}
			}

	}


private:
	void parse() {
		XPCC_LOG_DEBUG .printf(": %s\n", buffer);

		char* tok = strtok(buffer, " ");

		char* arglist[6];
		uint8_t nargs = 0;

		memset(arglist, 0, sizeof(char*)*6);

		while(tok && nargs < 5) {
			arglist[nargs] = tok;
			tok = strtok(0, " ");
			nargs++;
		}

		handleCommand(nargs, arglist);

	}

	int to_int(char *p) {
		int k = 0;
		bool neg = false;
		if(*p == '-') {
			p++;
			neg = true;
		}

		while (*p) {
			k = (k << 3) + (k << 1) + (*p) - '0';
			p++;
		}
		if(neg) return -k;
		return k;
	}

	bool float_scan(const wchar_t* wcs, float* val)
	// (C)2009 Marcin Sokalski gumix@ghnet.pl - All rights reserved.
			{
		int hdr = 0;
		while (wcs[hdr] == L' ')
			hdr++;

		int cur = hdr;

		bool negative = false;
		bool has_sign = false;

		if (wcs[cur] == L'+' || wcs[cur] == L'-') {
			if (wcs[cur] == L'-')
				negative = true;
			has_sign = true;
			cur++;
		} else
			has_sign = false;

		int quot_digs = 0;
		int frac_digs = 0;

		bool full = false;

		wchar_t period = 0;
		int binexp = 0;
		int decexp = 0;
		unsigned long value = 0;

		while (wcs[cur] >= L'0' && wcs[cur] <= L'9') {
			if (!full) {
				if (value >= 0x19999999 && wcs[cur] - L'0' > 5
						|| value > 0x19999999) {
					full = true;
					decexp++;
				} else
					value = value * 10 + wcs[cur] - L'0';
			} else
				decexp++;

			quot_digs++;
			cur++;
		}

		if (wcs[cur] == L'.' || wcs[cur] == L',') {
			period = wcs[cur];
			cur++;

			while (wcs[cur] >= L'0' && wcs[cur] <= L'9') {
				if (!full) {
					if (value >= 0x19999999 && wcs[cur] - L'0' > 5
							|| value > 0x19999999)
						full = true;
					else {
						decexp--;
						value = value * 10 + wcs[cur] - L'0';
					}
				}

				frac_digs++;
				cur++;
			}
		}

		if (!quot_digs && !frac_digs)
			return false;

		wchar_t exp_char = 0;

		int decexp2 = 0; // explicit exponent
		bool exp_negative = false;
		bool has_expsign = false;
		int exp_digs = 0;

		// even if value is 0, we still need to eat exponent chars
		if (wcs[cur] == L'e' || wcs[cur] == L'E') {
			exp_char = wcs[cur];
			cur++;

			if (wcs[cur] == L'+' || wcs[cur] == L'-') {
				has_expsign = true;
				if (wcs[cur] == '-')
					exp_negative = true;
				cur++;
			}

			while (wcs[cur] >= L'0' && wcs[cur] <= L'9') {
				if (decexp2 >= 0x19999999)
					return false;
				decexp2 = 10 * decexp2 + wcs[cur] - L'0';
				exp_digs++;
				cur++;
			}

			if (exp_negative)
				decexp -= decexp2;
			else
				decexp += decexp2;
		}

		// end of wcs scan, cur contains value's tail

		if (value) {
			while (value <= 0x19999999) {
				decexp--;
				value = value * 10;
			}

			if (decexp) {
				// ensure 1bit space for mul by something lower than 2.0
				if (value & 0x80000000) {
					value >>= 1;
					binexp++;
				}

				if (decexp > 308 || decexp < -307)
					return false;

				// convert exp from 10 to 2 (using FPU)
				int E;
				double v = pow(10.0, decexp);
				double m = frexp(v, &E);
				m = 2.0 * m;
				E--;
				value = (unsigned long) floor(value * m);

				binexp += E;
			}

			binexp += 23; // rebase exponent to 23bits of mantisa

			// so the value is: +/- VALUE * pow(2,BINEXP);
			// (normalize manthisa to 24bits, update exponent)
			while (value & 0xFE000000) {
				value >>= 1;
				binexp++;
			}
			if (value & 0x01000000) {
				if (value & 1)
					value++;
				value >>= 1;
				binexp++;
				if (value & 0x01000000) {
					value >>= 1;
					binexp++;
				}
			}

			while (!(value & 0x00800000)) {
				value <<= 1;
				binexp--;
			}

			if (binexp < -127) {
				// underflow
				value = 0;
				binexp = -127;
			} else if (binexp > 128)
				return false;

			//exclude "implicit 1"
			value &= 0x007FFFFF;

			// encode exponent
			unsigned long exponent = (binexp + 127) << 23;
			value |= exponent;
		}

		// encode sign
		unsigned long sign = negative << 31;
		value |= sign;

		if (val) {
			*(unsigned long*) val = value;
		}

		return true;
	}
};

Terminal terminal;


ButtonTask btnTask;
//DataMeasurement dataTask;



void idleTask() {

	static PeriodicTimer<> t(250);

	if(t.isExpired()) {

		display.setCursor(0, 0);
		display.printf("%.3fV %.2fA ", battery.getVoltage(), battery.getCurrent());


		display.setCursor(0, 1);
		display.printf("%.1fA %.1fC", discharger.getCurrent(),
				discharger.getTemperature());

	}
}



int main() {
	NVIC_SetVTOR(0x1000);

	lpc17::SysTickTimer::enable();
	lpc17::SysTickTimer::attachInterrupt(sysTick);

	xpcc::Random::seed();


	SpiMaster0::initialize(SpiMaster0::Mode::MODE_0, 1000000);
	Pinsel::setFunc(0, 15, 2); //SCK0
	Pinsel::setFunc(0, 17, 2); //MISO0
	Pinsel::setFunc(0, 18, 2); //MOSI0

	SpiMaster1::initialize(SpiMaster1::Mode::MODE_0, 10000000);
	Pinsel::setFunc(0, 7, 2); //set SCK1
	Pinsel::setFunc(0, 9, 2); //set MOSI1

	fanEn::setOutput(0);

	xpcc::PeriodicTimer<> t(500);

	usbConnPin::setOutput(true);
	device.connect();

	NVIC_SetPriority(USB_IRQn, 16);

	display.initialize();
	display.clear();
	display.printf("BCMS %s", fwversion);

	ADC::init(20000);
	ADC::burstMode(true);
	ADC::start(ADC::ADCStartMode::START_CONTINUOUS);

	battery.init();
	discharger.init();
	charger.init();

	GpioInterrupt::enableGlobalInterrupts();

	//sdCard.disk_initialize();

	TickerTask::tasksRun(idleTask);
}
