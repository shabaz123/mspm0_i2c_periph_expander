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
 *  @file       at24_emulation.c
 *
 *  AT24x I2C EEPROM Emulation Library
 *
 ******************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include <ti/devices/msp/msp.h>
#include "at24_emulation.h"

/**
 * @brief   Item data union to enable easy access at 32-bit word level or
 *          at the 8-bit byte level for one Type-B EEPROM data item.
 */
typedef union
{
    uint32_t u32;
    uint8_t u8[4];
} at24_eeprom_emu_item_data;

/**
 * @brief   Item data union to enable easy access at 32-bit word level or
 *          at the 8-bit byte level for 8 Type-B EEPROM data items in a page.
 */
typedef union
{
    uint32_t u32[AT24_EMU_PAGE_TOT_ITEMS];
    uint8_t u8[AT24_EMU_PAGE_TOT_BYTES];
} at24_eeprom_emu_item_pdata;

/**
 * @brief   I2C EEPROM emulation page buffer
 */
at24_eeprom_emu_item_pdata at24_pagebuf;

/**
 * @brief   I2C EEPROM emulation EEPROM address space RW head pointer
 */
uint16_t at24_rw_addr;

/**
 * @brief   I2C target driver to use for read/write operations
 *          when handling callbacks
 */
i2c_tar_driver_t *at24_i2c_tar_driver_handle;

/**
 *  @brief      Get the item at itemIndex from the EEPROM area
 *
 *  @param      itemIndex is index in EEPROM memory for the desired item
 *
 *  @return     The item value (if it was written previously, else
 *              the default unwritten value if it was not previously written
 *
 */
__STATIC_INLINE uint32_t at24_eeprom_emu_read(uint16_t itemIndex);

/**
 *  @brief      Write data to the item at itemIndex in the EEPROM area
 *
 *  @param      itemIndex is index in EEPROM memory for the desired item
 *
 *  @param      data is the data to write to EEPROM memory at itemIndex
 *
 *  @return     none
 *
 */
__STATIC_INLINE void at24_eeprom_emu_write(uint16_t itemIndex, uint32_t data);

/**
 *  @brief      Handle Type-B EEPROM emulation garbage collection
 *
 *  This function needs to be called after writes to emulated EEPROM.
 *  It is responsible for doing a flash sector erase on sectors which
 *  contain old data and need to be freed to take new data.  Note
 *  that this function will operate on the embedded flash, and so we
 *  need to be careful about what else we are doing at the same time.
 *  To ensure safety, global interrupts will be BLOCKED during any
 *  flash sector erase operations, which may impact other application
 *  activity that is depending on timely interrupts.  Take care.
 *
 *  @param      none
 *
 *  @return    none
 *
 */
static void at24_garbageCollection(void);

/**
 * Standard function implementations
 */

uint32_t at24_open(i2c_tar_driver_t *pI2CTarDriverInst)
{
    uint32_t eepromState;

    at24_i2c_tar_driver_handle = pI2CTarDriverInst;

    __disable_irq();
    eepromState = EEPROM_TypeB_init();
    __enable_irq();

    if (eepromState == EEPROM_EMULATION_INIT_ERROR ||
            eepromState == EEPROM_EMULATION_TRANSFER_ERROR)
    {
        return eepromState;
    }

    at24_rw_addr = 0x0000;

    return 0;
}

void at24_garbageCollection(void)
{
    if (gEEPROMTypeBEraseFlag == 1)
    {
        __disable_irq();
        EEPROM_TypeB_eraseGroup();
        gEEPROMTypeBEraseFlag = 0;
        __enable_irq();
    }
}

/**
 * Helper functions for interfacing with the TI Type-B EEPROM emulation library
 * on an EEPROM address index basis.
 */

__STATIC_INLINE uint32_t at24_eeprom_emu_read(uint16_t itemIndex)
{
    uint32_t eepromData;

    eepromData = EEPROM_TypeB_readDataItem(itemIndex);
    if (gEEPROMTypeBSearchFlag == 0)
    {
        eepromData = AT24_EMU_MEM_BLANK;
    }
    return eepromData;
}

__STATIC_INLINE void at24_eeprom_emu_write(uint16_t itemIndex, uint32_t data)
{
    EEPROM_TypeB_write(itemIndex, data);
}

/**
 * I2C target driver callback function implementations
 */

