# MSPM0 Peripheral Expander User Guide

The MSPM0 Peripheral Expander is a multi-function I2C target (slave) device that provides UART emulation, ADC channels, AC RMS measurement, and PWM/Frequency capture capabilities.

## I2C Addressing

The device responds to two distinct 7-bit I2C base addresses. The actual addresses depend on the hardware configuration of the **A0** (PA14) and **A1** (PA16) address pins.

| Feature Group | Base Address (A1=0, A0=0) | Address Range |
| :--- | :--- | :--- |
| **UART Emulation** | `0x48` | `0x48` - `0x4b` |
| **ADC / AC RMS / PWM Capture** | `0x20` | `0x20` - `0x23` |

The 7-bit address is calculated as: `Base Address + (A1 << 1) | A0`.

---

## Peripheral 1: UART Emulation (Base Address 0x48)

This peripheral emulates a subset of the SC16IS740 UART-to-I2C expander.

### Register Table

| Register | Address | Access | Description |
| :--- | :--- | :--- | :--- |
| **THR** | `0x00` | Write | Transmit Holding Register (Data to send) |
| **RHR** | `0x00` | Read | Receive Holding Register (Data received) |
| **FCR** | `0x10` | Write | FIFO Control Register (Buffer management) |
| **MCR** | `0x20` | Write | Modem Control Register (RS-485 control) |
| **LSR** | `0x28` | Read | Line Status Register (Status flags) |
| **RSV** | `0x68` | Write | Reserved Register (Baud rate selection) |

### Register Details

#### THR / RHR (0x00)
- **Write (THR)**: Send one or more bytes to the UART transmit buffer.
- **Read (RHR)**: Read one byte from the UART receive buffer. Before reading, check the **LSR** register to ensure data is available.

#### FCR (0x10)
- **Write**: Used to flush the receive buffer.
  - Write `0x02` to clear the UART receive FIFO.

#### MCR (0x20)
- **Write**: Controls the RS-485 Transmit Enable (RTS) pin.
  - Write `0x02` to enable manual RS-485 transmit mode.
  - Write `0x00` to disable/return to default.
  *Note: Manual control via MCR is only required if specific timing or slower control of the RS-485 transmit enable pin is needed. By default, the firmware automatically controls this pin during UART transmissions.*

#### LSR (0x28)
- **Read**: Returns a 1-byte status bitmask.
  - **Bit 0 (0x01)**: Data Ready. Set when data is available in the RHR.
  - **Bit 6 (0x40)**: Transmitter Empty. Set when all data in the THR has been sent.

#### RSV (0x68)
- **Write**: Selects the UART baud rate.
  - `0x00`: 9600
  - `0x01`: 115200 (Default)
  - `0x02`: 19200
  - `0x03`: 38400
  - `0x04`: 57600
  - `0x05`: 31250

---

## Peripheral 2: ADC, AC RMS & Capture (Base Address 0x20)

This peripheral provides analog measurements and digital signal capture.

### Register Table

| Register | Address | Access | Description |
| :--- | :--- | :--- | :--- |
| **ADC_COMMAND** | `0x00` | Write | Enable ADC channels |
| **ADC_RESULT** | `0x01` | Read | Read 12-bit ADC values |
| **CAPTURE_PERIOD** | `0x40` | Read | PWM/Frequency Period |
| **CAPTURE_DUTY** | `0x41` | Read | PWM Duty Cycle (x10) |
| **ACMEAS_START** | `0x50` | Write | Start AC RMS Measurement |
| **ACMEAS_RESULT** | `0x51` | Read | Read AC RMS Results |

### Register Details

#### ADC_COMMAND (0x00)
- **Write**: Enable specific ADC channels.
  - Write `[0xE0, 0x00]` to enable channels 0, 1, and 2.

#### ADC_RESULT (0x01)
- **Read**: Returns 2 bytes per channel requested.
  - **Byte 1**: `[Channel Num (4 bits)][ADC High (4 bits)]`
  - **Byte 2**: `[ADC Low (8 bits)]`
  - Value: 12-bit unsigned integer (0-4095).
  - Formula: `Voltage = (Value / 4095) * VREF` (Typically 3.3V).

#### CAPTURE_PERIOD (0x40)
- **Read**: Returns 2 bytes (Big Endian) representing the signal period.
  - **Value**: 16-bit clock counts.
  - **0xFFFF**: Indicates no signal or out of range.
  - **Formula**: `Period (ms) = (Value * 1000) / 4,000,000` (based on 4MHz capture clock).

#### CAPTURE_DUTY (0x41)
- **Read**: Returns 2 bytes (Big Endian) representing the duty cycle.
  - **Value**: 16-bit integer (Duty Cycle * 10).
  - **0xFFFF**: Indicates no signal or out of range.
  - **Formula**: `Duty Cycle (%) = Value / 10.0`.

#### ACMEAS_START (0x50)
- **Write**: Trigger a new AC measurement on the dedicated AC RMS channel (ADC Channel 3).
  - Send the register address with no additional data to start.
  - Allow ~1.5 seconds for sampling to complete before reading results.

#### ACMEAS_RESULT (0x51)
- **Read**: Returns 8 bytes (Big Endian) of measurement data.
  - **Bytes 0-1**: RMS counts (12-bit scale).
  - **Bytes 2-3**: Peak-to-Peak counts (12-bit scale).
  - **Bytes 4-5**: DC Level counts (12-bit scale).
  - **Bytes 6-7**: Frequency in Hz.
  - *Note: All counts should be converted using the same 12-bit ADC formula as regular channels.*
