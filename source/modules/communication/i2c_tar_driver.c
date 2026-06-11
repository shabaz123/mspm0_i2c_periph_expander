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
 *  @file       i2c_tar_driver.c
 *
 *  I2C target mode application interface driver
 *
 ******************************************************************************/

#include "communication/i2c_tar_driver.h"

/**
 * Function prototypes for local helper functions used by the driver internally.
 */

__STATIC_INLINE void I2CTarDriver_resetRXBuf(i2c_tar_driver_t *pHandle);

__STATIC_INLINE void I2CTarDriver_resetTXBuf(i2c_tar_driver_t *pHandle);

__STATIC_INLINE void I2CTarDriver_handleStartIRQ(i2c_tar_driver_t *pHandle);

__STATIC_INLINE void I2CTarDriver_handleStopIRQ(i2c_tar_driver_t *pHandle);

__STATIC_INLINE void I2CTarDriver_handleRxIRQ(i2c_tar_driver_t *pHandle);

__STATIC_INLINE void I2CTarDriver_handleTxIRQ(i2c_tar_driver_t *pHandle);

__STATIC_INLINE void I2CTarDriver_handleTxEmptyIRQ(i2c_tar_driver_t *pHandle);

/**
 * Open/close, read/write, get state function implementations
 */

void I2CTarDriver_open(i2c_tar_driver_t *pHandle)
{
    pHandle->state = I2C_TAR_DRV_STATE__IDLE;

    /* Clearing the SWUEN in SCTR prevents I2C SCL/SDA low lockup in the event
     * that the I2C bus is held low by another device for an extended period
     * of time.  This does mean that this module does not support I2C wake-up
     * from low power modes in certain circumstances.  The peripheral functional
     * clock must be greater than or equal to 20X the SCL frequency for correct
     * operation of the I2C module. */
    pHandle->pModule->SLAVE.SCTR &= ~I2C_SCTR_SWUEN_MASK;

    DL_I2C_enableTarget(pHandle->pModule);
    DL_I2C_enableInterrupt(pHandle->pModule, \
        DL_I2C_INTERRUPT_TARGET_START |\
        DL_I2C_INTERRUPT_TARGET_STOP);
}

void I2CTarDriver_close(i2c_tar_driver_t *pHandle)
{
    DL_I2C_disableTarget(pHandle->pModule);
    DL_I2C_disableInterrupt(pHandle->pModule, \
        DL_I2C_INTERRUPT_TARGET_START |\
        DL_I2C_INTERRUPT_TARGET_STOP |\
        DL_I2C_INTERRUPT_TARGET_RXFIFO_TRIGGER |\
        DL_I2C_INTERRUPT_TARGET_TXFIFO_TRIGGER |\
        DL_I2C_INTERRUPT_TARGET_TXFIFO_EMPTY);
    DL_I2C_clearInterruptStatus(pHandle->pModule, \
        DL_I2C_INTERRUPT_TARGET_RXFIFO_TRIGGER);
}

uint8_t I2CTarDriver_read(i2c_tar_driver_t *pHandle)
{
    uint8_t data;

    if (pHandle->rxBufPopIdx < pHandle->rxBufPushIdx)
    {
        data = pHandle->pRxBuf[pHandle->rxBufPopIdx++];
    }
    else
    {
        data = 0x00;
    }

    return data;
}

void I2CTarDriver_write(i2c_tar_driver_t *pHandle, uint8_t data)
{
    if (pHandle->txBufPushIdx < pHandle->txBufLen)
    {
        pHandle->pTxBuf[pHandle->txBufPushIdx++] = data;
    }
}

/**
 * Internal helper function implementations
 */

__STATIC_INLINE void I2CTarDriver_resetRXBuf(i2c_tar_driver_t *pHandle)
{
    pHandle->rxBufPushIdx = 0;
    pHandle->rxBufPopIdx = 0;
}

__STATIC_INLINE void I2CTarDriver_resetTXBuf(i2c_tar_driver_t *pHandle)
{
    pHandle->txBufPushIdx = 0;
    pHandle->txBufPopIdx = 0;
}

