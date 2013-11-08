/* mbed Microcontroller Library
 * Copyright (c) 2006-2012 ARM Limited
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef MBED_SDFILESYSTEM_H
#define MBED_SDFILESYSTEM_H

#include <stdint.h>
#include <xpcc/architecture.hpp>

/* Introduction
 * ------------
 * SD and MMC cards support a number of interfaces, but common to them all
 * is one based on SPI. This is the one I'm implmenting because it means
 * it is much more portable even though not so performant, and we already
 * have the mbed SPI Interface!
 *
 * The main reference I'm using is Chapter 7, "SPI Mode" of:
 *  http://www.sdcard.org/developers/tech/sdcard/pls/Simplified_Physical_Layer_Spec.pdf
 *
 * SPI Startup
 * -----------
 * The SD card powers up in SD mode. The SPI interface mode is selected by
 * asserting CS low and sending the reset command (CMD0). The card will
 * respond with a (R1) response.
 *
 * CMD8 is optionally sent to determine the voltage range supported, and
 * indirectly determine whether it is a version 1.x SD/non-SD card or
 * version 2.x. I'll just ignore this for now.
 *
 * ACMD41 is repeatedly issued to initialise the card, until "in idle"
 * (bit 0) of the R1 response goes to '0', indicating it is initialised.
 *
 * You should also indicate whether the host supports High Capicity cards,
 * and check whether the card is high capacity - i'll also ignore this
 *
 * SPI Protocol
 * ------------
 * The SD SPI protocol is based on transactions made up of 8-bit words, with
 * the host starting every bus transaction by asserting the CS signal low. The
 * card always responds to commands, data blocks and errors.
 *
 * The protocol supports a CRC, but by default it is off (except for the
 * first reset CMD0, where the CRC can just be pre-calculated, and CMD8)
 * I'll leave the CRC off I think!
 *
 * Standard capacity cards have variable data block sizes, whereas High
 * Capacity cards fix the size of data block to 512 bytes. I'll therefore
 * just always use the Standard Capacity cards with a block size of 512 bytes.
 * This is set with CMD16.
 *
 * You can read and write single blocks (CMD17, CMD25) or multiple blocks
 * (CMD18, CMD25). For simplicity, I'll just use single block accesses. When
 * the card gets a read command, it responds with a response token, and then
 * a data token or an error.
 *
 * SPI Command Format
 * ------------------
 * Commands are 6-bytes long, containing the command, 32-bit argument, and CRC.
 *
 * +---------------+------------+------------+-----------+----------+--------------+
 * | 01 | cmd[5:0] | arg[31:24] | arg[23:16] | arg[15:8] | arg[7:0] | crc[6:0] | 1 |
 * +---------------+------------+------------+-----------+----------+--------------+
 *
 * As I'm not using CRC, I can fix that byte to what is needed for CMD0 (0x95)
 *
 * All Application Specific commands shall be preceded with APP_CMD (CMD55).
 *
 * SPI Response Format
 * -------------------
 * The main response format (R1) is a status byte (normally zero). Key flags:
 *  idle - 1 if the card is in an idle state/initialising
 *  cmd  - 1 if an illegal command code was detected
 *
 *    +-------------------------------------------------+
 * R1 | 0 | arg | addr | seq | crc | cmd | erase | idle |
 *    +-------------------------------------------------+
 *
 * R1b is the same, except it is followed by a busy signal (zeros) until
 * the first non-zero byte when it is ready again.
 *
 * Data Response Token
 * -------------------
 * Every data block written to the card is acknowledged by a byte
 * response token
 *
 * +----------------------+
 * | xxx | 0 | status | 1 |
 * +----------------------+
 *              010 - OK!
 *              101 - CRC Error
 *              110 - Write Error
 *
 * Single Block Read and Write
 * ---------------------------
 *
 * Block transfers have a byte header, followed by the data, followed
 * by a 16-bit CRC. In our case, the data will always be 512 bytes.
 *
 * +------+---------+---------+- -  - -+---------+-----------+----------+
 * | 0xFE | data[0] | data[1] |        | data[n] | crc[15:8] | crc[7:0] |
 * +------+---------+---------+- -  - -+---------+-----------+----------+
 */

