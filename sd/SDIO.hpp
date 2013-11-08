/*
 * SDIO.hpp
 *
 *  Created on: Oct 30, 2013
 *      Author: walmis
 */

#ifndef SDIO_HPP_
#define SDIO_HPP_

#include "SDCardAsync.h"
#include <fatfs/diskio.h>
#include <fatfs/ff.h>
#include <xpcc/driver/storage/fat.hpp>


template <typename Spi, typename Cs>
class SDIOAsync : public SDCardAsync<Spi,Cs>, public xpcc::fat::PhysicalVolume {

	DSTATUS doInitialize () override {
		XPCC_LOG_DEBUG .printf("%s()\n", __FUNCTION__);

		return RES_OK;
	}

	DSTATUS doGetStatus () override {
		XPCC_LOG_DEBUG .printf("%s()\n", __FUNCTION__);
		return RES_OK;
	}

	xpcc::fat::Result
	doRead(uint8_t *buffer, int32_t sectorNumber, uint32_t sectorCount) override {
		XPCC_LOG_DEBUG .printf("%s(%d, %d)\n", __FUNCTION__, sectorNumber, sectorCount);

		if(this->isOpInprogress()) {
			return RES_ERROR;
		}

		if(sectorCount == 1) {
			XPCC_LOG_DEBUG .printf("read single block\n");
			if(this->readSingleBlock(buffer, sectorNumber)) {
				//XPCC_LOG_DEBUG .dump_buffer(buffer, 512);
				return RES_OK;
			}
		}

		return RES_ERROR;
	}

	xpcc::fat::Result
	doWrite(const uint8_t *buffer, int32_t sectorNumber, uint32_t sectorCount) override {
		XPCC_LOG_DEBUG .printf("%s(%d, %d)\n", __FUNCTION__, sectorNumber, sectorCount);

		if(this->isOpInprogress()) {
			return RES_ERROR;
		}

		if(sectorCount == 1) {
			if(this->writeBlock(sectorNumber, buffer)) {
				return RES_OK;
			} else {
				return RES_ERROR;
			}

		}

		return RES_ERROR;
	}

	xpcc::fat::Result
	doIoctl(uint8_t command, uint32_t *buffer) override {
		XPCC_LOG_DEBUG .printf("%s(%d)\n", __FUNCTION__, command);
		return RES_OK;
	}

};

#endif /* SDIO_HPP_ */
