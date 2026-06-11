#include <stdint.h>
#include <stdbool.h>
#include <ti/driverlib/dl_uart_main.h>
#include <ti/driverlib/dl_gpio.h>
#include "sc16is740_emulation.h"
#include "ti_msp_dl_config.h"

/*!
 * @brief Circular buffer for UART RX
 */
#define UART_RX_BUF_SIZE 64
static uint8_t uart_rx_buf[UART_RX_BUF_SIZE];
static uint32_t uart_rx_head = 0;
static uint32_t uart_rx_tail = 0;

/*!
 * @brief SC16IS740 Register addresses (shifted left by 3 bits for I2C)
 */
#define SC16IS740_REG_RHR_THR  (0x00 << 3)
#define SC16IS740_REG_IER      (0x01 << 3)
#define SC16IS740_REG_IIR_FCR  (0x02 << 3)
#define SC16IS740_REG_LCR      (0x03 << 3)
#define SC16IS740_REG_MCR      (0x04 << 3)
#define SC16IS740_REG_LSR      (0x05 << 3)
#define SC16IS740_REG_MSR      (0x06 << 3)
#define SC16IS740_REG_SPR      (0x07 << 3)
#define SC16IS740_REG_RESV     (0x0d << 3) // Reserved register repurposed

/*!
 * @brief LSR bits
 */
#define SC16IS740_LSR_DR       (1 << 0) // Data Ready
#define SC16IS740_LSR_THRE     (1 << 5) // THR Empty
#define SC16IS740_LSR_TEMT     (1 << 6) // Transmitter Empty

// baud rate registers
#define BAUD_9600_IBRD  208
#define BAUD_9600_FBRD  21
#define BAUD_115200_IBRD  17
#define BAUD_115200_FBRD  23
#define BAUD_19200_IBRD  104
#define BAUD_19200_FBRD  11
#define BAUD_38400_IBRD  52
#define BAUD_38400_FBRD  5
#define BAUD_57600_IBRD  34
#define BAUD_57600_FBRD  46
#define BAUD_312500_IBRD  64
#define BAUD_312500_FBRD  0



static uint8_t sc16is740_regFilePtr = 0;
static i2c_tar_driver_t *sc16is740_i2c_handle;

// Register state storage
static uint8_t sc16is740_ier = 0;
static uint8_t sc16is740_lcr = 0;
static uint8_t sc16is740_mcr = 0;
static uint8_t sc16is740_spr = 0;

// RS-485 auto TX control (MCR bit 1 can override this)
static volatile bool rs485_auto_tx_active = false;

static void rs485_enable_tx(void) {
    DL_GPIO_setPins(GPIO_485_GRP_PORT, GPIO_485_GRP_PIN_485_ENABLE_PIN);
}

static void rs485_disable_tx(void) {
    DL_GPIO_clearPins(GPIO_485_GRP_PORT, GPIO_485_GRP_PIN_485_ENABLE_PIN);
}

void sc16is740_open(i2c_tar_driver_t *pI2CTarDriverInst)
{
    sc16is740_i2c_handle = pI2CTarDriverInst;
    sc16is740_regFilePtr = 0;

    // Reset circular buffer
    uart_rx_head = 0;
    uart_rx_tail = 0;

    // UART0 configuration is expected to be initialized by SYSCFG_DL_init()
    // but we enable the RX interrupt here.
    DL_UART_Main_enableInterrupt(UART0,
    DL_UART_MAIN_INTERRUPT_RX | DL_UART_MAIN_INTERRUPT_EOT_DONE);
    NVIC_ClearPendingIRQ(UART0_INT_IRQn);
    NVIC_EnableIRQ(UART0_INT_IRQn);
}

void UART0_IRQHandler(void)
{
    switch (DL_UART_Main_getPendingInterrupt(UART0)) {
        case DL_UART_MAIN_IIDX_RX:
            while (!DL_UART_Main_isRXFIFOEmpty(UART0)) {
                uint8_t data = DL_UART_Main_receiveData(UART0);
                uint32_t next = (uart_rx_head + 1) % UART_RX_BUF_SIZE;
                if (next != uart_rx_tail) {
                    uart_rx_buf[uart_rx_head] = data;
                    uart_rx_head = next;
                }
            }
            break;

        case DL_UART_MAIN_IIDX_EOT_DONE:
            if (rs485_auto_tx_active && ((sc16is740_mcr & (1 << 1)) == 0)) {
                rs485_disable_tx();
            }
            rs485_auto_tx_active = false;
            break;

        default:
            break;
    }
}