/** Access the filesystem on an SD Card using SPI
 *
 * @code
 * #include "mbed.h"
 * #include "SDFileSystem.h"
 *
 * SDFileSystem sd(p5, p6, p7, p12, "sd"); // mosi, miso, sclk, cs
 *
 * int main() {
 *     FILE *fp = fopen("/sd/myfile.txt", "w");
 *     fprintf(fp, "Hello World!\n");
 *     fclose(fp);
 * }
 */

#define SD_DBG             0

#define R1_IDLE_STATE           (1 << 0)
#define R1_ERASE_RESET          (1 << 1)
#define R1_ILLEGAL_COMMAND      (1 << 2)
#define R1_COM_CRC_ERROR        (1 << 3)
#define R1_ERASE_SEQUENCE_ERROR (1 << 4)
#define R1_ADDRESS_ERROR        (1 << 5)
#define R1_PARAMETER_ERROR      (1 << 6)

#define SD_COMMAND_TIMEOUT 50

// Types
//  - v1.x Standard Capacity
//  - v2.x Standard Capacity
//  - v2.x High Capacity
//  - Not recognised as an SD Card
#define SDCARD_FAIL 0
#define SDCARD_V1   1
#define SDCARD_V2   2
#define SDCARD_V2HC 3

uint8_t const DATA_START_BLOCK = 0XFE;
/** stop token for write multiple blocks*/
uint8_t const STOP_TRAN_TOKEN = 0XFD;

/** init timeout ms */
uint16_t const SD_INIT_TIMEOUT = 2000;
/** erase timeout ms */
uint16_t const SD_ERASE_TIMEOUT = 10000;
/** read timeout ms */
uint16_t const SD_READ_TIMEOUT = 300;
/** write time out ms */
uint16_t const SD_WRITE_TIMEOUT = 600;
/** start data token for write multiple blocks*/
uint8_t const WRITE_MULTIPLE_TOKEN = 0XFC;
/** mask for data response tokens after a write block operation */
uint8_t const DATA_RES_MASK = 0X1F;
/** write data accepted token */
uint8_t const DATA_RES_ACCEPTED = 0X05;

template<typename Spi, typename Cs>
class SDCard {
public:

	SDCard() {
		Cs::setOutput(1);
		initialized = false;
	}

	bool isInitialised() {
		return initialized;
	}

	bool initialise() {

		if(initialized)
			return true;

		PROFILE();

		int i = initialise_card();
		XPCC_LOG_DEBUG.printf("init card = %d\n", i);
		if (i == 0) {
			return false;
		}

		_sectors = _sd_sectors();

		// Set block length to 512 (CMD16)
		if (_cmd(16, 512) != 0) {
			XPCC_LOG_DEBUG.printf("Set 512-byte block timed out\n");
			return false;
		}

		Spi::frequency(25000000);

		initialized = true;

		onInitialized();
		cardLocked.reset();

		return true;
	}

	virtual void onInitialized() {	}

	xpcc::atomic::Flag cardLocked;

	//------------------------------------------------------------------------------
	/** Begin a read multiple blocks sequence.
	 *
	 * \return The value one, true, is returned for success and
	 * the value zero, false, is returned for failure.
	 */
	bool readStart(uint32_t blockNumber) {
		//PROFILE();
		//SD_TRACE("RS", blockNumber);
		//if (type()!= SD_CARD_TYPE_SDHC) blockNumber <<= 9;
		cardLocked.set();

		if (_cmd(18, blockNumber * cdv) != 0) {
			initialized = false;
			XPCC_LOG_DEBUG.printf("SD_CARD_ERROR_CMD18\n");
			return false;
		}
		return true;
	}
	//------------------------------------------------------------------------------
	/** End a read multiple blocks sequence.
	 *
	 * \return The value one, true, is returned for success and
	 * the value zero, false, is returned for failure.
	 */
	bool readStop() {
		//PROFILE();

		Cs::reset();

		cardLocked.reset();

		if (_cmd(12, 0) == -1) {
			goto fail;
		}
		Cs::set();
		return true;

		fail:
			initialized = false;
			Cs::set();
		return false;
	}

