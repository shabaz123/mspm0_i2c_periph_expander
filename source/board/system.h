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
 *  @file       system.h
 *  @brief      System management and ISR's for Processor Companion Utility
 *
 *  This file contains the interrupt handlers and system
 *  management routines used by the application.
 *
 ******************************************************************************/

#ifndef SYSTEM_H_
#define SYSTEM_H_

#include <stdint.h>
#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_core.h>
#include "ti_msp_dl_config.h"

// I2C base addresses
#define BASE_PRI_ADDR 0x48
#define BASE_SEC_ADDR 0x20

/*!
 * @brief No system events are pending.
 */
#define SYSEVENT__NONE_PENDING 0x00000000

/*!
 * @brief System wake timer event pending flag.
 */
#define SYSEVENT__WAKE_TIMER 0x00000001

/*!
 * @brief Variable for pending events needing application response.
 *
 * Interrupts requiring delay of LPM entry set bits in this variable.  The
 * application is responsible to check and clear them and handle the events.
 */
extern volatile uint32_t System_pendingEvents;
extern uint8_t addrSel;

/**
 *  @brief      Initialize system variables and enable NVIC interrupts
 *  @param      none
 *  @return     none
 */
extern void System_init(void);

/**
 *  @brief      Safely sleep the processor until an interrupt arrives
 *  @param      none
 *  @return     none
 */
extern void System_sleepUntilInterrupt(void);

/**
 *  @brief      Get the current bit field of pending system events
 *  @param      none
 *  @return     Bit field of pending events
 */
__STATIC_INLINE uint32_t System_getPendingEvents(void) \
{
    return System_pendingEvents;
}

/**
 *  @brief      Clear specified events in the pending event variable
 *  @param      events is a mask of the events to clear
 *  @return     none
 */
__STATIC_INLINE void System_clearPendingEvents(uint32_t events) \
{
    System_pendingEvents &= ~events;
}

#endif /* SYSTEM_H_ */
