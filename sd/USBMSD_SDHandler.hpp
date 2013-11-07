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

	ProfileTimer tm;


    uint8_t buffer[512];
    uint32_t requestedBlock = -1;

    uint8_t *dataptr;
    uint32_t blocksLeft;

    volatile int32_t readingBlock = -1; //currently reading block
    volatile int32_t haveBlock = -1; //specifies which block is in the buffer

    void onBlockRead() {
    	XPCC_LOG_DEBUG .printf("X:Have block %d\n", readingBlock);

    	haveBlock = readingBlock;
    	readingBlock = -1;
    }

    void handleTick() override {

    	if(requestedBlock != -1 && haveBlock == -1 && readingBlock == -1) {

    		readingBlock = requestedBlock;
    		XPCC_LOG_DEBUG .printf("X:first read %d\n", requestedBlock);
      		card->readAsync(buffer, 512).attach(this, &USBMSD_SDHandler::onBlockRead);
    	}

    	if(haveBlock == requestedBlock && requestedBlock != -1) {
    		xpcc::atomic::Lock lock;
    		memcpy(dataptr, buffer, 512);

    		if(blocksLeft > 0) {
    			readingBlock = requestedBlock+1;
    			haveBlock = -1;
    			XPCC_LOG_DEBUG .printf("X:read %d\n", readingBlock);
    			card->readAsync(buffer, 512).attach(this, &USBMSD_SDHandler::onBlockRead);
    		} else {
    			card->readStop();
    		}
    		requestedBlock = -1;

    		XPCC_LOG_DEBUG.printf("X:writing block\n");
    		disk_read_finalize(true);

    	}
    }

    void transfer_begins(TransferType type, uint32_t startBlock, int numBlocks) override{
		XPCC_LOG_DEBUG .printf("TestMSD::start transfer %d %d %d\n", type, startBlock, numBlocks);

		readingBlock = -1;
		haveBlock = -1;
		requestedBlock = -1;

		if(card) {
			if(type == READ) {
				card->readStart(startBlock);
			} else if(type == WRITE) {
				card->writeStart(startBlock, numBlocks);
			}
		}
    }

    int disk_read_start(uint8_t * data, uint32_t block, uint32_t blocksLeft) override {
    	requestedBlock = block;
    	dataptr = data;
    	this->blocksLeft = blocksLeft;

		//XPCC_LOG_DEBUG .printf("TestMSD::disk_read_start %d %d\n", (uint32_t)block, blocksLeft);

//    	//we have the next block already
//    	if(this->block == block) {
//    		this->block = -1;
//
//    		memcpy(data, buffer, 512);
//
//    		disk_read_finalize(true);
//
//    	} else {
//
//			card->readAsync(data, 512).attach([this, blocksLeft, block](){
//				//lambda function on read complete
//
//				if(blocksLeft == 0) {
//					bufReading = false;
//					if(card)
//						card->readStop();
//
//				} else {
//					bufReading = true;
//					card->readAsync(buffer, 512).attach([this, block]() {
//						this->block = block+1;
//
//					});
//
//				}
//				//XPCC_LOG_DEBUG .printf("fb %d\n", blocksLeft);
//				disk_read_finalize(true);
//			});
//    	}


		return 0;
    }

    int disk_write_start(const uint8_t * data, uint32_t block, uint32_t blocksLeft) override {
            //XPCC_LOG_DEBUG .printf("TestMSD::writeStart %d %d %d\n", _write, block, blocksLeft);
            //_write = true;

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
