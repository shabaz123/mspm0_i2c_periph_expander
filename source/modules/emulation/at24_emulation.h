/*
 * Copyright (c) 2024, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*!****************************************************************************
 *  @file       at24_emulation.h
 *  @brief      Emulation library for at24-style I2C EEPROM
 *  @defgroup   AT24x I2C EEPROM Emulation Library
 *
 *  The AT24x emulation library enables MSPM0 devices to emulate a discrete
 *  EEPROM via an I2C target interface, compatible with I2C bus controllers
 *  which are expecting to use the AT24x I2C EEPROM device protocol.
 *
 *  This emulation library uses the underlying flash memory in the MSPM0 MCU
 *  as the non-volatile storage medium.  The Texas Instruments Type-B EEPROM
 *  emulation library is used to manage storing and loading data from the flash.
 *
 *  This emulation library uses the i2c_tar_driver API for implementing the
 *  I2C target I/F on the MSPM0, such that external I2C bus controllers
 *  connected via I2C to the MSPM0 may read and write the emulated EEPROM
 *  using AT24x compatible I2C commands and addressing.  This library layer
 *  is the glue logic between the I2C target driver API and the TI Type-B
 *  EEPROM emulation library.  As such it is responsible for handling the I2C
 *  commands in the same way as a discrete EERPROM IC would handle them, to the
 *  extent that this is possible with embedded flash memory in the MSPM0.
 *
 *  Known limitations:
 *      (LIMITATION-1): It is possible that >50ms may be required for some
 *      write operations to complete before new read/write commands may be
 *      accepted on the I2C I/F.  This is because of the time needed to emulate
 *      EEPROM in sectored flash memory.  Once more items have been written than
 *      the available EEPROM emulation space, re-mapping is done and a sector
 *      erase must be done.  These operations can take 10's to 100's of
 *      milliseconds depending on the cycling history of the memory and the
 *      operating temperature of the device.  I2C bus controllers issuing
 *      write commands must be aware of this limitation and must either wait
 *      this amount of time after each write transaction to emulated EEPROM,
 *      or re-try until the start is ACK'ed by this device.  Writes to
 *      emulated EEPROM only happen on a STOP condition (doing a write and
 *      issuing a RESTART will not save data to emulated EEPROM).  Once the
 *      STOP condition is issued, the commit of data to emulated EEPROM (flash)
 *      will begin.  During this time, the device will NAK any I2C START
 *      conditions which are addressed to it until the write runs to completion.
 *      A workaround is available to prevent a Linux host from attempting
 *      another operation within this time.  The at24.write_timeout kernel
 *      parameter may be set to a higher value (e.g. 1000ms) to minimize the
 *      possibility of a host access timing out due to an EEPROM write function
 *      taking several hundred ms to complete.  This may be set by adding
 *      "at24.write_timeout=1000" to the kernel parameters in the U-BOOT
 *      configuration file (uEnv.txt).
 *
 ******************************************************************************/

#ifndef COMMUNICATION_AT24_EMULATION_H_
#define COMMUNICATION_AT24_EMULATION_H_

#include "emulation/eeprom_emulation_type_b.h"
#include "communication/i2c_tar_driver.h"

/*!
 * @brief Total # of bytes of EEPROM which are emulated
 *
 * The emulated EEPROM memory map starts from address 0x00 and proceeds up to
 * this value less 1.  This value may be adjusted, but the parameters
 * EEPROM_EMULATION_GROUP_ACCOUNT and EEPROM_EMULATION_SECTOR_INGROUP_ACCOUNT in
 * the TI Type-B EEPROM emulation library header file must be compatible with
 * the size put here.
 */
#define AT24_EMU_TOT_BYTES 4096

/*!
 * @brief Blank data value for unwritten EEPROM addresses
 *
 * This is the data which will be returned when an unwritten emulated EEPROM
 * item is requested to be read by an external I2C bus controller.
 */
#define AT24_EMU_MEM_BLANK 0xFFFFFFFF

/*!
 * @brief Type-B EEPROM emulation item size
 *
 * The TI Type-B EEPROM emulation library requires one item (entry)
 * in the write log to store 4 bytes of actual data (one 32-bit word).
 * This is currently a fixed value based on the Type-B EEPROM library
 * construction, and should not be changed.
 */
#define AT24_EMU_ITEM_BYTES 4

