// ad7291_emulation.h
// modified, it now serves to support other registers too, i.e.
// no longer an exact emulation of the ad7291

#ifndef EMULATION_AD7291_EMULATION_H_
#define EMULATION_AD7291_EMULATION_H_

#include "communication/i2c_tar_driver.h"

/**
 *  @brief      Open the AD7291 ADC emulation library
 *
 *  This function opens the ADC emulation library.  It must be called
 *  at start-up before I2C requests from external I2C bus controllers may be
 *  handled by the device.
 *
 *  This function expects that the I2C target driver instance which is passed
 *  has been initialized by the application with the expected configuration
 *  settings and target address.
 *
 *  This function expects that the application above this layer has configured
 *  the IOMUX to enable I2C communication on the desired pins to the module
 *  whose base address was passed to this function.
 *
 *  @param[in]  Pointer to the I2C target driver instance to use.
 *
 *  @return     none
 *
 */

#ifndef CAPTURE_STALE_TICKS
#define CAPTURE_STALE_TICKS 2   // about 1 second
#endif

extern void ad7291_open(i2c_tar_driver_t *pI2CTarDriverInst);

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
extern void ad7291_i2c_rx_callback(uint32_t bytes, \
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
extern void ad7291_i2c_tx_callback(void);

#endif /* EMULATION_AD7291_EMULATION_H_ */

