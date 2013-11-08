/*
 * USBMSD_SDHandler.hpp
 *
 *  Created on: Oct 31, 2013
 *      Author: walmis
 */

#ifndef USBMSD_SDHANDLER_HPP_
#define USBMSD_SDHANDLER_HPP_

#include <xpcc/architecture.hpp>
#include "SDCardAsync.h"

namespace xpcc {


template <class SDCardAsync>
class USBMSD_SDHandler : public USBMSDHandler, TickerTask {
public:
	USBMSD_SDHandler(uint8_t bulkIn = EPBULK_IN,
                    uint8_t bulkOut = EPBULK_OUT) :
                            USBMSDHandler(bulkIn, bulkOut) {}

	void assignCard(SDCardAsync& card) {
		this->card = &card;
	}

protected:

    uint8_t buffer[512];
    uint32_t requestedBlock = -1;

    uint8_t *dataptr;
    uint32_t blocksLeft;

    TransferType opType;
    uint32_t totalBlocks;

    volatile int32_t readingBlock = -1; //currently reading block
    volatile int32_t haveBlock = -1; //specifies which block is in the buffer

    void onBlockRead() {
    	//XPCC_LOG_DEBUG .printf("X:Have block %d\n", readingBlock);

    	haveBlock = readingBlock;
    	readingBlock = -1;
    }

    void handleTick() override {

    	if(requestedBlock != -1 && haveBlock == -1 && readingBlock == -1) {
			bool res;
    		if(opType == READ) {
    			if(!card->readStart(requestedBlock)) {
    				disk_read_finalize(false);
    				requestedBlock = -1;
    				return;
    			}

			} else if(opType == WRITE) {
				card->writeStart(requestedBlock, totalBlocks);
			}

    		readingBlock = requestedBlock;
    		//XPCC_LOG_DEBUG .printf("X:first read %d\n", requestedBlock);
      		card->readAsync(buffer, 512).attach(this, &USBMSD_SDHandler::onBlockRead);
    	}

    	//if(requestedBlock != -1 && v.isExpired()){
    	if(haveBlock == requestedBlock && requestedBlock != -1) {
    		//xpcc::atomic::Lock lock;
    		memcpy(dataptr, buffer, 512);

    		if(blocksLeft > 0) {
    			readingBlock = requestedBlock+1;
    			haveBlock = -1;
    			//XPCC_LOG_DEBUG .printf("X:read %d\n", readingBlock);
    			card->readAsync(buffer, 512).attach(this, &USBMSD_SDHandler::onBlockRead);
    		} else {
    			card->readStop();
    		}
    		requestedBlock = -1;

    		//XPCC_LOG_DEBUG.printf("X: finalizing write\n");

    		disk_read_finalize(true);
    		//XPCC_LOG_DEBUG.printf("---\n");
    	}
    }

    void transfer_begins(TransferType type, uint32_t startBlock, int numBlocks) override{
		//XPCC_LOG_DEBUG .printf("TestMSD::start transfer %d %d %d\n", type, startBlock, numBlocks);

		readingBlock = -1;
		haveBlock = -1;
		requestedBlock = -1;
		opType = type;
		totalBlocks = numBlocks;
    }

    int disk_read_start(uint8_t * data, uint32_t block, uint32_t blocksLeft) override {
    	requestedBlock = block;
    	dataptr = data;
    	this->blocksLeft = blocksLeft;

		return 0;
    }

    int disk_write_start(const uint8_t * data, uint32_t block, uint32_t blocksLeft) override {

    	disk_write_finalize(true);

            return 0;
    }

    int disk_initialize() override {
            XPCC_LOG_DEBUG .printf("disk initialize\n");

            return DISK_OK;
    }

    uint32_t disk_sectors() override {
    	//return card->numSectors();
    	XPCC_LOG_DEBUG .printf("card sector count %d\n", card->numSectors());
    	return card->numSectors();
    }

    uint16_t disk_sector_size() override {
            return 512;
    }

    int disk_status() override {
    	//return NO_DISK;
    	//return DISK_OK | WRITE_PROTECT;
    	if(!card)
    		return NO_DISK;

    	if(card->isInitialised()) {
            return DISK_OK | WRITE_PROTECT;
    	} else {
    		return NO_DISK;
    	}
    }

protected:

	SDCardAsync* card;
};

}


#endif /* USBMSD_SDHANDLER_HPP_ */