/*!
 * @brief Total number of storable Type-B EEPROM emulation items
 *
 * This is how many Type-B EEPROM items may be stored at the same time.
 */
#define AT24_EMU_TOT_ITEMS (AT24_EMU_TOT_BYTES / AT24_EMU_ITEM_BYTES)

/*!
 * @brief The page size alignment used for paged writes
 *
 * The number of bytes in one page of emulated EEPROM.  This is the also
 * the max number of bytes which may be written as a part of one I2C
 * transaction.  Pages are aligned in EEPROM address space on this boundary.
 * Writing beyond the end of a page in one I2C transaction causes the writes
 * to roll over to the beginning of the page within which the write began.
 */
#define AT24_EMU_PAGE_TOT_BYTES 32

/*!
 * @brief Number of Type-B EEPROM emulation items in one emulated EEPROM page
 *
 * This is how many items are contained in an emulated page.
 */
#define AT24_EMU_PAGE_TOT_ITEMS (AT24_EMU_PAGE_TOT_BYTES / AT24_EMU_ITEM_BYTES)

/*!
 * Simple error checking for known incompatible configurations of this library
 * and the TI Type-B EEPROM emulation library to find simple issues quickly
 */

#if AT24_EMU_TOT_BYTES % AT24_EMU_ITEM_BYTES
#error AT24_EMU_TOT_BYTES MUST BE A MULTIPLE OF 4!
#endif

#if AT24_EMU_PAGE_TOT_BYTES % AT24_EMU_ITEM_BYTES
#error AT24_EMU_PAGE_TOT_BYTES MUST BE A MULTIPLE OF 4!
#endif

#if AT24_EMU_TOT_BYTES > (((EEPROM_EMULATION_SECTOR_INGROUP_ACCOUNT * 1024) \
    - 8)/ 2)
#error AT24_EMU_TOT_BYTES CANNOT BE SUPPORTED BY EEPROM CONFIGURATION!
#endif

/**
 *  @brief      Open the AT24x EEPROM emulation library
 *
 *  This function opens the EEPROM emulation library.  It must be called
 *  at start-up before I2C requests from external I2C bus controllers may be
 *  handled by the device.
 *
 *  This function expects that the I2C target driver instance which is passed
 *  has been initialized by the application with the expected configuration
 *  settings and slave address.
 *
 *  This function expects that the application above this layer has configured
 *  the IOMUX to enable I2C communication on the desired pins to the module
 *  whose base address was passed to this function.
 *
 *  Once this function returns and the I2C target driver is enabled,
 *  it is possible for external I2C bus controllers to start
 *  reading or writing EEPROM data within the defined address range of
 *  0 to AT24_EMU_ITEM_BYTES-1, with writes page-aligned to the
 *  AT24_EMU_PAGE_TOT_BYTES boundary (else roll-over will be observed).
 *
 *  This function may need to initialize flash memory space used for EEPROM
 *  emulation by the TI Type-B EEPROM emulation library.  As flash operations
 *  may be needed, global interrupts are disabled during flash operations to
 *  avoid issues.  The calling application must be aware of this.
 *
 *  @param[in]  Pointer to the I2C target driver instance to use.
 *
 *  @return     Zero if the library initialized correctly, else returns
 *              the EEPROM emulation library error code to the calling function.
 *
 */
extern uint32_t at24_open(i2c_tar_driver_t *pI2CTarDriverInst);

/**
 *  @brief      I2C target receive callback handler
 *
 *  This callback function must be connected to the I2C target driver.
 *  It is responsible for handling incoming I2C data to this module from an
 *  external I2C bus controller (this is the scenario of an I2C bus write).
 *
 *  @param      bytes is the number of bytes received for processing
 *
 *  @param      trig indicates if we got this data after a RESTART
 *              or a STOP condition on the I2C bus
 *
 *  @return    none
 *
 */
extern void at24_i2c_rx_callback(uint32_t bytes, \
    i2c_tar_driver_call_trig_t trig);

/**
 *  @brief      I2C target transmit callback handler
 *
 *  This callback function must be connected to the I2C target driver.
 *  It is responsible for handling outbound I2C data from this module to an
 *  external I2C bus controller (this is the scenario of an I2C bus read).
 *
 *  @param      none
 *
 *  @return    none
 *
 */
extern void at24_i2c_tx_callback(void);

#endif /* COMMUNICATION_AT24_EMULATION_H_ */
