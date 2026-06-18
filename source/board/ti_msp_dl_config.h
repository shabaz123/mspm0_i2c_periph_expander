/*
 * Copyright (c) 2023, Texas Instruments Incorporated - http://www.ti.com
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

/*
 *  ============ ti_msp_dl_config.h =============
 *  Configured MSPM0 DriverLib module declarations
 *
 *  DO NOT EDIT - This file is generated for the MSPM0L110X
 *  by the SysConfig tool.
 */
#ifndef ti_msp_dl_config_h
#define ti_msp_dl_config_h

#define CONFIG_MSPM0L110X
#define CONFIG_MSPM0L1105

#if defined(__ti_version__) || defined(__TI_COMPILER_VERSION__)
#define SYSCONFIG_WEAK __attribute__((weak))
#elif defined(__IAR_SYSTEMS_ICC__)
#define SYSCONFIG_WEAK __weak
#elif defined(__GNUC__)
#define SYSCONFIG_WEAK __attribute__((weak))
#endif

#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_core.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  ======== SYSCFG_DL_init ========
 *  Perform all required MSP DL initialization
 *
 *  This function should be called once at a point before any use of
 *  MSP DL.
 */


/* clang-format off */

#define POWER_STARTUP_DELAY                                                (16)


#define GPIO_ROSC_PORT                                                     GPIOA
#define GPIO_ROSC_PIN                                              DL_GPIO_PIN_2
#define GPIO_ROSC_IOMUX                                           (IOMUX_PINCM3)
#define CPUCLK_FREQ                                                     32000000



/* Defines for CAPTURE_0 */
#define CAPTURE_0_INST                                                   (TIMG4)
#define CAPTURE_0_INST_IRQHandler                               TIMG4_IRQHandler
#define CAPTURE_0_INST_INT_IRQN                                 (TIMG4_INT_IRQn)
#define CAPTURE_0_INST_LOAD_VALUE                                           (0U)
/* GPIO defines for channel 0 */
#define GPIO_CAPTURE_0_C0_PORT                                             GPIOA
#define GPIO_CAPTURE_0_C0_PIN                                     DL_GPIO_PIN_10
#define GPIO_CAPTURE_0_C0_IOMUX                                  (IOMUX_PINCM11)
#define GPIO_CAPTURE_0_C0_IOMUX_FUNC                 IOMUX_PINCM11_PF_TIMG4_CCP0





/* Defines for WAKEUP_TIMER */
#define WAKEUP_TIMER_INST                                                (TIMG0)
#define WAKEUP_TIMER_INST_IRQHandler                            TIMG0_IRQHandler
#define WAKEUP_TIMER_INST_INT_IRQN                              (TIMG0_INT_IRQn)
#define WAKEUP_TIMER_INST_LOAD_VALUE                                    (16383U)
/* Defines for TIMER_ACMEAS */
#define TIMER_ACMEAS_INST                                                (TIMG1)
#define TIMER_ACMEAS_INST_IRQHandler                            TIMG1_IRQHandler
#define TIMER_ACMEAS_INST_INT_IRQN                              (TIMG1_INT_IRQn)
#define TIMER_ACMEAS_INST_LOAD_VALUE                                      (999U)
#define TIMER_ACMEAS_INST_PUB_0_CH                                           (1)




