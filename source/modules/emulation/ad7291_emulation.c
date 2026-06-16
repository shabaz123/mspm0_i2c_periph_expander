// ad7291_emulation.c


#include <stdint.h>
#include <stdbool.h>
#include <ti/driverlib/dl_adc12.h>
#include "ti_msp_dl_config.h"
#include "ad7291_emulation.h"

/*!
 * @brief Total # analog input channels
 */
#define AD7291_CH_CNT 4

/*!
 * @brief Total # of 16-bit user registers in the AD7291
 */
#define AD7291_REGFILE_LEN 33

/*!
 * @brief AD7291_REG_CMD indicates no channels are enabled
 */
#define AD7291_NO_CHANNELS_ENABLED 0xFF

/**
 * @brief   AD7291 emulated register file names
 */
enum ad7291_regs
{
    AD7291_REG_CMD = 0x00,
    AD7291_REG_VCONV_RES = 0x01,
    AD7291_REG_TCONV_RES = 0x02,
    AD7291_REG_TAVG_RES = 0x03,
    AD7291_REG_CH0_DHIGH = 0x04,
    AD7291_REG_CH0_DLOW = 0x05,
    AD7291_REG_CH0_HYST = 0x06,
    AD7291_REG_CH1_DHIGH = 0x07,
    AD7291_REG_CH1_DLOW = 0x08,
    AD7291_REG_CH1_HYST = 0x09,
    AD7291_REG_CH2_DHIGH = 0x0A,
    AD7291_REG_CH2_DLOW = 0x0B,
    AD7291_REG_CH2_HYST = 0x0C,
    AD7291_REG_CH3_DHIGH = 0x0D,
    AD7291_REG_CH3_DLOW = 0x0E,
    AD7291_REG_CH3_HYST = 0x0F,
    AD7291_REG_CH4_DHIGH = 0x10,
    AD7291_REG_CH4_DLOW = 0x11,
    AD7291_REG_CH4_HYST = 0x12,
    AD7291_REG_CH5_DHIGH = 0x13,
    AD7291_REG_CH5_DLOW = 0x14,
    AD7291_REG_CH5_HYST = 0x15,
    AD7291_REG_CH6_DHIGH = 0x16,
    AD7291_REG_CH6_DLOW = 0x17,
    AD7291_REG_CH6_HYST = 0x18,
    AD7291_REG_CH7_DHIGH = 0x19,
    AD7291_REG_CH7_DLOW = 0x1A,
    AD7291_REG_CH7_HYST = 0x1B,
    AD7291_REG_TS_DHIGH = 0x1C,
    AD7291_REG_TS_DLOW = 0x1D,
    AD7291_REG_TS_HYST = 0x1E,
    AD7291_REG_ALERT_A = 0x1F,
    AD7291_REG_ALERT_B = 0x20,
    AD7291_REG_INVALID = 0x21
};

typedef union
{
    uint16_t r16;
    uint8_t r8[2];
} ad7291_reg_t;

// capture related definitions
#define VREG_PERIOD_COUNTS      0x40
#define VREG_DUTY_PERCENTX10    0x41
// capture global vars from system.c
extern volatile bool captureValid;
extern volatile uint16_t capturePeriodCounts;
extern volatile uint16_t captureHighCounts;
extern volatile uint32_t wakeTicks;
extern volatile uint32_t captureLastTick;

// acmeas related definitions
#define ACMEAS_START_REG 0x50
#define ACMEAS_RESULT_REG 0x51
extern volatile uint8_t acmeas_state;
extern volatile uint16_t acmeas_rms_result;
extern volatile uint16_t acmeas_pp_result;
extern volatile uint16_t acmeas_freq_result;
extern volatile uint16_t acmeas_dclevel_result;
extern volatile uint64_t sum_sq_adc;        // RMS accumulator
extern volatile uint32_t sum_adc;       // DC offset accumulator
extern volatile uint32_t sample_count;  // number of samples
extern volatile uint16_t min_adc;       // for p-p
extern volatile uint16_t max_adc;
extern freq_counter_t freq_counter;
extern volatile bool adc_restore_pending;

/**
 * @brief   AD7291 emulated register file storage
 */
