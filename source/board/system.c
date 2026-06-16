// system.c

#include "ti_msp_dl_config.h"
#include "communication/i2c_tar_driver.h"
#include "emulation/sc16is740_emulation.h"
#include "emulation/ad7291_emulation.h"
#include "system.h"

/*!
 * @brief   Length of the I2C receive buffer in bytes
 */
#define TAR_I2C_RXBUF_LEN 128

/*!
 * @brief   Length of the I2C transmit buffer in bytes
 */
#define TAR_I2C_TXBUF_LEN 128

/**
 * @brief   I2C target driver RX buffer
 */
uint8_t tar_i2c_rxbuf[TAR_I2C_RXBUF_LEN];

/**
 * @brief   I2C target driver TX buffer
 */
uint8_t tar_i2c_txbuf[TAR_I2C_TXBUF_LEN];

/**
 * The I2C target driver data structure instance for the I2C I/F we
 * are using for communication with external I2C bus controllers
 */
i2c_tar_driver_t tar_i2c =
{
    .pModule = TAR_I2C_INST,
    .pRxBuf = &tar_i2c_rxbuf[0],
    .rxBufLen = TAR_I2C_RXBUF_LEN,
    .pTxBuf = &tar_i2c_txbuf[0],
    .txBufLen = TAR_I2C_TXBUF_LEN,
    .rxCallback = &sc16is740_i2c_rx_callback,
    .txCallback = &sc16is740_i2c_tx_callback,
    .rxCallback2 = &ad7291_i2c_rx_callback,
    .txCallback2 = &ad7291_i2c_tx_callback
};

// global variable to hold the address-select pins value (0-3)
uint8_t addrSel = 0;

// global variables for timer capture
#define CAPTURE_TIMER_LOAD_VALUE      (0xFFFFU)
#define CAPTURE_TIMER_WRAP_COUNTS     (65536UL)
#define CAPTURE_CLOCK_HZ              (4000000UL)
#define CAPTURE_MIN_FREQ_HZ           (70UL)
#define CAPTURE_MAX_PERIOD_COUNTS     (CAPTURE_CLOCK_HZ / CAPTURE_MIN_FREQ_HZ)

volatile bool captureValid;
volatile uint16_t capturePeriodCounts;
volatile uint16_t captureHighCounts;

// acmeas related variables
volatile uint8_t acmeas_state;
volatile uint16_t acmeas_rms_result;
volatile uint16_t acmeas_pp_result;
volatile uint16_t acmeas_freq_result;
volatile uint16_t acmeas_dclevel_result;
volatile uint64_t sum_sq_adc;        // RMS accumulator
volatile uint32_t sum_adc;       // DC offset accumulator
volatile uint32_t sample_count;  // number of samples
uint32_t  dc_level;     // estimated or fixed midpoint, e.g. 2048
int32_t  x;             // centred ADC sample
uint16_t min_adc;       // for p-p
uint16_t max_adc;
uint16_t pp_counts;
uint32_t rms_counts;
freq_counter_t freq_counter;
volatile bool adc_restore_pending;

// Wake-timer based stale detection: no capture for ~1 second => invalid
volatile uint32_t wakeTicks;
volatile uint32_t captureLastTick;

// Extended timestamp state for detecting true below-range periods
static volatile uint32_t captureOverflowCount;
static uint32_t captureLastExt;
static bool captureHaveLastExt;

static inline uint32_t capture_makeExtTimestamp(uint32_t overflowCount,
    uint16_t captureValue)
{
    return (overflowCount * CAPTURE_TIMER_WRAP_COUNTS) +
        ((uint16_t)(CAPTURE_TIMER_LOAD_VALUE - captureValue));
}

/*!
 * @brief Indicates pending system events
 */
volatile uint32_t System_pendingEvents;

static const DL_I2C_ClockConfig localTAR_I2CClockConfig = {
    .clockSel = DL_I2C_CLOCK_BUSCLK,
    .divideRatio = DL_I2C_CLOCK_DIVIDE_1,
};