/* Defines for TAR_I2C */
#define TAR_I2C_INST                                                        I2C0
#define TAR_I2C_INST_IRQHandler                                  I2C0_IRQHandler
#define TAR_I2C_INST_INT_IRQN                                      I2C0_INT_IRQn
#define TAR_I2C_TARGET_OWN_ADDR                                             0x48
#define TAR_I2C_TARGET_SEC_OWN_ADDR                                         0x20
#define TAR_I2C_TARGET_SEC_OWN_ADDR_MASK                                     0x0
#define GPIO_TAR_I2C_SDA_PORT                                              GPIOA
#define GPIO_TAR_I2C_SDA_PIN                                       DL_GPIO_PIN_0
#define GPIO_TAR_I2C_IOMUX_SDA                                    (IOMUX_PINCM1)
#define GPIO_TAR_I2C_IOMUX_SDA_FUNC                     IOMUX_PINCM1_PF_I2C0_SDA
#define GPIO_TAR_I2C_SCL_PORT                                              GPIOA
#define GPIO_TAR_I2C_SCL_PIN                                       DL_GPIO_PIN_1
#define GPIO_TAR_I2C_IOMUX_SCL                                    (IOMUX_PINCM2)
#define GPIO_TAR_I2C_IOMUX_SCL_FUNC                     IOMUX_PINCM2_PF_I2C0_SCL


/* Defines for UART_0 */
#define UART_0_INST                                                        UART0
#define UART_0_INST_FREQUENCY                                           32000000
#define UART_0_INST_IRQHandler                                  UART0_IRQHandler
#define UART_0_INST_INT_IRQN                                      UART0_INT_IRQn
#define GPIO_UART_0_RX_PORT                                                GPIOA
#define GPIO_UART_0_TX_PORT                                                GPIOA
#define GPIO_UART_0_RX_PIN                                         DL_GPIO_PIN_9
#define GPIO_UART_0_TX_PIN                                        DL_GPIO_PIN_17
#define GPIO_UART_0_IOMUX_RX                                     (IOMUX_PINCM10)
#define GPIO_UART_0_IOMUX_TX                                     (IOMUX_PINCM18)
#define GPIO_UART_0_IOMUX_RX_FUNC                      IOMUX_PINCM10_PF_UART0_RX
#define GPIO_UART_0_IOMUX_TX_FUNC                      IOMUX_PINCM18_PF_UART0_TX
#define UART_0_BAUD_RATE                                                  (9600)
#define UART_0_IBRD_32_MHZ_9600_BAUD                                       (208)
#define UART_0_FBRD_32_MHZ_9600_BAUD                                        (21)





/* Defines for ADC0 */
#define ADC0_INST                                                           ADC0
#define ADC0_INST_IRQHandler                                     ADC0_IRQHandler
#define ADC0_INST_INT_IRQN                                       (ADC0_INT_IRQn)
#define ADC0_ADCMEM_0                                         DL_ADC12_MEM_IDX_0
#define ADC0_ADCMEM_0_REF                        DL_ADC12_REFERENCE_VOLTAGE_VDDA
#define ADC0_ADCMEM_0_REF_VOLTAGE_V                                          3.3
#define ADC0_ADCMEM_1                                         DL_ADC12_MEM_IDX_1
#define ADC0_ADCMEM_1_REF                        DL_ADC12_REFERENCE_VOLTAGE_VDDA
#define ADC0_ADCMEM_1_REF_VOLTAGE_V                                          3.3
#define ADC0_ADCMEM_2                                         DL_ADC12_MEM_IDX_2
#define ADC0_ADCMEM_2_REF                        DL_ADC12_REFERENCE_VOLTAGE_VDDA
#define ADC0_ADCMEM_2_REF_VOLTAGE_V                                          3.3
#define ADC0_ADCMEM_3                                         DL_ADC12_MEM_IDX_3
#define ADC0_ADCMEM_3_REF                        DL_ADC12_REFERENCE_VOLTAGE_VDDA
#define ADC0_ADCMEM_3_REF_VOLTAGE_V                                          3.3
#define ADC0_INST_SUB_CH                                                     (1)
#define GPIO_ADC0_C0_PORT                                                  GPIOA
#define GPIO_ADC0_C0_PIN                                          DL_GPIO_PIN_27
#define GPIO_ADC0_IOMUX_C0                                       (IOMUX_PINCM28)
#define GPIO_ADC0_IOMUX_C0_FUNC                   (IOMUX_PINCM28_PF_UNCONNECTED)
#define GPIO_ADC0_C1_PORT                                                  GPIOA
#define GPIO_ADC0_C1_PIN                                          DL_GPIO_PIN_26
#define GPIO_ADC0_IOMUX_C1                                       (IOMUX_PINCM27)
#define GPIO_ADC0_IOMUX_C1_FUNC                   (IOMUX_PINCM27_PF_UNCONNECTED)
#define GPIO_ADC0_C2_PORT                                                  GPIOA
#define GPIO_ADC0_C2_PIN                                          DL_GPIO_PIN_25
#define GPIO_ADC0_IOMUX_C2                                       (IOMUX_PINCM26)
#define GPIO_ADC0_IOMUX_C2_FUNC                   (IOMUX_PINCM26_PF_UNCONNECTED)
#define GPIO_ADC0_C3_PORT                                                  GPIOA
#define GPIO_ADC0_C3_PIN                                          DL_GPIO_PIN_24
#define GPIO_ADC0_IOMUX_C3                                       (IOMUX_PINCM25)
#define GPIO_ADC0_IOMUX_C3_FUNC                   (IOMUX_PINCM25_PF_UNCONNECTED)