	bool readData(uint8_t* buffer, size_t length);

	bool readSingleBlock(uint8_t* buffer, size_t block_number) {

		if (_cmd(17, block_number * cdv) != 0) {
			XPCC_LOG_DEBUG .printf("readSingleBlock cmd17 failed\n");
			return false;
		}

		return readData(buffer, 512);
	}

	//------------------------------------------------------------------------------
	/** Start a write multiple blocks sequence.
	 *
	 * \param[in] blockNumber Address of first block in sequence.
	 * \param[in] eraseCount The number of blocks to be pre-erased.
	 *
	 * \note This function is used with writeData() and writeStop()
	 * for optimized multiple block writes.
	 *
	 * \return The value one, true, is returned for success and
	 * the value zero, false, is returned for failure.
	 */
	bool writeStart(uint32_t blockNumber, uint32_t eraseCount) {
		//SD_TRACE("WS", blockNumber);
		// send pre-erase count
		PROFILE();
		cardLocked.set();
		if (_acmd(23, eraseCount) != 0) {
			XPCC_LOG_DEBUG.printf("SD_CARD_ERROR_ACMD23\n");
			goto fail;
		}

		if (_cmd(25, blockNumber * cdv) != 0) {
			XPCC_LOG_DEBUG.printf("SD_CARD_ERROR_CMD25\n");
			goto fail;
		}
		Cs::set();
		return true;

		fail:
		initialized = false;
		Cs::set();
		return false;
	}
	//------------------------------------------------------------------------------
	/** End a write multiple blocks sequence.
	 *
	 * \return The value one, true, is returned for success and
	 * the value zero, false, is returned for failure.
	 */
	bool writeStop() {
		PROFILE();
		Cs::reset();


		if (!waitNotBusy(SD_WRITE_TIMEOUT))
			goto fail;

		Spi::write(STOP_TRAN_TOKEN);

		if (!waitNotBusy(SD_WRITE_TIMEOUT))
			goto fail;
		Cs::set();
		cardLocked.reset();
		return true;

		fail:
			initialized = false;
			XPCC_LOG_DEBUG.printf("SD_CARD_ERROR_STOP_TRAN\n");
			Cs::set();
			cardLocked.reset();
		return false;
	}

	//------------------------------------------------------------------------------
	/** Write one data block in a multiple block write sequence
	 * \param[in] src Pointer to the location of the data to be written.
	 * \return The value one, true, is returned for success and
	 * the value zero, false, is returned for failure.
	 */
	bool writeData(const uint8_t* src) {
		PROFILE();
		Cs::reset();
		// wait for previous write to finish
		if (!waitNotBusy(SD_WRITE_TIMEOUT))
			goto fail;
		if (!writeData(WRITE_MULTIPLE_TOKEN, src))
			goto fail;
		Cs::set();
		return true;

		fail:
		initialized = false;
		XPCC_LOG_DEBUG.printf("SD_CARD_ERROR_WRITE_MULTIPLE\n");
		Cs::set();
		return false;
	}
	//------------------------------------------------------------------------------
	// send one block of data for write block or write multiple blocks
	bool writeData(uint8_t token, const uint8_t* src);

	//write a single block to SD card
	bool writeBlock(uint32_t blockNumber, const uint8_t* src);

	//wait for card to go not busy
	bool waitNotBusy(uint16_t timeoutMs) {
		//PROFILE();
		xpcc::Timeout<> t(timeoutMs);

		while (!t.isExpired()) {
			if (!isBusy())
				return true;
		}
		return false;
	}

