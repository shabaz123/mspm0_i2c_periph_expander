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
 *  @file       i2c_tar_driver.h
 *  @brief      I2C target mode application interface driver
 *  @defgroup   I2C Target Driver
 *
 *  The I2C target driver implements a simple application interface for I2C
 *  target communication via the to the underlying MSPM0 I2C DriverLib
 *  hardware abstraction and by extension the MSPM0 I2C peripheral in target
 *  operating mode.
 *
 *  This application interface abstracts all I2C interrupt management
 *  and I2C state management for the application.  The application
 *  only needs to provide definitions for transmit and receive callback
 *  functions (to handle I2C bus read and write commands, respectively),
 *  with those callback functions calling the simple read/write function
 *  calls provided by this library to get to and from the application.
 *
 *  This interface is designed to support multiple instances of I2C target
 *  peripherals at the same time without adding code size.  All functions
 *  provided operate with respect to a i2c_tar_driver_t data structure
 *  which indicates the corresponding peripheral, buffers, and state.
 *  More than one i2c_tar_driver_t data structure may be set up in
 *  an application with each one being linked to a unique I2C peripheral
 *  in the device itself.  Each peripheral's interrupt handler in the
 *  application must call I2CTarDriver_ISR with the corresponding
 *  i2c_tar_driver_t pointer passed so that the correct driver instance
 *  is handled.
 *
 *  This interface supports responding to two I2C target addresses, with
 *  each address mapping to its own set of transmit and receive callback
 *  functions, respectively.  If callback functions for transmit are not
 *  mapped for a given address, 0xFF is sent by default.
 *
 *  This interface uses fully interrupt driven I/O.  It does not use DMA.
 *
 ******************************************************************************/

#ifndef COMMUNICATION_I2C_TAR_DRIVER_H_
#define COMMUNICATION_I2C_TAR_DRIVER_H_

#include <stdint.h>
#include <stdbool.h>
#include <ti/driverlib/dl_i2c.h>

/**
 * @brief  Value representing no callback function assigned
 */
#define I2C_TAR_DRIVER_NO_CALLBACK 0

/**
 * @brief  State definition enum for the I2C target driver.
 */
typedef enum
{
    /*! I2C driver is in the idle state. */
    I2C_TAR_DRV_STATE__IDLE,
    /*! I2C driver received a START, but no RX/TX has occurred yet. */
    I2C_TAR_DRV_STATE__START,
    /*! I2C driver is in a transmit state (I2C bus read) */
    I2C_TAR_DRV_STATE__TX,
    /*! I2C driver is in the receive state. (I2C bus write) */
    I2C_TAR_DRV_STATE__RX
} i2c_tar_driver_state_t;

/**
 * @brief   Enum used to indicate if the callback is being called
 *          after an I2C bus RESTART condition or STOP condition.
 */
typedef enum
{
    /*! I2C driver callback was triggered by a RESTART condition on the bus */
    I2C_TAR_DRV_CALL_TRIG__RESTART,
    /*! I2C driver callback was triggered by a STOP condition on the bus */
    I2C_TAR_DRV_CALL_TRIG__STOP
} i2c_tar_driver_call_trig_t;

/**
 * @brief  Configuration struct for the I2C target driver.
 */
typedef struct
{
    /*! Pointer to the I2C peripheral memory base address
     * (must be set by the calling application) */
    I2C_Regs *pModule;
    /*! The state variable for the driver */
    i2c_tar_driver_state_t state;
    /*! Pointer to the RX buffer base address
     * (must be set by the calling application) */
    uint8_t *pRxBuf;
    /*! Length of the RX buffer pointed to by pRxBuf
     * (must be set by the calling application) */
    uint32_t rxBufLen;
    /*! Current push index for the RX buffer */
    uint32_t rxBufPushIdx;
    /*! Current pop index for the RX buffer */
    uint32_t rxBufPopIdx;
    /*! Pointer to the TX buffer base address
     * (must be set by the calling application) */
    uint8_t *pTxBuf;
    /*! Length of the TX buffer pointed to by pTxBuf
     * (must be set by the calling application) */
    uint32_t txBufLen;
    /*! Current push index for the TX buffer */
    uint32_t txBufPushIdx;
    /*! Current pop index for the TX buffer */
    uint32_t txBufPopIdx;
    /*! Function pointer to the RX callback in the
     * application layer above this driver.  Set to I2C_TAR_DRIVER_NO_CALLBACK
     * if the first address is not used in the application
     * (must be set by the calling application) */
    void (*rxCallback)(uint32_t bytes, i2c_tar_driver_call_trig_t trig);
    /*! Function pointer to the TX callback in the
     * application layer above this driver.  Set to I2C_TAR_DRIVER_NO_CALLBACK
     * if the first address is not used in the application
     * (must be set by the calling application) */
    void (*txCallback)(void);
    /*! Function pointer to the 2nd RX callback in the
     * application layer above this driver.  This supports
     * the second I2C address, and is optional.  Set to
     * I2C_TAR_DRIVER_NO_CALLBACK if the second address is not used in the
     * application (must be set by the calling application) */
    void (*rxCallback2)(uint32_t bytes, i2c_tar_driver_call_trig_t trig);
    /*! Function pointer to the 2nd TX callback in the
     * application layer above this driver.  This supports
     * the second I2C address, and is optional.  Set to
     * I2C_TAR_DRIVER_NO_CALLBACK if the second address is not used in the
     * application (must be set by the calling application) */
    void (*txCallback2)(void);
    /*!  */
    bool trxToSecAddr;
} i2c_tar_driver_t;