ad7291_reg_t ad7291_regFile[AD7291_REGFILE_LEN];

/**
 * @brief   AD7291 emulated register file storage
 */
const uint8_t ad7291_chLUT[AD7291_CH_CNT] =
{
    DL_ADC12_INPUT_CHAN_0,
    DL_ADC12_INPUT_CHAN_1,
    DL_ADC12_INPUT_CHAN_2,
    DL_ADC12_INPUT_CHAN_3
};

/**
 * @brief   AD7291 emulated register file pointer register
 */
uint8_t ad7291_regFilePtr;

/**
 * @brief   AD7291 next channel select
 */
uint8_t ad7291_nextChannel;

/**
 * @brief   I2C target driver to use for read/write operations
 *          when handling callbacks
 */
i2c_tar_driver_t *ad7291_i2c_tar_driver_handle;

/**
 *  @brief      Set the global next channel pointer to default value
 *
 *  @param      none
 *
 *  @return     none
 *
 */
static void ad7291_resetNextChannel(void);

/**
 *  @brief      Get the next channel index to convert (index in channel LUT)
 *
 *  @param      none
 *
 *  @return     The next channel index to convert
 *
 */
static uint8_t ad7291_getNextChannel(void);

/**
 *  @brief      Convert the selected analog input
 *
 *  @param      MSPM0 ADC channel index to convert
 *
 *  @return     The 12-bit conversion result
 *
 */
static uint16_t AD7291_convertAnalogInput(uint8_t channel);

/**
 * Standard function implementations
 */

// this is used for the AC measurement mode,
// i.e. when the user writes to register ACMEAS_START_REG
static void adc_config_acmeas_event_trigger(void)
{
    DL_ADC12_disableConversions(ADC0_INST);

    DL_ADC12_initSeqSample(ADC0_INST,
        DL_ADC12_REPEAT_MODE_ENABLED,
        DL_ADC12_SAMPLING_SOURCE_AUTO,
        DL_ADC12_TRIG_SRC_EVENT,
        DL_ADC12_SEQ_START_ADDR_03,
        DL_ADC12_SEQ_END_ADDR_03,
        DL_ADC12_SAMP_CONV_RES_12_BIT,
        DL_ADC12_SAMP_CONV_DATA_FORMAT_UNSIGNED);

    DL_ADC12_setSubscriberChanID(ADC0_INST, ADC0_INST_SUB_CH);

    DL_ADC12_clearInterruptStatus(ADC0_INST,
        DL_ADC12_INTERRUPT_MEM3_RESULT_LOADED);

    DL_ADC12_enableInterrupt(ADC0_INST,
        DL_ADC12_INTERRUPT_MEM3_RESULT_LOADED);

    NVIC_ClearPendingIRQ(ADC0_INST_INT_IRQN);
    NVIC_EnableIRQ(ADC0_INST_INT_IRQN);

    DL_ADC12_enableConversions(ADC0_INST);
}

// this function is used when acmeas completes, to go back to the normal AD7291 emulation mode
static void adc_config_software_trigger(void)
{
    DL_ADC12_disableConversions(ADC0_INST);

    DL_ADC12_disableInterrupt(ADC0_INST,
        DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED |
        DL_ADC12_INTERRUPT_MEM1_RESULT_LOADED |
        DL_ADC12_INTERRUPT_MEM2_RESULT_LOADED |
        DL_ADC12_INTERRUPT_MEM3_RESULT_LOADED);

    NVIC_DisableIRQ(ADC0_INST_INT_IRQN);
    NVIC_ClearPendingIRQ(ADC0_INST_INT_IRQN);

    DL_ADC12_clearInterruptStatus(ADC0_INST,
        DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED |
        DL_ADC12_INTERRUPT_MEM1_RESULT_LOADED |
        DL_ADC12_INTERRUPT_MEM2_RESULT_LOADED |
        DL_ADC12_INTERRUPT_MEM3_RESULT_LOADED);

    DL_ADC12_initSeqSample(ADC0_INST,
        DL_ADC12_REPEAT_MODE_DISABLED,
        DL_ADC12_SAMPLING_SOURCE_AUTO,
        DL_ADC12_TRIG_SRC_SOFTWARE,
        DL_ADC12_SEQ_START_ADDR_00,
        DL_ADC12_SEQ_END_ADDR_02,
        DL_ADC12_SAMP_CONV_RES_12_BIT,
        DL_ADC12_SAMP_CONV_DATA_FORMAT_UNSIGNED);

    DL_ADC12_enableConversions(ADC0_INST);
}


