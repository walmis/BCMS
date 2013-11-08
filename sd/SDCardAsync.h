/*
 * SDCardAsync.h
 *
 *  Created on: Oct 29, 2013
 *      Author: walmis
 */

#ifndef SDCARDASYNC_H_
#define SDCARDASYNC_H_

#include "SDCard.h"
#include <xpcc/processing.hpp>
#include <xpcc/debug.hpp>


class Operation {

public:
	Operation();
	virtual ~Operation();

	virtual void handleTick() = 0;

	void reset() {
		status = -1;
		isComplete = false;
	}

	void complete(int status) {
		if(!isComplete) {
			this->status = status;
			isComplete = true;
			onComplete();
		}
	}

	void tick() {
		if(!isComplete) {
			handleTick();
		}
	}

	xpcc::FunctionPointer onComplete;

	int status;
	bool isComplete;


	Operation* next = 0;
	static Operation* base;
};

template <typename Spi, typename Cs>
class SD_CMD : public Operation {
public:
	SD_CMD(int cmd, int arg, int timeout = SD_COMMAND_TIMEOUT) :
			cmd(cmd), arg(arg), t(timeout) {

		begin(cmd, arg, timeout);
	}

	SD_CMD() {
		cmd = -1;
		arg = 0;
	};

	void reset() {
		cmd = -1;
		arg = 0;
		Operation::reset();
	}

	void begin(int cmd, int arg, int timeout = SD_COMMAND_TIMEOUT) {
		this->cmd = cmd;
		this->arg = arg;
		this->t.restart(timeout);
		status = -1;
		isComplete = false;

		Cs::reset();
		// send a command
		Spi::write(uint8_t(0x40 | cmd));
		Spi::write(uint8_t(arg >> 24));
		Spi::write(uint8_t(arg >> 16));
		Spi::write(uint8_t(arg >> 8));
		Spi::write(uint8_t(arg >> 0));
		Spi::write(0x95);
		XPCC_LOG_DEBUG.printf("_cmdAsync(%d, %x) = ", cmd, arg);
		// wait for the repsonse (response[7] == 0)
	}

	void handleTick() {
		if(isComplete || cmd < 0)
			return;

		if(!t.isExpired()) {
			int response = Spi::write(0xFF);
			if (!(response & 0x80)) {
				Cs::set();
				Spi::write(0xFF);
				XPCC_LOG_DEBUG.printf("%d\n", response);
				complete(response);
			}
		} else {
			XPCC_LOG_DEBUG.printf("cmd %d timeout\n", cmd);
			Cs::set();
			Spi::write(0xFF);
			complete(-1);
		}
	}

	int cmd;
	int arg;
	xpcc::Timeout<> t;

};

template <typename Spi, typename Cs>
class SD_READ_DATA : public Operation {
public:
	SD_READ_DATA(uint8_t* buffer, size_t length) :
		buffer(buffer), length(length){
		//t.start();
		//XPCC_LOG_DEBUG .printf("OP:start read\n");
		waitStartByte = true;
		//waitStartByte = false;
		started = false;

		Cs::reset();
	}

protected:

	void handleTick() {

		if(waitStartByte) {
			if(Spi::write(0xFF) == 0xFE) {
				waitStartByte = false;
			}
		} else {
			if(!started) {
				//XPCC_LOG_DEBUG .printf("OP:start spi transfer\n");
				started = true;
				Spi::transfer(0, buffer, length);

			} else
			if(Spi::isFinished()) {
				//finish read
				//XPCC_LOG_DEBUG .printf("OP:finish transfer\n");
				Spi::write(0xFF); // checksum
				Spi::write(0xFF);
//
				Cs::set();
				Spi::write(0xFF);

				complete(1);
			}
		}
	}

	//xpcc::ProfileTimer t;
	uint8_t* buffer;
	size_t length;

	bool waitStartByte;
	bool started;

};

template <typename Spi, typename Cs>
class SD_WRITE_DATA : public Operation {
public:
	SD_WRITE_DATA(uint8_t* src, uint8_t token) {
		begin(src, token);
	}

