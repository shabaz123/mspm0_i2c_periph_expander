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
    SYSCFG_DL_SYSCTL_init();
    SYSCFG_DL_CAPTURE_0_init();
    SYSCFG_DL_WAKEUP_TIMER_init();
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