// Capture related functions
static bool capture_isFreshAndValid(void)
{
    if (!captureValid) return false;
    if ((wakeTicks - captureLastTick) >= CAPTURE_STALE_TICKS) return false;
    return true;
}

static uint16_t capture_getPeriodCounts(void)
{
    if (!capture_isFreshAndValid()) return 0xFFFF;
    return capturePeriodCounts;
}

static uint16_t capture_getDutyPercentX10(void)
{
    if (!capture_isFreshAndValid()) return 0xFFFF;
    if (capturePeriodCounts == 0) return 0xFFFF;
    if (captureHighCounts == 0xFFFF) return 0xFFFF;

    return (uint16_t)(((uint32_t)captureHighCounts * 1000U) /
        capturePeriodCounts);
}



// AD7291 related functions
void ad7291_open(i2c_tar_driver_t *pI2CTarDriverInst)
{
    uint8_t i;

    ad7291_i2c_tar_driver_handle = pI2CTarDriverInst;
    ad7291_regFilePtr = 0;

    for (i=0; i<AD7291_REGFILE_LEN; i++)
    {
        ad7291_regFile[i].r16 = 0x0000;
    }

    ad7291_regFile[AD7291_REG_CH0_DHIGH].r16 = 0x0FFF;
    ad7291_regFile[AD7291_REG_CH1_DHIGH].r16 = 0x0FFF;
    ad7291_regFile[AD7291_REG_CH2_DHIGH].r16 = 0x0FFF;
    ad7291_regFile[AD7291_REG_CH3_DHIGH].r16 = 0x0FFF;
    ad7291_regFile[AD7291_REG_CH4_DHIGH].r16 = 0x0FFF;
    ad7291_regFile[AD7291_REG_CH5_DHIGH].r16 = 0x0FFF;
    ad7291_regFile[AD7291_REG_CH6_DHIGH].r16 = 0x0FFF;
    ad7291_regFile[AD7291_REG_CH7_DHIGH].r16 = 0x0FFF;
    ad7291_regFile[AD7291_REG_TS_DHIGH].r16 = 0x0800;
    ad7291_regFile[AD7291_REG_TS_DLOW].r16 = 0x07FF;

    /*
     * Take dummy conversion of the internal supply monitor to fire up the ADC
     */
    AD7291_convertAnalogInput(0);  // chan 15 is not configured, so use 0

    return;
}

void ad7291_resetNextChannel(void)
{
    ad7291_nextChannel = 0;
}

uint8_t ad7291_getNextChannel(void)
{
    uint8_t enabledChannelsMask;
    uint8_t channelSelMask;
    uint8_t channelSel;

    enabledChannelsMask = ad7291_regFile[AD7291_REG_CMD].r8[1];

    if (enabledChannelsMask == 0)
    {
        return AD7291_NO_CHANNELS_ENABLED;
    }

    do {
        channelSel = ad7291_nextChannel;
        channelSelMask = 0x80 >> ad7291_nextChannel;
        if (++ad7291_nextChannel >= 4)
        {
            ad7291_nextChannel = 0;
        }
    } while ((enabledChannelsMask & channelSelMask) == 0);

    return channelSel;
}