// this is called when the master writes to the I2C target (i.e. writes to SC16IS740 registers)
void sc16is740_i2c_rx_callback(uint32_t bytes, i2c_tar_driver_call_trig_t trig)
{
    uint8_t ibrd = 0, fbrd = 0;
    if (bytes == 0) return;

    // First byte is always the register pointer
    sc16is740_regFilePtr = I2CTarDriver_read(sc16is740_i2c_handle);
    bytes--;

    if (sc16is740_regFilePtr == SC16IS740_REG_RHR_THR) {
        DL_UART_Main_disableInterrupt(UART0, DL_UART_MAIN_INTERRUPT_EOT_DONE);
        DL_UART_Main_clearInterruptStatus(UART0, DL_UART_MAIN_INTERRUPT_EOT_DONE);

        rs485_auto_tx_active = true;
        rs485_enable_tx();
    }

    // Subsequent bytes are data to write to the register
    while (bytes > 0) {
        uint8_t data = I2CTarDriver_read(sc16is740_i2c_handle);
        bytes--;

        switch (sc16is740_regFilePtr) {
            case SC16IS740_REG_RESV:
                // we will repurpose bits 0-3 to select a baud rate
                switch (data & 0x0F) {
                    case 0x0: // 9600 baud
                        ibrd = BAUD_9600_IBRD;
                        fbrd = BAUD_9600_FBRD;
                        break;
                    case 0x1: // 115200 baud
                        ibrd = BAUD_115200_IBRD;
                        fbrd = BAUD_115200_FBRD;
                        break;
                    case 0x2: // 19200 baud
                        ibrd = BAUD_19200_IBRD;
                        fbrd = BAUD_19200_FBRD;
                        break;
                    case 0x3: // 38400 baud
                        ibrd = BAUD_38400_IBRD;
                        fbrd = BAUD_38400_FBRD;
                        break;
                    case 0x4: // 57600 baud
                        ibrd = BAUD_57600_IBRD;
                        fbrd = BAUD_57600_FBRD;
                        break;
                    case 0x5: // 31250 baud (for MIDI)
                        ibrd = BAUD_312500_IBRD;
                        fbrd = BAUD_312500_FBRD;
                        break;
                    default:
                        break; // Invalid value, ignore
                }
                DL_UART_Main_setBaudRateDivisor(UART0, ibrd, fbrd);
                break;
                
            case SC16IS740_REG_RHR_THR:
                // transmitter is already enabled earlier
                DL_UART_Main_transmitDataBlocking(UART0, data);
                break;
            
            case SC16IS740_REG_IER:
                sc16is740_ier = data;
                break;
            case SC16IS740_REG_LCR:
                sc16is740_lcr = data;
                break;
            case SC16IS740_REG_IIR_FCR:
                // Only support RX FIFO reset (bit 1) for now
                if (data & (1 << 1)) {
                     __disable_irq();
                    uart_rx_tail = uart_rx_head; // Clear RX buffer
                    __enable_irq();
                }
                break;
            case SC16IS740_REG_MCR:
                sc16is740_mcr = data;
                // RTS control (Bit 1 in MCR)
                // PA15 is used for RTS. Typically active low in UART, 
                // but for RS-485 DE it might be active high. 
                // We'll follow the MCR bit state for now.
                if (data & (1 << 1)) {
                    DL_GPIO_setPins(GPIO_485_GRP_PORT, GPIO_485_GRP_PIN_485_ENABLE_PIN);
                } else {
                    DL_GPIO_clearPins(GPIO_485_GRP_PORT, GPIO_485_GRP_PIN_485_ENABLE_PIN);
                }
                break;
            case SC16IS740_REG_SPR:
                sc16is740_spr = data;
                break;
            default:
                break;
        }
        // Note: SC16IS740 supports auto-increment for burst writes, 
        // but for simplicity we'll assume single register writes or 
        // burst writes to THR for now.
    }
    if (sc16is740_regFilePtr == SC16IS740_REG_RHR_THR) {
        /* Enable EOT interrupt only after data has been queued.
        * Otherwise we can get a stale EOT from a previous transmission,
        * or miss the EOT if transmission completed before interrupt enable.
        */
        if (DL_UART_Main_isBusy(UART0)) {
            DL_UART_Main_enableInterrupt(UART0, DL_UART_MAIN_INTERRUPT_EOT_DONE);
        } else {
            if ((sc16is740_mcr & (1 << 1)) == 0) {
                rs485_disable_tx();
            }
            rs485_auto_tx_active = false;
        }
    }
}

// this is called when the master reads from the I2C target (i.e. reads from SC16IS740 registers)
void sc16is740_i2c_tx_callback(void)
{
    uint8_t data = 0;

    switch (sc16is740_regFilePtr) {
        case SC16IS740_REG_RHR_THR:
            // Read from UART RX buffer
            if (uart_rx_head != uart_rx_tail) {
                data = uart_rx_buf[uart_rx_tail];
                uart_rx_tail = (uart_rx_tail + 1) % UART_RX_BUF_SIZE;
            } else {
                data = 0x00; // Buffer empty
            }
            break;
        case SC16IS740_REG_LSR:
            // Report status: THR always empty for now (since we write directly),
            // and check if RX buffer has data.
            data = SC16IS740_LSR_THRE | SC16IS740_LSR_TEMT;
            if (uart_rx_head != uart_rx_tail) {
                data |= SC16IS740_LSR_DR;
            }
            break;
        case SC16IS740_REG_IER:
            data = sc16is740_ier;
            break;
        case SC16IS740_REG_LCR:
            data = sc16is740_lcr;
            break;
        case SC16IS740_REG_MCR:
            data = sc16is740_mcr;
            break;
        case SC16IS740_REG_SPR:
            data = sc16is740_spr;
            break;
        default:
            data = 0xFF;
            break;
    }

    I2CTarDriver_write(sc16is740_i2c_handle, data);
}