	bool isBusy() {
		return Spi::write(0xFF) != 0xFF;
	}

	uint32_t numSectors() {
		return _sectors;
	}

protected:

	int _acmd(int cmd, int arg) {
		_cmd(55, 0);
		return _cmd(cmd, arg);
	}

	int _cmd(int cmd, int arg, int timeout = SD_COMMAND_TIMEOUT);
	int _cmd8();
	int _cmd58();
	int initialise_card();
	int initialise_card_v1();
	int initialise_card_v2();



	bool initialized;

	static uint32_t ext_bits(unsigned char* data, int msb, int lsb);

	uint32_t _sd_sectors();

	uint32_t _sectors;

	int cdv;
};

template<typename Spi, typename Cs>
inline bool SDCard<Spi, Cs>::readData(uint8_t* buffer, size_t length) {
	PROFILE();
	Cs::reset();
	// read until start byte (0xFF)
	xpcc::Timeout<> t(100);
	while (Spi::write(0xFF) != 0xFE) {
		if (t.isExpired()) {
			initialized = false;
			return false;
		}
	}
	// read data
	for (uint32_t i = 0;i < length;i++) {
		buffer[i] = Spi::write(0xFF);
	}
	Spi::write(0xFF); // checksum
	Spi::write(0xFF);
	Cs::set();
	Spi::write(0xFF);
	return true;
}

template<typename Spi, typename Cs>
inline bool SDCard<Spi, Cs>::writeData(uint8_t token, const uint8_t* src) {
	Cs::reset();

#if USE_SD_CRC
		uint16_t crc = CRC_CCITT(src, 512);
#else  // USE_SD_CRC
		uint16_t crc = 0XFFFF;
#endif  // USE_SD_CRC

	Spi::write(token);
	for (int i = 0; i < 512; i++) {
		Spi::write(*src++);
	}
	Spi::write(crc >> 8);
	Spi::write(crc & 0XFF);
	uint8_t status_ = Spi::write(0xFF);
	if ((status_ & DATA_RES_MASK) != DATA_RES_ACCEPTED) {
		XPCC_LOG_DEBUG.printf("SD_CARD_ERROR_WRITE(%x)\n", status_);
		goto fail;
	}
	return true;
	fail:
	initialized = false;
	Cs::set();
	return false;
}

template<typename Spi, typename Cs>
inline bool SDCard<Spi, Cs>::writeBlock(uint32_t blockNumber,
		const uint8_t* src) {
	//SD_TRACE("WB", blockNumber);
	PROFILE();
	if (_cmd(24, blockNumber * cdv)) {
		XPCC_LOG_DEBUG.printf("SD_CARD_ERROR_CMD24\n");
		goto fail;
	}
	if (!writeData(DATA_START_BLOCK, src))
	goto fail;

	// wait for flash programming to complete
	if (!waitNotBusy(SD_WRITE_TIMEOUT)) {
		XPCC_LOG_DEBUG.printf("SD_CARD_ERROR_WRITE_TIMEOUT\n");
		goto fail;
	}
	// response is r2 so get and check two bytes for nonzero
	if (_cmd(13, 0) != 0) {
		XPCC_LOG_DEBUG.printf("SD_CARD_ERROR_WRITE_PROGRAMMING\n");
		goto fail;
	}
	Cs::set();
	Spi::write(0xFF);
	return true;
	fail:
	Cs::set();
	Spi::write(0xFF);
	initialized = false;
	return false;
}