uint16_t AD7291_convertAnalogInput(uint8_t channel)
{
    uint32_t timeout = 100000;

    DL_ADC12_clearInterruptStatus(ADC0,
        DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED |
        DL_ADC12_INTERRUPT_MEM1_RESULT_LOADED |
        DL_ADC12_INTERRUPT_MEM2_RESULT_LOADED |
        DL_ADC12_INTERRUPT_MEM3_RESULT_LOADED);

    NVIC_ClearPendingIRQ(ADC0_INST_INT_IRQN);

    if (adc_restore_pending) {
        adc_config_software_trigger();
        adc_restore_pending = false;
    }

    DL_ADC12_enableConversions(ADC0);
    DL_ADC12_startConversion(ADC0);

    while (!DL_ADC12_getRawInterruptStatus(ADC0,
            DL_ADC12_INTERRUPT_MEM2_RESULT_LOADED) && timeout--) {
    }

    if (timeout == 0) {
        return 0xFFFF;
    }

    switch (channel) {
        case DL_ADC12_INPUT_CHAN_0:
            return DL_ADC12_getMemResult(ADC0, DL_ADC12_MEM_IDX_0);
        case DL_ADC12_INPUT_CHAN_1:
            return DL_ADC12_getMemResult(ADC0, DL_ADC12_MEM_IDX_1);
        case DL_ADC12_INPUT_CHAN_2:
            return DL_ADC12_getMemResult(ADC0, DL_ADC12_MEM_IDX_2);
        case DL_ADC12_INPUT_CHAN_3:
            return DL_ADC12_getMemResult(ADC0, DL_ADC12_MEM_IDX_3);
        default:
            return 0xFFFF;
    }
}


// this is called when the master writes to the I2C target 
void ad7291_i2c_rx_callback(uint32_t bytes, i2c_tar_driver_call_trig_t trig)
{
    ad7291_reg_t updatedReg;
    uint8_t newRegFilePtr;

    while (bytes>0)
    {
        if (bytes >=3)
        {
            newRegFilePtr = I2CTarDriver_read(ad7291_i2c_tar_driver_handle);
            updatedReg.r8[1] = I2CTarDriver_read(ad7291_i2c_tar_driver_handle);
            updatedReg.r8[0] = I2CTarDriver_read(ad7291_i2c_tar_driver_handle);
            if (newRegFilePtr < AD7291_REG_INVALID)
            {
                ad7291_regFilePtr = newRegFilePtr;
                ad7291_regFile[ad7291_regFilePtr] = updatedReg;

            }
            bytes -= 3;
        }
        else if (bytes == 1)
        {
            newRegFilePtr = I2CTarDriver_read(ad7291_i2c_tar_driver_handle);
            if ((newRegFilePtr < AD7291_REG_INVALID) ||
                (newRegFilePtr == VREG_PERIOD_COUNTS) ||
                (newRegFilePtr == VREG_DUTY_PERCENTX10) ||
                (newRegFilePtr == ACMEAS_START_REG) ||
                (newRegFilePtr == ACMEAS_RESULT_REG))
            {
                ad7291_regFilePtr = newRegFilePtr;
            }
            bytes -= 1;
        }
        if (ad7291_regFilePtr == AD7291_REG_VCONV_RES)
        {
            ad7291_resetNextChannel();
        } else if (ad7291_regFilePtr == ACMEAS_START_REG)
        {
            // we only start if the state is not running, otherwise ignore the write
            if (acmeas_state != ACMEAS_STATE_RUNNING)
            {
                acmeas_state = ACMEAS_STATE_RUNNING;

                DL_TimerG_stopCounter(TIMER_ACMEAS_INST);
                DL_Timer_clearInterruptStatus(TIMER_ACMEAS_INST, DL_TIMER_INTERRUPT_ZERO_EVENT);
                adc_config_acmeas_event_trigger();
                //NVIC_ClearPendingIRQ(TIMER_ACMEAS_INST_INT_IRQN);
                //NVIC_EnableIRQ(TIMER_ACMEAS_INST_INT_IRQN);

                freq_counter.state = WAIT_FOR_NEGATIVE;
                freq_counter.sample_index = 0;
                freq_counter.last_pos_crossing = 0;
                freq_counter.period_samples = 0;
                freq_counter.valid = 0;
                sum_sq_adc = 0; // reset the accumulators
                sum_adc = 0;
                sample_count = 0;
                acmeas_rms_result = 0;
                min_adc = 0x0fff;  // reset the min/max for p-p; min_adc should be set to 12-bit max!
                max_adc = 0x0000; // not a bug; max_adc should be set to 0!
                acmeas_pp_result = 0;
                acmeas_dclevel_result = 0;
                acmeas_freq_result = 0;
                acmeas_state = ACMEAS_STATE_RUNNING;
                // start the timer to begin sampling
                DL_TimerG_startCounter(TIMER_ACMEAS_INST);
            }
        }
    }
}