/**
 *  @brief      Open the I2C target driver.
 *
 *  This function opens the I2C target driver based on the parameters
 *  set up in the i2c_tar_driver_t structure passed in.
 *  Once open, the driver is ready for I2C read/write commands from a
 *  an I2C controller on the I2C bus.
 *
 *  This function expects that the IOMUX was configured to connect
 *  the desired pins to the  I2C peripheral module in advance.
 *
 *  This function expects that the I2C peripheral was pre-initialized
 *  by the application but left in a disabled state.
 *
 *  @param[in]  pHandle Pointer to the I2C target driver data structure
 *                      which must be pre-configured by the application
 *
 *  @return     none
 *
 */
extern void I2CTarDriver_open(i2c_tar_driver_t *pHandle);

/**
 *  @brief      Closes the I2C target driver.
 *
 *  Disables all interrupts used by the I2C target driver
 *  and disables the I2C peripheral itself.  Note that the
 *  state variable is not valid when the driver is closed.
 *
 *  @param[in]  pHandle Pointer to the I2C target driver data structure
 *
 *  @return     none
 *
 */
extern void I2CTarDriver_close(i2c_tar_driver_t *pHandle);

/**
 *  @brief      Get the next data byte from the receive queue.
 *
 *  This function gets the next byte in the receive queue.  This function
 *  is expected to be called by the receive callback function implemented
 *  in the application layer which is pointed to by rxCallback in the
 *  driver's data structure.
 *
 *  @param[in]  pHandle Pointer to the I2C target driver data structure
 *
 *  @return     The next byte in the receive queue (if data present in
 *              the receive queue), else returns zero if the receive
 *              queue is now empty
 *
 */
extern uint8_t I2CTarDriver_read(i2c_tar_driver_t *pHandle);

/**
 *  @brief      Put the next data byte into the transmit queue.
 *
 *  This function puts the provided byte into the driver's transmit queue
 *  for transmission during I2C bus read commands (when an I2C bus controller
 *  is reading from this device).  If the transmit queue is full, the data
 *  provided is dropped and not added to the transmit queue.
 *
 *  @param[in]  pHandle Pointer to the I2C target driver data structure
 *
 *  @param[in]  data is the byte to add to the transmit queue
 *
 *  @return     none
 *
 */
extern void I2CTarDriver_write(i2c_tar_driver_t *pHandle, uint8_t data);

/**
 *  @brief      Get the state of the driver.
 *
 *  This function gets the current state of the I2C driver.  The value
 *  returned is only valid if I2CTarDriver_open() has been called
 *  and I2CTarDriver_close() has not been called since I2CTarDriver_open()
 *  was called.  Once I2CTarDriver_close() is called, the state is not
 *  maintained until I2CTarDriver_open() is again called to open the driver.
 *
 *  @param[in]  pHandle Pointer to the I2C target driver data structure
 *
 *  @return     A i2c_tar_driver_state_t enum indicating the current state.
 *
 */
__STATIC_INLINE i2c_tar_driver_state_t I2CTarDriver_getState( \
    i2c_tar_driver_t *pHandle)
{
    return pHandle->state;
}

/**
 *  @brief      The interrupt service routine for the I2C peripheral.
 *
 *  This function must be called by the function which directly handles
 *  the peripheral interrupt.
 *
 *  @param[in]  pHandle Pointer to the I2C target driver data structure
 *
 *  @return     none
 *
 */
extern void I2CTarDriver_ISR(i2c_tar_driver_t *pHandle);

#endif /* COMMUNICATION_I2C_TAR_DRIVER_H_ */
