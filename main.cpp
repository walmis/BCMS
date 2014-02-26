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
#include <xpcc/processing.hpp>
#include <xpcc/debug.hpp>

#include <xpcc/driver/connectivity/usb/USBDevice.hpp>
#include <xpcc/driver/display.hpp>

#include <xpcc/ui/button_group.hpp>
#include <xpcc/math.hpp>
#include "Filters.h"

#include <malloc.h>
#include <math.h>

#include "terminal.hpp"
#include "discharger.hpp"
#include "battery.hpp"
#include "charger.hpp"
#include "smt160.hpp"
#include "CycleManager.hpp"

#include "sd/SDIO.hpp"
#include "sd/USBMSD_SDHandler.hpp"

using namespace xpcc;
using namespace xpcc::lpc17;

const char fwversion[16] __attribute__((used, section(".fwversion"))) = "v0.1";

#include "pindefs.hpp"


//Hd44780<lcd_data, lcd_rw, lcd_rs, lcd_e> display(16, 2);

Hd44780<lcd_e, lcd_rw, lcd_rs, lcd_data> display(16, 2);

Battery battery;
Discharger discharger;
Charger charger;

CycleManager manager;

//xpcc::USBSerial device(0xffff);

SDIOAsync<lpc17::SpiMaster0, sdCs> sdCard;

USBCDCMSD<USBMSD_SDHandler<typeof(sdCard)>> device(0xffff, 0x0001, 0);

fat::FileSystem fs(&sdCard, 0);

xpcc::log::Logger xpcc::log::debug(device);
xpcc::log::Logger xpcc::log::error(device);

xpcc::IOStream serial(device);



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


//UARTDevice uart(115200);

//xpcc::log::Logger xpcc::log::debug(uart);

extern "C" void BusFault_Handler(void) {
	  asm volatile("MRS r0, MSP;"
			       "B Hard_Fault_Handler");
}

extern "C" void UsageFault_Handler(void) {
	  asm volatile("MRS r0, MSP;"
			       "B Hard_Fault_Handler");
}

enum { r0, r1, r2, r3, r12, lr, pc, psr};

extern "C" void HardFault_Handler(void)
{
  asm volatile("MRS r0, MSP;"
		       "B Hard_Fault_Handler");
}

extern "C"
void Hard_Fault_Handler(uint32_t stack[]) {

	//register uint32_t* stack = (uint32_t*)__get_MSP();

	//XPCC_LOG_DEBUG .printf("Hard Fault\n");

//	XPCC_LOG_DEBUG .printf("r0  = 0x%08x\n", stack[r0]);
//	XPCC_LOG_DEBUG .printf("r1  = 0x%08x\n", stack[r1]);
//	XPCC_LOG_DEBUG .printf("r2  = 0x%08x\n", stack[r2]);
//	XPCC_LOG_DEBUG .printf("r3  = 0x%08x\n", stack[r3]);
//	XPCC_LOG_DEBUG .printf("r12 = 0x%08x\n", stack[r12]);
//	XPCC_LOG_DEBUG .printf("lr  = 0x%08x\n", stack[lr]);
//	XPCC_LOG_DEBUG .printf("pc  = 0x%08x\n", stack[pc]);
//	XPCC_LOG_DEBUG .printf("psr = 0x%08x\n", stack[psr]);

	display.clear();
	display.setCursor(0, 0);
	display.printf("Fault lr:%x\n", stack[lr]);
	display.printf("pc=0x%08x", stack[pc]);

	while(1);
//	while(1) {
//		if(!progPin::read()) {
//			for(int i = 0; i < 10000; i++) {}
//			NVIC_SystemReset();
//		}
//	}
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

		if(buttons.isPressed(BUTTON_UP)) {
			manager.startCycle(CycleManager::CycleType::CHARGE);
		}
		if(buttons.isPressed(BUTTON_DOWN)) {
			manager.startCycle(CycleManager::CycleType::DISCHARGE);
		}

		if(buttons.isPressedLong(BUTTON_LEFT)) {
			NVIC_DeInit();

			display.clear();
			display.setCursor(0, 0);
			display.printf("Bootloader\n");

			usbConnPin::setOutput(false);
			delay_ms(100);
			LPC_WDT->WDFEED = 0x56;
		}

		if(buttons.isPressed(BUTTON_RIGHT)) {
			manager.stopCycle();
		}

	}
};

#define NOM(x) ((int)x)
#define DENOM(x, y) ( (int)((x - (int)x) * y) )

void sysTick() {

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

void loadConfiguration() {
	XPCC_LOG_DEBUG .printf("Loading configuration\n");
	fat::File f;
	uint8_t buf[100];
	if(f.open("/config.ini", "r") == FR_OK) {
		XPCC_LOG_DEBUG .printf("File opened\n");
		XPCC_LOG_DEBUG .printf("readLine %d\n", f.readLine(buf, 100));


	}

}

#define CMD(i, name) (strcmp(argv[i], name) == 0)

class CmdTerminal : public Terminal {
public:
	CmdTerminal(IODevice& dev) : Terminal(dev) {}

protected:
	void handleCommand(uint8_t nargs, char* argv[]) {
		//XPCC_LOG_DEBUG .printf("nargs %d %s\n", nargs, argv[0]);

		if(CMD(0, "charge")) {
			manager.startCycle(CycleManager::CycleType::CHARGE);
		} else
		if(CMD(0, "discharge")) {
			manager.startCycle(CycleManager::CycleType::DISCHARGE);
		} else
		if(CMD(0, "stop")) {
			manager.stopCycle();
		} else
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
		if(CMD(0, "load")) {
			loadConfiguration();
		} else
		if(CMD(0, "ls")) {
			fat::FileInfo info;
			fat::Directory dir;
			dir.open("/");

			while(dir.readDir(info) == FR_OK) {
				if(info.eod()) {
					break;
				} else {
					XPCC_LOG_DEBUG .printf("file: %s\n", info.getName());
				}
			}

			dir.close();
		}
	}
};


CmdTerminal terminal(device);
ButtonTask btnTask;




void idleTask() {

	static PeriodicTimer<> t(250);

	LPC_WDT->WDFEED = 0xAA;
	LPC_WDT->WDFEED = 0x55;

	static uint8_t count = 0;

	if(t.isExpired()) {
		display.setCursor(14, 0);

		if(!sdCard.isInitialised()) {
			display << "  ";

			if(sdCard.initialise()) {
				fs.mount();

			}

		} else {
			display << "SD";
		}
	}
}


int main() {

	lpc17::SysTickTimer::enable();
	lpc17::SysTickTimer::attachInterrupt(sysTick);

	display.initialize();
	display.clear();
	display.printf("BCMS %s", fwversion);

	xpcc::Random::seed();

	device.msd.assignCard(sdCard);

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

	ADC::init(20000);
	ADC::burstMode(true);
	ADC::start(ADC::ADCStartMode::START_CONTINUOUS);

	battery.init();
	discharger.init();
	charger.init();

	//display.clear();
	TickerTask::tasksRun(idleTask);
}
