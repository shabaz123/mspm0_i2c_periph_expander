# MSPM0 Peripheral Expander ("Processor Companion")

This project is partially based on https://github.com/beagleboard/mspm0-adc-eeprom.
The project implements a multi-device I2C target (slave) using the TI MSPM0L1105 microcontroller. It allows a host controller (such as a Linux single-board computer or another microcontroller) to interface (via I2C) with the MSPM0 and gain new features.

## I2C Addresses
The MSPM0 will respond to a master I2C controller on the following 7-bit I2C base addresses:
UART Emulation feature: 0x48
All other peripherals: 0x20
The actual I2C addresses can be modified, by setting A0 and A1 address pins. Thus, I2C addresses within the range 0x48-0x4b and 0x20-0x23 are feasible. This allows up to 4 devices to be attached to the same I2C bus.

Detailed information on the specific I2C registers can be found in the [User Guide](User_Guide.md).

To see examples of the I2C registers being used, see the Python test harness code.

## Features

- **I2C UART Emulation**: Loosely based on **16550** and **SC16IS740** UART peripherals.
  - Additionally, usable for **RS-485** communication with hardware direction control.
  - **Baud Rate**: Pre-configured to 9600, adjustable with a register setting.
- **I2C ADC Emulation**: Loosely based on **AD7291** 12-bit ADC, with 3 channels available.
- **AC RMS Measurement**: AC RMS, Peak-to-Peak, DC level and Frequency measurement analog input.
- **PWM/Frequency Capture**: Period (1/Freq) and Duty Cycle measurement digital input.

## Hardware Mapping (Physical package is TI 28-pin DGS)

- **UART0_RXD**: PA9 (PinCM 10) Physical Pin 14
- **UART0_TXD**: PA17 (PinCM 18) Physical Pin 20
- **RS-485 RTS (Direction)**: PA15 (PinCM 16) Physical Pin 18 - Automatically controlled, can be manually controlled via the emulated MCR register.
- **ADC Chan 0**: PA27/A0 Physical Pin 2
- **ADC Chan 1**: PA26/A1 Physical Pin 1
- **ADC Chan 2**: PA25/A2 Physical Pin 28
- **AC RMS Chan**: PA24/A3 Physical Pin 27
- **Period/Duty Capture**: PA10/TIMG4_C0 Physical Pin 15
- **I2C SDA**: PA0 (PinCM 1) Physical Pin 4
- **I2C SCL**: PA1 (PinCM 2) Physical Pin 5
- **Address Select 0**: PA14 Physical Pin 17
- **Address Select 1**: PA16 Physical Pin 9


## Building the Project

The project can be built using the GNU ARM Toolchain.

The TI MSPM0 SDK needs to be installed.

Note: You may need to edit the Makefile, or Environment variables. Setting up the environment is beyond the scope of this README. The steps below assume Windows 11 and specific hard-coded settings, so examine the Makefile first.

```powershell
# Clean previous build artifacts
make clean

# Build the project
make
```

The build process produces `mspm0_i2c_periph_expander.hex`, `.out`, and `.map` files in the root directory.

## Programming the Flash
There are several ways to do this; I used the flash.ps1 script with Windows 11, and a CMSIS-DAP debug probe attached to the SWD pins on the microcontroller.

```powershell
# Flash the microcontroller
.\flash.ps1

# Recover a microcontroller that won't program
# Try holding down RESET, and possibly the BOOT button too, and while holding down, type:
.\flash.ps1 recover
```

## Running Tests
End-to-end testing can be performed by attaching a Pi Pico to the I2C and UART connections. The Pi Pico needs to run software that turns the Pico into a USB-to-UART and USB-to-I2C adapter. The Pico software is here: https://github.com/shabaz123/ez_i2c_adapter_and_uart_bridge 

To run the end-to-end tests, go to the test_harness folder and type:

```powershell
# Run Tests via I2C, with user prompts for UART input via user-provided terminal
python .\expander_test.py

# Run Tests end-to-end with I2C and UART controlled by the Python script
python .\expander_test.py --uart-test-port COM3
```


## Credits

This project is partially based on the [beagleboard/mspm0-adc-eeprom](https://github.com/beagleboard/mspm0-adc-eeprom) project.