/* Port definition for Pin Group MCU_GPIO */
#define MCU_GPIO_PORT                                                    (GPIOA)

/* Defines for MCU_GPIO0_1: GPIOA.18 with pinCMx 19 on package pin 21 */
#define MCU_GPIO_MCU_GPIO0_1_PIN                                (DL_GPIO_PIN_18)
#define MCU_GPIO_MCU_GPIO0_1_IOMUX                               (IOMUX_PINCM19)
/* Port definition for Pin Group GPIO_485_GRP */
#define GPIO_485_GRP_PORT                                                (GPIOA)

/* Defines for PIN_485_ENABLE: GPIOA.15 with pinCMx 16 on package pin 18 */
#define GPIO_485_GRP_PIN_485_ENABLE_PIN                         (DL_GPIO_PIN_15)
#define GPIO_485_GRP_PIN_485_ENABLE_IOMUX                        (IOMUX_PINCM16)
/* Port definition for Pin Group GPIO_ADDR_GRP */
#define GPIO_ADDR_GRP_PORT                                               (GPIOA)

/* Defines for PIN_ADDR0: GPIOA.14 with pinCMx 15 on package pin 17 */
#define GPIO_ADDR_GRP_PIN_ADDR0_PIN                             (DL_GPIO_PIN_14)
#define GPIO_ADDR_GRP_PIN_ADDR0_IOMUX                            (IOMUX_PINCM15)
/* Defines for PIN_ADDR1: GPIOA.16 with pinCMx 17 on package pin 19 */
#define GPIO_ADDR_GRP_PIN_ADDR1_PIN                             (DL_GPIO_PIN_16)
#define GPIO_ADDR_GRP_PIN_ADDR1_IOMUX                            (IOMUX_PINCM17)


/* Defines for WWDT */
#define WWDT0_INST                                                       (WWDT0)
#define WWDT0_INT_IRQN                                          (WWDT0_INT_IRQn)



/* clang-format on */

void SYSCFG_DL_init(void);
void SYSCFG_DL_initPower(void);
void SYSCFG_DL_GPIO_init(void);
void SYSCFG_DL_SYSCTL_init(void);
void SYSCFG_DL_CAPTURE_0_init(void);
void SYSCFG_DL_WAKEUP_TIMER_init(void);
void SYSCFG_DL_TIMER_ACMEAS_init(void);
void SYSCFG_DL_TAR_I2C_init(void);
void SYSCFG_DL_UART_0_init(void);
void SYSCFG_DL_ADC0_init(void);

void SYSCFG_DL_WWDT0_init(void);


#ifdef __cplusplus
}
#endif

#endif /* ti_msp_dl_config_h */