// this is a copy of SYSCFG_DL_TAR_I2C_init
// but modiied so we can dynamically set the I2C addresses
// based on ADDR0 and ADDR pin states at runtime
void local_SYSCFG_DL_TAR_I2C_init(void) {

    DL_I2C_setClockConfig(TAR_I2C_INST,
        (DL_I2C_ClockConfig *) &localTAR_I2CClockConfig);
    DL_I2C_setAnalogGlitchFilterPulseWidth(TAR_I2C_INST,
        DL_I2C_ANALOG_GLITCH_FILTER_WIDTH_50NS);
    DL_I2C_enableAnalogGlitchFilter(TAR_I2C_INST);

    // read ADDR0 and ADDR1 pin states to determine I2C target own address and alternate own address
    delay_cycles(1000); /* Allow ADDR pins to settle after GPIO config */
    uint8_t addr0State =
        (DL_GPIO_readPins(GPIO_ADDR_GRP_PORT, GPIO_ADDR_GRP_PIN_ADDR0_PIN) != 0) ? 1 : 0;
    uint8_t addr1State =
        (DL_GPIO_readPins(GPIO_ADDR_GRP_PORT, GPIO_ADDR_GRP_PIN_ADDR1_PIN) != 0) ? 1 : 0;
    
    addrSel = (addr1State << 1) | addr0State;

    /* Configure Target Mode */
    DL_I2C_setTargetOwnAddress(TAR_I2C_INST, BASE_PRI_ADDR + addrSel);
    DL_I2C_setTargetOwnAddressAlternate(TAR_I2C_INST, BASE_SEC_ADDR + addrSel);
    DL_I2C_enableTargetOwnAddressAlternate(TAR_I2C_INST);
    DL_I2C_setTargetTXFIFOThreshold(TAR_I2C_INST, DL_I2C_TX_FIFO_LEVEL_EMPTY);
    DL_I2C_setTargetRXFIFOThreshold(TAR_I2C_INST, DL_I2C_RX_FIFO_LEVEL_BYTES_1);
    DL_I2C_enableTargetTXEmptyOnTXRequest(TAR_I2C_INST);
    DL_I2C_enableTargetClockStretching(TAR_I2C_INST);
    /* Workaround for errata I2C_ERR_04 */
    DL_I2C_disableTargetWakeup(TAR_I2C_INST);
}