void at24_i2c_rx_callback(uint32_t bytes, i2c_tar_driver_call_trig_t trig)
{
    at24_eeprom_emu_item_data itemData;
    uint16_t itemIndex;
    uint16_t itemOffset;
    uint16_t byteOffset;
    uint16_t activeItemIndex;

    /* If insufficient address data is loaded, bail out
    * and don't update the R/W byte address pointer. */
    if (bytes < 2)
    {
        return;
    }

    /* Update the R/W byte address with the first two bytes received.
     * Ensure the address is bound within 0 to AT24_EMU_TOT_BYTES-1 by
     * rolling over any inputs to be within this range. */
    at24_rw_addr = (uint16_t)(I2CTarDriver_read( \
        at24_i2c_tar_driver_handle)) << 8;
    at24_rw_addr = at24_rw_addr | (uint16_t)(I2CTarDriver_read( \
        at24_i2c_tar_driver_handle));
    at24_rw_addr %= AT24_EMU_TOT_BYTES;
    bytes -= 2;

    /* If there is no data and this is just an address reception, then we are
     * done.  Likewise, even if there is data, if this is an I2C RESTART trigger
     * we will not allow writes in that case as we don't want to be running
     * flash operations in the middle of an ongoing I2C frame. */
    if ((bytes == 0) || (trig == I2C_TAR_DRV_CALL_TRIG__RESTART))
    {
        return;
    }

    /* Otherwise, take the data written via I2C and store it into emulated
     * EEPROM using the EEPROM write helper function. We will also
     * increment the RW address accordingly and handle the wrap-around case. */
    if (bytes == 1)
    {
        /* If this is a byte write only, we can do a read-modify-write
        * on just one 4-byte EEPROM data item, and increment the R/W
        * byte address for any subsequent operation. */
        itemIndex = at24_rw_addr >> 2;
        itemOffset = at24_rw_addr % 4;
        itemData.u32 = at24_eeprom_emu_read(itemIndex);
        itemData.u8[itemOffset] = I2CTarDriver_read(at24_i2c_tar_driver_handle);
        __disable_irq();
        at24_eeprom_emu_write(itemIndex, itemData.u32);
        __enable_irq();
        at24_rw_addr = (at24_rw_addr + 1) % AT24_EMU_TOT_BYTES;
    }

    /* If this is more than a byte write, we have more work to do.
     * The at24_writebuf array will be used as a RAM copy of the
     * active page, which will be read out, updated, and any
     * changed EEPROM data items will be written back to flash. */
    else if (bytes > 1)
    {
        /* Read in active page to SRAM page buffer by finding item index and
         * then aligning to start of page boundary */
        itemIndex = at24_rw_addr >> 2;
        itemIndex &= 0xFFF8;
        for (activeItemIndex=0; activeItemIndex<AT24_EMU_PAGE_TOT_ITEMS; \
            activeItemIndex++)
        {
            at24_pagebuf.u32[activeItemIndex] = at24_eeprom_emu_read(itemIndex);
            itemIndex++;
        }

        /* Read new data from I2C target driver and write to SRAM page buffer */
        byteOffset = at24_rw_addr % AT24_EMU_PAGE_TOT_BYTES;
        while(bytes--)
        {
            at24_pagebuf.u8[byteOffset] = I2CTarDriver_read( \
                at24_i2c_tar_driver_handle);
            byteOffset = (byteOffset + 1) % AT24_EMU_PAGE_TOT_BYTES;
        }

        /* Write back SRAM page buffer into EEPROM memory for any EEPROM
         * emulation items which have changed values */
        itemIndex = at24_rw_addr >> 2;
        itemIndex &= 0xFFF8;
        for (activeItemIndex=0; activeItemIndex<AT24_EMU_PAGE_TOT_ITEMS; \
            activeItemIndex++)
        {
            if (at24_pagebuf.u32[activeItemIndex] != at24_eeprom_emu_read( \
                itemIndex))
            {
                /* Disable interrupts globally during flash operation
                 * for safety */
                __disable_irq();
                at24_eeprom_emu_write(itemIndex, \
                    at24_pagebuf.u32[activeItemIndex]);
                __enable_irq();
            }
            itemIndex++;
        }

        /* Update RW address to last written byte address in EEPROM space */
        itemIndex = at24_rw_addr >> 2;
        itemIndex &= 0xFFF8;
        at24_rw_addr = (itemIndex * AT24_EMU_ITEM_BYTES) + byteOffset;
    }

    /* Run garbage collection check in case we need to free flash sectors */
    at24_garbageCollection();
}

void at24_i2c_tx_callback(void)
{
    uint16_t itemIndex;
    uint16_t itemOffset;
    uint16_t bytes;

    /* Double check that our RW address pointer (in EEPROM space)
     * is within the EEPROM total area.  Roll it over to EEPROM space if not.
     */
    at24_rw_addr %= AT24_EMU_TOT_BYTES;

    /* Get the item containing the first byte to transmit. */
    itemIndex = at24_rw_addr >> 2;
    itemOffset = at24_rw_addr % 4;
    at24_pagebuf.u32[0] = at24_eeprom_emu_read(itemIndex);

    /* If the first byte is not aligned to an item boundary, get the next byte
     * so that we can send 4 sequential byte starting from the request */
    if (itemOffset != 0)
    {
        itemIndex++;
        if (itemIndex >= AT24_EMU_TOT_ITEMS)
        {
            itemIndex = 0;
        }
        at24_pagebuf.u32[1] = at24_eeprom_emu_read(itemIndex);
    }

    /* Write the bytes to the I2C target driver for transmission */
    bytes = 4;
    while (bytes--)
    {
        I2CTarDriver_write(at24_i2c_tar_driver_handle, \
            at24_pagebuf.u8[itemOffset++]);
    }

    /* Increment the RW pointer in EEPROM space and catch any rollover */
    at24_rw_addr += 4;
    at24_rw_addr %= AT24_EMU_TOT_BYTES;
}