template<typename Spi, typename Cs>
inline int SDCard<Spi, Cs>::_cmd(int cmd, int arg, int timeout) {
	//PROFILE();
	Cs::reset();

	waitNotBusy(300);

	// send a command
	Spi::write(uint8_t(0x40 | cmd));
	Spi::write(uint8_t(arg >> 24));
	Spi::write(uint8_t(arg >> 16));
	Spi::write(uint8_t(arg >> 8));
	Spi::write(uint8_t(arg >> 0));
	Spi::write(0x95);
	//XPCC_LOG_DEBUG.printf("_cmd(%d, %x) = ", cmd, arg);
	// wait for the repsonse (response[7] == 0)
	xpcc::Timeout<> t(timeout);
	while (!t.isExpired()) {
		int response = Spi::write(0xFF);
		if (!(response & 0x80)) {
			Cs::set();
			Spi::write(0xFF);
			//XPCC_LOG_DEBUG.printf("_cmd(%d, %x) = %d\n", cmd, arg, response);
			return response;
		}
	}

	XPCC_LOG_DEBUG.printf("timeout\n");
	initialized = false;

	Cs::set();
	Spi::write(0xFF);
	return -1; // timeout
}

template<typename Spi, typename Cs>
inline int SDCard<Spi, Cs>::_cmd8() {
	Cs::reset();
	// send a command
	Spi::write(0x40 | 8); // CMD8
	Spi::write(0x00); // reserved
	Spi::write(0x00); // reserved
	Spi::write(0x01); // 3.3v
	Spi::write(0xAA); // check pattern
	Spi::write(0x87); // crc
	// wait for the repsonse (response[7] == 0)
	xpcc::Timeout<> t(SD_COMMAND_TIMEOUT);
	while (!t.isExpired()) {
		char response[5];
		response[0] = Spi::write(0xFF);
		if (!(response[0] & 0x80)) {
			for (int j = 1; j < 5; j++) {
				response[j] = Spi::write(0xFF);
			}
			Cs::set();
			Spi::write(0xFF);
			XPCC_LOG_DEBUG.printf("_cmd8() = %x\n", response[0]);
			return response[0];
		}
	}

	initialized = false;
	Cs::set();
	Spi::write(0xFF);
	return -1; // timeout
}

template<typename Spi, typename Cs>
inline int SDCard<Spi, Cs>::_cmd58() {
	Cs::reset();
	int arg = 0;
	// send a command
	Spi::write(0x40 | 58);
	Spi::write(arg >> 24);
	Spi::write(arg >> 16);
	Spi::write(arg >> 8);
	Spi::write(arg >> 0);
	Spi::write(0x95);
	// wait for the repsonse (response[7] == 0)
	xpcc::Timeout<> t(SD_COMMAND_TIMEOUT);
	while (!t.isExpired()) {
		int response = Spi::write(0xFF);
		if (!(response & 0x80)) {
			int ocr = Spi::write(0xFF) << 24;
			ocr |= Spi::write(0xFF) << 16;
			ocr |= Spi::write(0xFF) << 8;
			ocr |= Spi::write(0xFF) << 0;
			Cs::set();
			Spi::write(0xFF);
			return response;
		}
	}
	initialized = false;
	Cs::set();
	Spi::write(0xFF);
	return -1; // timeout
}

template<typename Spi, typename Cs>
inline int SDCard<Spi, Cs>::initialise_card() {
	// Set to 100kHz for initialisation, and clock card with cs = 1
	Spi::frequency(100000);
	Cs::set();
	for (int i = 0; i < 32; i++) {
		Spi::write(0xFF);
	}
	uint8_t s = _cmd(0, 0, 10);
	XPCC_LOG_DEBUG.printf("_cmd(0, 0) = %x\n", s);
	// send CMD0, should return with all zeros except IDLE STATE set (bit 0)
	if (s != R1_IDLE_STATE) {
		XPCC_LOG_DEBUG.printf(
				"No disk, or could not put SD card in to SPI idle state\n");
		return SDCARD_FAIL;
	}
	// send CMD8 to determine whther it is ver 2.x
	int r = _cmd8();
	if (r == R1_IDLE_STATE) {
		return initialise_card_v2();
	} else if (r == (R1_IDLE_STATE | R1_ILLEGAL_COMMAND)) {
		return initialise_card_v1();
	} else {
		XPCC_LOG_DEBUG.printf(
				"Not in idle state after sending CMD8 (not an SD card?)\n");
		return SDCARD_FAIL;
	}
}

