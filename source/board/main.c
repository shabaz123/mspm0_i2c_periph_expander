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
 *  @file       main.c
 *  @brief      Main routine for Processor Companion Utility
 *
 *  This file is the MAIN routine for the PCU (processor companion utility)
 *  project.  The PCU is a project for MSPM0 microcontrollers which is
 *  intended to enable several board utility functions for a host microprocessor
 *  such as a high performance Cortex-A class MPU.  Specifically, this project
 *  enables MSPM0 MCUs such as the MSPM0L1105 MCU to provide services such as
 *  EEPROM emulation via an I2C interface to a host processor, together with
 *  other functionality which may be expanded as desired to serve a variety
 *  of application use cases.
 *
 *  Target Device: MSPM0L1105TRGER (32MHz CM0+, 32kB flash, 4kB SRAM, VQFN-24)
 *  Device Configuration: SysConfig managed
 *
 *  The utilities which are currently enabled within this project include:
 *      (1) Emulated I2C EEPROM with 4kB (32kbit) data storage capability and
 *          command compatibility with AT24x style devices (32-byte page writes)
 *      (2) Emulated I2C ADC with 8 channels, 12-bit resolution, supply voltage
 *          reference, compatible with Linux IIO AD7291 device driver
 *
 ******************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include "system.h"

int main(void)
{
    /* Initialize modules, IO, and NVIC */
    System_init();

    /* Enter the application loop */
    while (1)
    {
        if (System_getPendingEvents() & SYSEVENT__WAKE_TIMER)
        {
            System_clearPendingEvents(SYSEVENT__WAKE_TIMER);
            DL_WWDT_restart(WWDT0_INST);
        }

        System_sleepUntilInterrupt();
    }

    return 0;
}