__STATIC_INLINE void I2CTarDriver_handleStartIRQ(i2c_tar_driver_t *pHandle)
{
    /* Handle special case of a I2C bus re-start condition, where
     * we need to wrap up the end of a RX or TX process and clean up. */
    if (pHandle->state == I2C_TAR_DRV_STATE__RX)
    {
        /* Here we will tell the application callback that this
         * was a re-start triggered callback, meaning that we are
         * not going to pause the I2C peripheral for callback processing
         * as we are still in the middle of an I2C frame. */
        if (pHandle->trxToSecAddr == false)
        {
            if (pHandle->rxCallback != I2C_TAR_DRIVER_NO_CALLBACK)
            {
                pHandle->rxCallback(pHandle->rxBufPushIdx, \
                    I2C_TAR_DRV_CALL_TRIG__RESTART);
            }
        }
        else
        {
            if (pHandle->rxCallback2 != I2C_TAR_DRIVER_NO_CALLBACK)
            {
                pHandle->rxCallback2(pHandle->rxBufPushIdx, \
                    I2C_TAR_DRV_CALL_TRIG__RESTART);
            }
        }
    }
    else if (pHandle->state == I2C_TAR_DRV_STATE__TX)
    {
        DL_I2C_disableInterrupt(pHandle->pModule, \
            DL_I2C_INTERRUPT_TARGET_TXFIFO_TRIGGER);
        DL_I2C_flushTargetTXFIFO(pHandle->pModule);
    }

    /* Proceed with what is needed for a normal I2C bus START
     * condition or a RESTART condition.  We don't know yet if
     * the transaction is RX or TX, so we enable the IRQ's needed
     * for handling both scenarios and put the state to START. */
    if ((DL_I2C_getTargetStatus(pHandle->pModule) \
            & DL_I2C_TARGET_STATUS_OWN_ADDR_ALTERNATE_MATCHED) == 0)
    {
        pHandle->trxToSecAddr = false;
    }
    else
    {
        pHandle->trxToSecAddr = true;
    }

    DL_I2C_enableInterrupt(pHandle->pModule, \
        DL_I2C_INTERRUPT_TARGET_RXFIFO_TRIGGER |\
        DL_I2C_INTERRUPT_TARGET_TXFIFO_EMPTY);
    pHandle->state = I2C_TAR_DRV_STATE__START;
}

__STATIC_INLINE void I2CTarDriver_handleStopIRQ(i2c_tar_driver_t *pHandle)
{
    /* On a stop condition, we need to wrap up the end of the RX or TX
     * operation and clean up. */
    if (pHandle->state == I2C_TAR_DRV_STATE__RX)
    {
        /* If we are finishing a receive operation, as this is a STOP
         * condition and we are done with the frame, we will in fact
         * disable the I2C hardware such that further attempts to
         * communicate by a bus controller to this address will be NAK'ed
         * until we complete the receive callback processing.  This enables
         * safer handling of the received data as we don't allow another
         * transaction to start overlapping the completion of processing the
         * previous transaction.  I2C bus controllers need to be able to wait
         * for the max time needed for processing before starting a
         * communication again, or they need to know to re-try again if
         * they get a NAK. */
        DL_I2C_disableTarget(pHandle->pModule);
        if (pHandle->trxToSecAddr == false)
        {
            if (pHandle->rxCallback != I2C_TAR_DRIVER_NO_CALLBACK)
            {
                pHandle->rxCallback(pHandle->rxBufPushIdx, \
                    I2C_TAR_DRV_CALL_TRIG__STOP);
            }
        }
        else
        {
            if (pHandle->rxCallback2 != I2C_TAR_DRIVER_NO_CALLBACK)
            {
                pHandle->rxCallback2(pHandle->rxBufPushIdx, \
                    I2C_TAR_DRV_CALL_TRIG__STOP);
            }
        }
        DL_I2C_enableTarget(pHandle->pModule);
    }
    else if (pHandle->state == I2C_TAR_DRV_STATE__TX)
    {
        /* If we are finishing a transmit operation, we clean out the hardware
         * transmit FIFO to make sure we don't leave stale data behind for the
         * next transaction! */
        DL_I2C_flushTargetTXFIFO(pHandle->pModule);
    }

    /* We always turn off RX/TX related interrupts at the end of an I2C frame.
     * The next start condition will enable the needed IRQ's again. */
    DL_I2C_disableInterrupt(pHandle->pModule, \
        DL_I2C_INTERRUPT_TARGET_RXFIFO_TRIGGER |\
        DL_I2C_INTERRUPT_TARGET_TXFIFO_EMPTY |\
        DL_I2C_INTERRUPT_TARGET_TXFIFO_TRIGGER);
    pHandle->state = I2C_TAR_DRV_STATE__IDLE;
}