template<typename Spi, typename Cs>
inline int SDCard<Spi, Cs>::initialise_card_v1() {
	xpcc::Timeout<> t(500);
	while (!t.isExpired()) {
		_cmd(55, 0);
		if (_cmd(41, 0) == 0) {
			cdv = 512;
			XPCC_LOG_DEBUG.printf("Init: SDCARD_V1\n");
			return SDCARD_V1;
		}
	}
	XPCC_LOG_DEBUG.printf("Timeout waiting for v1.x card\n");
	return SDCARD_FAIL;
}

template<typename Spi, typename Cs>
inline int SDCard<Spi, Cs>::initialise_card_v2() {
	xpcc::Timeout<> t(500);
	while (!t.isExpired()) {

		_cmd58();
		_cmd(55, 0);
		if (_cmd(41, 0x40000000) == 0) {
			_cmd58();
			XPCC_LOG_DEBUG.printf("Init: SDCARD_V2\n");
			cdv = 1;
			return SDCARD_V2;
		}
	}
	XPCC_LOG_DEBUG.printf("Timeout waiting for v2.x card\n");
	return SDCARD_FAIL;
}

template<typename Spi, typename Cs>
inline uint32_t SDCard<Spi, Cs>::ext_bits(unsigned char* data, int msb,
		int lsb) {
	uint32_t bits = 0;
	uint32_t size = 1 + msb - lsb;
	for (uint32_t i = 0; i < size; i++) {
		uint32_t position = lsb + i;
		uint32_t byte = 15 - (position >> 3);
		uint32_t bit = position & 0x7;
		uint32_t value = (data[byte] >> bit) & 1;
		bits |= value << i;
	}
	return bits;
}

template<typename Spi, typename Cs>
inline uint32_t SDCard<Spi, Cs>::_sd_sectors() {
	uint32_t c_size, c_size_mult, read_bl_len;
	uint32_t block_len, mult, blocknr;
	uint32_t hc_c_size;
	uint64_t blocks, capacity;
	// CMD9, Response R2 (R1 byte + 16-byte block read)
	if (_cmd(9, 0) != 0) {
		XPCC_LOG_DEBUG.printf("Didn't get a response from the disk\n");
		return 0;
	}
	uint8_t csd[16];
	if (!readData(csd, 16)) {
		XPCC_LOG_DEBUG.printf("Couldn't read csd response from disk\n");
		return 0;
	}
	// csd_structure : csd[127:126]
	// c_size        : csd[73:62]
	// c_size_mult   : csd[49:47]
	// read_bl_len   : csd[83:80] - the *maximum* read block length
	int csd_structure = ext_bits(csd, 127, 126);
	switch (csd_structure) {
	case 0:
		cdv = 512;
		c_size = ext_bits(csd, 73, 62);
		c_size_mult = ext_bits(csd, 49, 47);
		read_bl_len = ext_bits(csd, 83, 80);

		block_len = 1 << read_bl_len;
		mult = 1 << (c_size_mult + 2);
		blocknr = (c_size + 1) * mult;
		capacity = blocknr * block_len;
		blocks = capacity / 512;
		XPCC_LOG_DEBUG.printf(
				"SDCard\nc_size: %d \ncapacity: %lld \nsectors: %lld\n", c_size,
				capacity, blocks);
		break;

	case 1:
		cdv = 1;
		hc_c_size = ext_bits(csd, 63, 48);
		blocks = (hc_c_size + 1) * 1024;
		capacity = (blocks * 512) / 1000000;
		XPCC_LOG_DEBUG.printf(
				"SDHC Card \nhc_c_size: %d\ncapacity: %lldMiB \nsectors: %lld\n",
				hc_c_size, capacity, blocks);
		break;

	default:
		XPCC_LOG_DEBUG.printf("CSD struct unsupported\n");
		return 0;
	};
	return blocks;
}

#endif
