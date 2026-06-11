#ifndef EMULATION_SC16IS740_EMULATION_H_
#define EMULATION_SC16IS740_EMULATION_H_

#include "communication/i2c_tar_driver.h"

/**
 * @brief   Initialize the SC16IS740 emulation module and UART hardware.
 *
 * @param[in]  pI2CTarDriverInst Pointer to the I2C target driver instance.
 */
void sc16is740_open(i2c_tar_driver_t *pI2CTarDriverInst);

/**
 * @brief   I2C receive callback for the SC16IS740 emulation.
 *
 * @param[in]  bytes Number of bytes received.
 * @param[in]  trig Trigger condition (START/RESTART).
 */
void sc16is740_i2c_rx_callback(uint32_t bytes, i2c_tar_driver_call_trig_t trig);

/**
 * @brief   I2C transmit callback for the SC16IS740 emulation.
 */
void sc16is740_i2c_tx_callback(void);

#endif /* EMULATION_SC16IS740_EMULATION_H_ */