	SD_WRITE_DATA(){
		len = 0;
	}

	void handleTick() {
		if(len) {
			Spi::write(*src++);
			len--;
			if(!len) {

#if USE_SD_CRC
				uint16_t crc = CRC_CCITT(src, 512);
#else  // USE_SD_CRC
				uint16_t crc = 0XFFFF;
#endif  // USE_SD_CRC

				Spi::write(crc >> 8);
				Spi::write(crc & 0XFF);

				uint8_t status_ = Spi::write(0xFF);
				XPCC_LOG_DEBUG.printf("write status %x\n", status);
				if ((status_ & DATA_RES_MASK) != DATA_RES_ACCEPTED) {
					XPCC_LOG_DEBUG.printf("SD_CARD_ERROR_WRITE\n");
					Cs::set();
					complete(-1);
					return;
				}
				complete(0);
			}
		}
	}

	void begin(uint8_t* src, uint8_t token) {
		this->src = src;
		this->token = token;
		len = 512;

		Cs::reset();

		Spi::write(token);
	}

	uint16_t len;
	uint8_t* src;
	uint8_t token;
};

//write a single block to SD card
template <typename Spi, typename Cs>
class SD_WRITE_BLOCK : public Operation {
public:

	SD_WRITE_BLOCK(size_t block, uint8_t* buffer) {
		//send cmd24
		//write data
		//wait
		//send cmd13
		this->block = block;
		this->buffer = buffer;

		cmd.begin(24, block);
	}

	void handleTick() {
		//first command is finished
		if(cmd.cmd == 24 && cmd.isComplete) {
			XPCC_LOG_DEBUG .printf("24 complete %d\n", cmd.status);
			if(cmd.status != 0) {
				complete(cmd.status);
				return;
			}

			cmd.reset();

			//begin write
			writer.begin(buffer, DATA_START_BLOCK);

		} else if(writer.isComplete) {
			//wait for not busy status
			if(Spi::write(0xFF) == 0xFF) {
//FIXME: add timeout
				cmd.begin(13, 0);

			}
		} else if(cmd.cmd == 13 && cmd.isComplete) {
			Cs::set();
			complete(cmd.status);
		}

	}
	size_t block;
	uint8_t* buffer;

	SD_CMD<Spi, Cs> cmd;
	SD_WRITE_DATA<Spi, Cs> writer;
};

//class SD_WRITE

template <typename Spi, typename Cs>
class SDCardAsync : public SDCard<Spi, Cs>, public xpcc::TickerTask {
public:
	SDCardAsync() {
		op = 0;
	}

	inline bool isOpInprogress() {
		XPCC_LOG_DEBUG .printf("test %d %d %d\n", op!=0, !Cs::read(), this->cardLocked.test());
		return op != 0 || !Cs::read() || this->cardLocked.test();
	}

	xpcc::FunctionPointer& readAsync(uint8_t* buffer, size_t length) {
		if(op) {
			XPCC_LOG_DEBUG .printf("readAsync: prev operation was not complete\n");
			delete op;
		}

		op = new SD_READ_DATA<Spi, Cs>(buffer, length);
		callback.attach(0);

		return callback;
	}

	xpcc::FunctionPointer& writeBlockAsync(size_t block, uint8_t* buffer) {
		if(op)
			delete op;

		op = new SD_WRITE_BLOCK<Spi, Cs>(block * this->cdv, buffer);
		callback.attach(0);

		return callback;
	}

	void waitCardNotBusy() {
		while(isOpInprogress()) {
			XPCC_LOG_DEBUG.printf("still busy\n");
			handleTick();
		}
	}

protected:

	void handleTick() {
		Operation* o = Operation::base;
		while(o) {
			o->tick();
			o = op->next;
		}

		if(op && op->isComplete) {
			//XPCC_LOG_DEBUG.printf("onComplete (%d)\n", op->status);

			//XPCC_LOG_DEBUG.printf("delete op %x\n", op);
			delete op;
			op = 0;

			callback();
		}
	}

	Operation *op;
	xpcc::FunctionPointer callback;

};



#endif /* SDCARDASYNC_H_ */
