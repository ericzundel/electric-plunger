/* Arduino WaveHC Library
 * Copyright (C) 2008 by William Greiman
 *  
 * This file is part of the Arduino FAT16 Library
 *  
 * This Library is free software: you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 
 * You should have received a copy of the GNU General Public License
 * along with the Arduino Fat16 Library.  If not, see
 * <http://www.gnu.org/licenses/>.
 */
#ifndef SdReader_h
#define SdReader_h
#include "SdInfo.h"
/** Optional readCID(), readCSD() and cardSize() if nonzero */
#define SD_CARD_INFO_SUPPORT 1
//
// SD card commands
/** GO_IDLE_STATE - init card in spi mode if CS low */
#define CMD0     0X00      
/** SEND_IF_COND - verify SD Memory Card interface operating condition.*/
#define CMD8     0X08
/** SEND_CSD - read the Card Specific Data (CSD register) */
#define CMD9     0X09      
 /** SEND_CID - read the card identification information (CID register) */
#define CMD10    0X0A     
/** READ_BLOCK - read a single data block from the card */
#define CMD17    0X11
/** APP_CMD - escape for application specific command */
#define CMD55    0X37      
/** READ_OCR - read the OCR register of a card */
#define CMD58    0X3A
/** SD_SEND_OP_COMD - Sends host capacity support information and
    activates the card's initialization process */
#define ACMD41   0X29      
//
// SD card errors
/** timeout error for command CMD0 */
#define SD_CARD_ERROR_CMD0  0X1
/** CMD8 was not accepted - not a valid SD card*/
#define SD_CARD_ERROR_CMD8  0X2
/** card returned an error response for CMD17 (read block) */
#define SD_CARD_ERROR_CMD17 0X3
/** card returned an error response for CMD24 (write block) */
#define SD_CARD_ERROR_CMD24 0X4
/** card returned an error response for CMD58 (read OCR) */
#define SD_CARD_ERROR_CMD58 0X5
/** card's ACMD41 initialization process timeout */
#define SD_CARD_ERROR_ACMD41 0X6
/** card returned a bad CSR version field */
#define SD_CARD_ERROR_BAD_CSD 0X7
/** read CID or CSD failed */
#define SD_CARD_ERROR_READ_REG 0X8
/** timeout occurred during write programming */
#define SD_CARD_ERROR_WRITE_TIMEOUT 0X9
/** attempt to write protected block zero */
#define SD_CARD_ERROR_WRITE_BLOCK_ZERO 0XA
////////////////////////////////////////define low bits in next errors////////////////
/** card returned an error token instead of read data */
#define SD_CARD_ERROR_READ 0X10
/** card returned an error token as a response to a write operation  */
#define SD_CARD_ERROR_WRITE 0X20
//
// card types
/** Standard capacity V1 SD card */
#define SD_CARD_TYPE_SD1 1
/** Standard capacity V2 SD card */
#define SD_CARD_TYPE_SD2 2
/** High Capacity SD card */
#define SD_CARD_TYPE_SDHC 3
/**
 * \class SdReader
 * \brief Hardware access class for SD flash cards
 *  
 * Supports raw access to SD and SDHC flash memory cards.
 *
 */
class SdReader  {
  uint32_t block_;
  uint8_t errorCode_;
  uint8_t errorData_;
  uint8_t inBlock_;
  uint16_t offset_;
  uint8_t partialBlockRead_;
  uint8_t response_;
  uint8_t type_;
  void (*busyFunc_)();
  uint8_t cardCommand(uint8_t cmd, uint32_t arg, uint8_t crc = 0XFF);
  void error(uint8_t code){errorCode_ = code;}
  void error(uint8_t code, uint8_t data) {errorCode_ = code; errorData_ = data;}
  uint8_t readRegister(uint8_t cmd, uint8_t *dst);
  void type(uint8_t value) {type_ = value;}
  uint8_t waitStartBlock(void);
public:
  /** Construct an instance of SdReader. */
  SdReader(void) :  errorCode_(0), inBlock_(0), partialBlockRead_(0), type_(0) {};
  uint32_t cardSize(void);
  /** \return error code for last error */
  uint8_t errorCode(void) {return errorCode_;}
  /** \return error data for last error */
  uint8_t errorData(void) {return errorData_;}
  uint8_t init(uint8_t slow = 0);
  void setBusyFunc(void (*busyFunc)()) {busyFunc_ = busyFunc;};
  /**
   * Enable or disable partial block reads.
   * 
   * Enabling partial block reads improves performance by allowing a block
   * to be read over the spi bus as several sub-blocks.  Errors will occur
   * if the time between reads is too long since the SD card will timeout.
   *
   * Use this for applications like the Adafruit Wave Shield.
   *
   * \param[in] value The value TRUE (non-zero) or FALSE (zero).)   
   */     
  void partialBlockRead(uint8_t value) {readEnd(); partialBlockRead_ = value;}
  /**
 * Read a 512 byte block from a SD card device.
 *
 * \param[in] block Logical block to be read.
 * \param[out] dst Pointer to the location that will receive the data. 

 * \return The value one, true, is returned for success and
 * the value zero, false, is returned for failure.      
 */ 
  uint8_t readBlock(uint32_t block, uint8_t *dst) {
    return readData(block, 0, dst, 512);}
  uint8_t readData(uint32_t block, uint16_t offset, uint8_t *dst, uint16_t count);
  /** 
   * Read a cards CID register. The CID contains card identification information
   * such as Manufacturer ID, Product name, Product serial number and
   * Manufacturing date. */
  uint8_t readCID(cid_t &cid) {return readRegister(CMD10, (uint8_t *)&cid);}
  /** 
   * Read a cards CSD register. The CSD contains Card-Specific Data that
   * provides information regarding access to the card contents. */
  uint8_t readCSD(csd_t &csd) {return readRegister(CMD9, (uint8_t *)&csd);}
  void readEnd(void);
  /** Return the card type: SD V1, SD V2 or SDHC */
  uint8_t type() {return type_;}
};
#endif //SdReader_h