__STATIC_INLINE void I2CTarDriver_handleRxIRQ(i2c_tar_driver_t *pHandle)
{
    uint8_t data;

    if (pHandle->state == I2C_TAR_DRV_STATE__START)
    {
        /* Handle the special case where this is a new receive after a START
         * or RESTART condition.  In these cases, we reset the receive buffer
         * push and pop indices as we are starting from the beginning of
         * the buffer with fresh data. */
        I2CTarDriver_resetRXBuf(pHandle);
        pHandle->state = I2C_TAR_DRV_STATE__RX;
    }

    /* Read data from I2C hardware RX FIFO into RX queue until I2C hardware RX
     * FIFO is empty or RX queue is full. Reading to empty on the HW FIFO
     * ensures that the RX FIFO trigger interrupt will be set the next time. */
    while ((pHandle->rxBufPushIdx < pHandle->rxBufLen) &&\
        (DL_I2C_isTargetRXFIFOEmpty(pHandle->pModule) == false))
    {
        data = DL_I2C_receiveTargetData(pHandle->pModule);
        pHandle->pRxBuf[pHandle->rxBufPushIdx++] = data;
    }
}

__STATIC_INLINE void I2CTarDriver_handleTxIRQ(i2c_tar_driver_t *pHandle)
{
    uint8_t data;

    /* Write data from driver TX queue into I2C hardware TX FIFO until
     * there is no data left in TX queue or the TX FIFO is full */
    while ((pHandle->txBufPopIdx < pHandle->txBufPushIdx) &&\
            (DL_I2C_isControllerTXFIFOFull(pHandle->pModule) == false))
    {
        data = pHandle->pTxBuf[pHandle->txBufPopIdx++];
        if (pHandle->txBufPopIdx == pHandle->txBufPushIdx)
        {
            DL_I2C_disableInterrupt(pHandle->pModule, \
                DL_I2C_INTERRUPT_TARGET_TXFIFO_TRIGGER);
        }
        DL_I2C_transmitTargetData(pHandle->pModule, data);
    }
}

__STATIC_INLINE void I2CTarDriver_handleTxEmptyIRQ(i2c_tar_driver_t *pHandle)
{
    /* Handle the case where an I2C bus controller is attempting
     * to read from us.  We need to fetch the data from the application,
     * so we will reset the TX buffer and call the application callback to get
     * the data.  Meanwhile the HW is stretching the clock to hold the bus. */
    I2CTarDriver_resetTXBuf(pHandle);
    if (pHandle->trxToSecAddr == false)
    {
        if (pHandle->txCallback != I2C_TAR_DRIVER_NO_CALLBACK)
        {
            pHandle->txCallback();
        }
        else
        {
            /* In the event that the 1st I2C target address has a transmit
             * request and there is no callback registered, we will simply
             * send back dummy 0xFF's to not block the bus.
             */
            I2CTarDriver_write(pHandle, 0xFF);
        }
    }
    else
    {
        if (pHandle->txCallback2 != I2C_TAR_DRIVER_NO_CALLBACK)
        {
            pHandle->txCallback2();
        }
        else
        {
            /* In the event that the 2nd I2C target address has a transmit
             * request and there is no callback registered, we will simply
             * send back dummy 0xFF's to not block the bus.
             */
            I2CTarDriver_write(pHandle, 0xFF);
        }
    }
    pHandle->state = I2C_TAR_DRV_STATE__TX;
    DL_I2C_enableInterrupt(pHandle->pModule, \
        DL_I2C_INTERRUPT_TARGET_TXFIFO_TRIGGER);
}

/**
 * Interrupt service routine function implementation
 */

void I2CTarDriver_ISR(i2c_tar_driver_t *pHandle)
{
    switch (DL_I2C_getPendingInterrupt(pHandle->pModule))
    {
        case DL_I2C_IIDX_TARGET_START:
            I2CTarDriver_handleStartIRQ(pHandle);
            break;
        case DL_I2C_IIDX_TARGET_STOP:
            I2CTarDriver_handleStopIRQ(pHandle);
            break;
        case DL_I2C_IIDX_TARGET_RXFIFO_TRIGGER:
            I2CTarDriver_handleRxIRQ(pHandle);
            break;
        case DL_I2C_IIDX_TARGET_TXFIFO_TRIGGER:
            I2CTarDriver_handleTxIRQ(pHandle);
            break;
        case DL_I2C_IIDX_TARGET_TXFIFO_EMPTY:
            I2CTarDriver_handleTxEmptyIRQ(pHandle);
            break;
        default:
            break;
    }
}