// this is called when the master reads from the I2C target
void ad7291_i2c_tx_callback(void)
{
    uint8_t channel;
    uint16_t virtualValue;

    if (ad7291_regFilePtr == ACMEAS_RESULT_REG)
    {
        if (acmeas_state != ACMEAS_STATE_DONE)
        {
            // if we are not done, return 0 for all results
            //for (int i=0; i<8; i++) {
            //    I2CTarDriver_write(ad7291_i2c_tar_driver_handle, 0);
            //}

            // debug, return the state of the acmeas_state variable and
            // the sample count
            I2CTarDriver_write(ad7291_i2c_tar_driver_handle, acmeas_state);
            I2CTarDriver_write(ad7291_i2c_tar_driver_handle, 0);
            I2CTarDriver_write(ad7291_i2c_tar_driver_handle, sample_count >> 8);
            I2CTarDriver_write(ad7291_i2c_tar_driver_handle, sample_count & 0xFF);
            I2CTarDriver_write(ad7291_i2c_tar_driver_handle, 0);
            I2CTarDriver_write(ad7291_i2c_tar_driver_handle, 0);
            I2CTarDriver_write(ad7291_i2c_tar_driver_handle, 0);
            I2CTarDriver_write(ad7291_i2c_tar_driver_handle, 0);

            return;
        } else {
            I2CTarDriver_write(ad7291_i2c_tar_driver_handle, acmeas_rms_result >> 8);
            I2CTarDriver_write(ad7291_i2c_tar_driver_handle, acmeas_rms_result & 0xFF);
            I2CTarDriver_write(ad7291_i2c_tar_driver_handle, acmeas_pp_result >> 8);
            I2CTarDriver_write(ad7291_i2c_tar_driver_handle, acmeas_pp_result & 0xFF);
            I2CTarDriver_write(ad7291_i2c_tar_driver_handle, acmeas_dclevel_result >> 8);
            I2CTarDriver_write(ad7291_i2c_tar_driver_handle, acmeas_dclevel_result & 0xFF);
            I2CTarDriver_write(ad7291_i2c_tar_driver_handle, acmeas_freq_result >> 8);
            I2CTarDriver_write(ad7291_i2c_tar_driver_handle, acmeas_freq_result & 0xFF);
            acmeas_state = ACMEAS_STATE_IDLE;
            return;
        }
    }

    if (ad7291_regFilePtr == VREG_PERIOD_COUNTS)
    {
        virtualValue = capture_getPeriodCounts();

        I2CTarDriver_write(ad7291_i2c_tar_driver_handle, virtualValue >> 8);
        I2CTarDriver_write(ad7291_i2c_tar_driver_handle, virtualValue & 0xFF);
        return;
    }

    if (ad7291_regFilePtr == VREG_DUTY_PERCENTX10)
    {
        virtualValue = capture_getDutyPercentX10();

        I2CTarDriver_write(ad7291_i2c_tar_driver_handle, virtualValue >> 8);
        I2CTarDriver_write(ad7291_i2c_tar_driver_handle, virtualValue & 0xFF);
        return;
    }

    if (ad7291_regFilePtr == AD7291_REG_VCONV_RES)
    {
        channel = ad7291_getNextChannel();
        if (channel == AD7291_NO_CHANNELS_ENABLED)
        {
            ad7291_regFile[AD7291_REG_VCONV_RES].r16 = 0;
        }
        else
        {
            ad7291_regFile[AD7291_REG_VCONV_RES].r16 =
                AD7291_convertAnalogInput(ad7291_chLUT[channel]);
            ad7291_regFile[AD7291_REG_VCONV_RES].r8[1] =
                (ad7291_regFile[AD7291_REG_VCONV_RES].r8[1] & 0x0F) |
                ((channel << 4) & 0xF0);
        }
    }

    // Write back 16 bit data for host to read out
    I2CTarDriver_write(ad7291_i2c_tar_driver_handle,
        ad7291_regFile[ad7291_regFilePtr].r8[1]);
    I2CTarDriver_write(ad7291_i2c_tar_driver_handle,
        ad7291_regFile[ad7291_regFilePtr].r8[0]);
}