void System_init(void)
{
    System_pendingEvents = SYSEVENT__NONE_PENDING;

    //SYSCFG_DL_init();
    // SYSCFG_DL_init replaced with the following
    SYSCFG_DL_initPower();
    SYSCFG_DL_GPIO_init();
    DL_GPIO_initPeripheralAnalogFunction(GPIO_ADC0_IOMUX_C0);
    DL_GPIO_initPeripheralAnalogFunction(GPIO_ADC0_IOMUX_C1);
    DL_GPIO_initPeripheralAnalogFunction(GPIO_ADC0_IOMUX_C2);
    DL_GPIO_initPeripheralAnalogFunction(GPIO_ADC0_IOMUX_C3);
    SYSCFG_DL_SYSCTL_init();
    SYSCFG_DL_CAPTURE_0_init();
    SYSCFG_DL_WAKEUP_TIMER_init();
    SYSCFG_DL_TIMER_ACMEAS_init();
    local_SYSCFG_DL_TAR_I2C_init();
    SYSCFG_DL_UART_0_init();
    SYSCFG_DL_ADC0_init();
    SYSCFG_DL_WWDT0_init();
    
    
    /* Move I2C pins to GPI, check for bus-high condition before
     * continuing to start the I2C target driver. */
    DL_GPIO_initDigitalInputFeatures(GPIO_TAR_I2C_IOMUX_SDA, \
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_DOWN, \
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(GPIO_TAR_I2C_IOMUX_SCL, \
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_DOWN, \
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    while (DL_GPIO_readPins(GPIOA, 0x03) != 0x03);

    DL_GPIO_initPeripheralInputFunctionFeatures(GPIO_TAR_I2C_IOMUX_SDA,
        GPIO_TAR_I2C_IOMUX_SDA_FUNC, DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE,
        DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initPeripheralInputFunctionFeatures(GPIO_TAR_I2C_IOMUX_SCL,
        GPIO_TAR_I2C_IOMUX_SCL_FUNC, DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE,
        DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_enableHiZ(GPIO_TAR_I2C_IOMUX_SDA);
    DL_GPIO_enableHiZ(GPIO_TAR_I2C_IOMUX_SCL);

    /* Start the UART emulation module */
    sc16is740_open(&tar_i2c);

    /* Start the ADC emulation module */
    ad7291_open(&tar_i2c);

    // acmeas
    acmeas_state = ACMEAS_STATE_IDLE;
    dc_level = 2048;
    min_adc = 2048;
    max_adc = 2048;
    adc_restore_pending = false;

    /* Start the I2C target driver */
    I2CTarDriver_open(&tar_i2c);

    /* CAPTURE_0_INST timer capture */
    captureValid = false;
    capturePeriodCounts = 0xFFFF;
    captureHighCounts = 0xFFFF;
    captureLastTick = wakeTicks;
    captureOverflowCount = 0;
    captureLastExt = 0;
    captureHaveLastExt = false;

    DL_TimerG_setLoadValue(CAPTURE_0_INST, CAPTURE_TIMER_LOAD_VALUE);
    NVIC_EnableIRQ(CAPTURE_0_INST_INT_IRQN);
    DL_TimerG_startCounter(CAPTURE_0_INST);

    /* Enable interrupts used by the application */
    NVIC_EnableIRQ(WAKEUP_TIMER_INST_INT_IRQN);
    NVIC_EnableIRQ(TAR_I2C_INST_INT_IRQN);
    NVIC_ClearPendingIRQ(ADC0_INST_INT_IRQN);
    NVIC_EnableIRQ(ADC0_INST_INT_IRQN);
}

void System_sleepUntilInterrupt(void)
{
    __disable_irq();
    if (System_pendingEvents == SYSEVENT__NONE_PENDING)
    {
        __WFI();
    }
    __enable_irq();
}

void WAKEUP_TIMER_INST_IRQHandler(void)
{
    DL_Timer_clearInterruptStatus(WAKEUP_TIMER_INST, \
        DL_TIMER_INTERRUPT_ZERO_EVENT);
    wakeTicks++;
    System_pendingEvents |= SYSEVENT__WAKE_TIMER;
}

void TAR_I2C_INST_IRQHandler(void)
{
    I2CTarDriver_ISR(&tar_i2c);
}


void CAPTURE_0_INST_IRQHandler(void)
{
    switch (DL_TimerG_getPendingInterrupt(CAPTURE_0_INST)) {
        case DL_TIMERG_IIDX_CC1_DN:
        {
            uint32_t overflowCount = captureOverflowCount;

            uint16_t nowPeriod = (uint16_t) DL_TimerG_getCaptureCompareValue(
                CAPTURE_0_INST, DL_TIMER_CC_1_INDEX);

            uint16_t nowHigh = (uint16_t) DL_TimerG_getCaptureCompareValue(
                CAPTURE_0_INST, DL_TIMER_CC_0_INDEX);

            uint32_t nowPeriodExt = capture_makeExtTimestamp(overflowCount,
                nowPeriod);
            uint32_t nowHighExt = capture_makeExtTimestamp(overflowCount,
                nowHigh);

            captureLastTick = wakeTicks;

            if (captureHaveLastExt) {
                uint32_t period = nowPeriodExt - captureLastExt;
                uint32_t high = nowHighExt - captureLastExt;

                /*
                 * In combined pulse-width/period mode CC0 may appear one whole
                 * period offset from CC1. Fold it back, preserving the behavior
                 * of the earlier working implementation.
                 */
                if (high > period) {
                    high -= period;
                }

                if ((period > 0U) &&
                    (period <= CAPTURE_MAX_PERIOD_COUNTS) &&
                    (high <= period)) {
                    capturePeriodCounts = (uint16_t) period;
                    captureHighCounts = (uint16_t) high;
                    captureValid = true;
                } else {
                    capturePeriodCounts = 0xFFFF;
                    captureHighCounts = 0xFFFF;
                    captureValid = false;
                }
            }

            captureLastExt = nowPeriodExt;
            captureHaveLastExt = true;
            break;
        }

        case DL_TIMERG_IIDX_ZERO:
            captureOverflowCount++;
            break;

        default:
            break;
    }
}

// ****  acmeas  ****

// this function is used when acmeas completes, to go back to the normal AD7291 emulation mode
#ifdef OLD_CODE
static void adc_config_software_trigger(void)
{
    DL_ADC12_disableConversions(ADC0_INST);

    DL_ADC12_initSeqSample(ADC0_INST,
        DL_ADC12_REPEAT_MODE_DISABLED,
        DL_ADC12_SAMPLING_SOURCE_AUTO,
        DL_ADC12_TRIG_SRC_SOFTWARE,
        DL_ADC12_SEQ_START_ADDR_00,
        DL_ADC12_SEQ_END_ADDR_03,
        DL_ADC12_SAMP_CONV_RES_12_BIT,
        DL_ADC12_SAMP_CONV_DATA_FORMAT_UNSIGNED);

    DL_ADC12_enableConversions(ADC0_INST);
}
#endif



// add_acmeas_sample contains the accumulators so that DC offset and AC RMS can be calculated 
void add_acmeas_sample(uint16_t adc)
{
    sum_adc += (uint32_t)adc;  // accumulate for DC offset calculations
    sum_sq_adc += (uint64_t)adc * adc;  // accumulate for RMS calculations
    sample_count++;

    if (adc < min_adc) min_adc = adc;
    if (adc > max_adc) max_adc = adc;
}

void freq_add_sample(freq_counter_t *f, uint16_t adc)
{
    f->sample_index++;

    switch (f->state) {
    case WAIT_FOR_NEGATIVE:
        if (adc < ACMEAS_FCALC_NEG_THRESH) {
            f->state = WAIT_FOR_POSITIVE;
        }
        break;

    case WAIT_FOR_POSITIVE:
        if (adc > ACMEAS_FCALC_POS_THRESH) {
            if (f->last_pos_crossing != 0) {
                f->period_samples = f->sample_index - f->last_pos_crossing;
                f->valid = 1;
            }

            f->last_pos_crossing = f->sample_index;
            f->state = WAIT_FOR_NEGATIVE;
        }
        break;
    }
}

void ADC0_INST_IRQHandler(void)
{
    switch (DL_ADC12_getPendingInterrupt(ADC0_INST)) {
        case DL_ADC12_IIDX_MEM3_RESULT_LOADED:
            DL_Timer_clearInterruptStatus(TIMER_ACMEAS_INST, \
                DL_TIMER_INTERRUPT_ZERO_EVENT);
            if (acmeas_state == ACMEAS_STATE_RUNNING)
            {
                if (sample_count < ACMEAS_MAX_SAMPLE_COUNT)
                {
                    uint16_t adc = DL_ADC12_getMemResult(ADC0, DL_ADC12_MEM_IDX_3);
                    add_acmeas_sample(adc);
                    freq_add_sample(&freq_counter, adc);
                }
                else
                {
                    acmeas_state = ACMEAS_STATE_DONE;
                    // stop the timer to stop sampling
                    DL_TimerG_stopCounter(TIMER_ACMEAS_INST);
                    // NVIC_DisableIRQ(TIMER_ACMEAS_INST_INT_IRQN);
                    //adc_config_software_trigger();
                    adc_restore_pending = true;
                    
                    // calculate results
                    uint32_t n = sample_count;
                    dc_level = (uint32_t)((sum_adc + (n / 2)) / n);
                    uint64_t mean_sq = sum_sq_adc / n;
                    uint64_t dc_sq   = (uint64_t)dc_level * dc_level;
                    uint64_t ac_var = (mean_sq > dc_sq) ? (mean_sq - dc_sq) : 0;

                    acmeas_rms_result = (uint16_t)sqrt((double)ac_var);
                    acmeas_dclevel_result = (uint16_t)dc_level;
                    acmeas_pp_result = max_adc - min_adc;

                    acmeas_freq_result =
                        freq_counter.valid ? (ACMEAS_SAMPLE_RATE_HZ / freq_counter.period_samples) : 0;
                }
            }
            break;
        default:
            break;
    }

}

