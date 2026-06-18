# Test harness for an I2C to UART expander, using the Pi Pico I2C EasyAdapter
# Setup: PC --> USB Cable --> Pi Pico (running EasyAdapter firmware) --> I2C to UART expander

import argparse
import easyadapter as ea
import os
import sys
import time # Import time for delays/polling

try:
    import serial
except ImportError:
    serial = None


run_all_test = False  # Set to False to run only test101 in a loop for debugging

# --- Globals ---
# Emulates an SC16IS740 I2C to UART expander (approximately)
# Emulates an AD7291 I2C ADC (approximately)
ADDR0 = 0
ADDR1 = 0
addrSel = (ADDR1 << 1) | ADDR0  # Calculate address selection bits based on ADDR1 and ADDR0
EXPANDER_I2C_ADDRESS = 0x48 + addrSel  # Base address 0x48 plus address selection bits
EXPANDER_I2C_ADC_ADDRESS = 0x20 + addrSel  # Base address 0x20 for ADC plus address selection bits

# SC16IS740 Register Addresses
MCR_REG = 0x20
THR_REG = 0x00
LSR_REG = 0x28
RHR_REG = 0x00
FCR_REG = 0x10
RSV_REG = 0x68

# AD7291 Register Addresses
ADC_COMMAND_REG = 0x00
ADC_RESULT_REG = 0x01
CAPTURE_PERIOD_REG = 0x40
CAPTURE_DUTY_X10_REG = 0x41
ACMEAS_START_REG = 0x50
ACMEAS_RESULT_REG = 0x51

ADC_SUPPLY_VOLTAGE = 3.3  # ADC reference voltage for converting raw values to volts
CAPTURE_CLOCK_HZ = 4_000_000  # 32 MHz / 8 (prescaler) = 4 MHz

DO_MANUAL_485_ENABLE = False  # Set to True if you want to manually control the RS485 transmit enable pin in the tests

# colors and reset for terminal output
BLACK   = "\033[30m"
RED     = "\033[31m"
GREEN   = "\033[32m"
YELLOW  = "\033[33m"
BLUE    = "\033[34m"
MAGENTA = "\033[35m"
CYAN    = "\033[36m"
WHITE   = "\033[37m"
BRIGHT_BLUE = "\033[94m"
BRIGHT_GREEN = "\033[92m"
BRIGHT_YELLOW = "\033[93m"
BRIGHT_CYAN = "\033[96m"
RESET   = "\033[0m"

RMS_PP_CONV = 0.35355339   # 1 / sqrt(2) / 2, used to convert RMS to peak-to-peak for a sine wave

adapter = ea.EasyAdapter()
uart_test_serial = None

total_tests = 0
successful_tests = 0

def prompt_if_manual(message):
    """Only pause for user input when the automated UART test port is not in use."""
    if uart_test_serial is None:
        input(message)
    else:
        print(f"{message} [auto UART mode: continuing]")

def uart_test_read(expected, timeout=3):
    """Read expected bytes from the PC UART adapter and compare them."""
    if uart_test_serial is None:
        return None
    expected_bytes = expected.encode() if isinstance(expected, str) else bytes(expected)
    deadline = time.time() + timeout
    received = bytearray()
    while len(received) < len(expected_bytes) and time.time() < deadline:
        chunk = uart_test_serial.read(len(expected_bytes) - len(received))
        if chunk:
            received.extend(chunk)
    print(f"UART monitor expected: {expected_bytes!r}, received: {bytes(received)!r}")
    return bytes(received) == expected_bytes

def uart_test_write(data, settle_time=0.1):
    """Write bytes/string from the PC UART adapter to the DUT UART input."""
    if uart_test_serial is None:
        return
    data_bytes = data.encode() if isinstance(data, str) else bytes(data)
    print(f"UART injector writing: {data_bytes!r}")
    uart_test_serial.write(data_bytes)
    uart_test_serial.flush()
    time.sleep(settle_time)

# --- Timer Capture Functions ---
def capture_period():
    """
    Capture the period of a signal on the expander's timer input pin.
    """
    global adapter
    adapter.i2c_write(EXPANDER_I2C_ADC_ADDRESS, CAPTURE_PERIOD_REG, [])
    data = adapter.i2c_read(EXPANDER_I2C_ADC_ADDRESS, 2)
    if data is not None and len(data) == 2:
        period_counts = (data[0] << 8) | data[1]
        if period_counts == 0xFFFF:
            return None
        return (period_counts * 1000.0) / CAPTURE_CLOCK_HZ
    print("Failed to read CAPTURE_PERIOD_REG!")
    return None

def capture_duty_cycle():
    global adapter
    adapter.i2c_write(EXPANDER_I2C_ADC_ADDRESS, CAPTURE_DUTY_X10_REG, [])
    data = adapter.i2c_read(EXPANDER_I2C_ADC_ADDRESS, 2)

    if data is not None and len(data) == 2:
        duty_cycle_x10 = (data[0] << 8) | data[1]

        if duty_cycle_x10 == 0xFFFF:
            return None

        return duty_cycle_x10 / 10.0

    print("Failed to read CAPTURE_DUTY_X10_REG!")
    return None

# --- ADC Functions ---
def adc_enable_all_channels():
    """
    Enable channels 0-2 only, by writing binary 0b11100000 (0xE0) and a command low byte of 0x00
    to the ADC's channel command register.
    """
    global adapter
    adapter.i2c_write(EXPANDER_I2C_ADC_ADDRESS, ADC_COMMAND_REG, [0xE0, 0x00])

def adc_read_all_channels():
    """
    Read the ADC values for channels 0-2 by reading from the ADC result register (two bytes per channel).
    The first byte contains the channel number in the upper 4 bits and the high 4 bits of the ADC value, 
    while the second byte contains the low 8 bits of the ADC value (i.e. a 12-bit value across the two bytes).
    The function returns a list of 3 values corresponding to the 3 channels.
    Note: The values may be read in a different sequence than the channel numbers, so the function should 
    parse the channel number from the first byte to determine which channel's value is being read.
    """
    global adapter
    channel_values = [None, None, None]  # Initialize list for 3 channels
    adapter.i2c_write(EXPANDER_I2C_ADC_ADDRESS, ADC_RESULT_REG, [])

    for _ in range(3):
        data = adapter.i2c_read(EXPANDER_I2C_ADC_ADDRESS, 2)

        if data is not None and len(data) == 2:
            channel_num = (data[0] >> 4) & 0x0F
            adc_value = ((data[0] & 0x0F) << 8) | data[1]

            if channel_num < 3:
                channel_values[channel_num] = adc_value

    return channel_values


def acmeas_start():
    """Start an AC measurement run on ADC channel 0."""
    global adapter
    adapter.i2c_write(EXPANDER_I2C_ADC_ADDRESS, ACMEAS_START_REG, [])


def acmeas_read_results():
    """
    Read AC measurement results.

    Firmware returns 8 bytes:
      RMS counts, peak-to-peak counts, DC level, frequency Hz
    all as big-endian uint16 values.
    """
    global adapter
    adapter.i2c_write(EXPANDER_I2C_ADC_ADDRESS, ACMEAS_RESULT_REG, [])
    data = adapter.i2c_read(EXPANDER_I2C_ADC_ADDRESS, 8)

    if data is None or len(data) != 8:
        print("Failed to read ACMEAS_RESULT_REG!")
        return None

    rms_counts = (data[0] << 8) | data[1]
    pp_counts = (data[2] << 8) | data[3]
    dc_level = (data[4] << 8) | data[5]
    freq_hz = (data[6] << 8) | data[7]

    # debug, print in hex each byte
    print(f"ACMEAS_RESULT_REG raw bytes: {[f'0x{b:02X}' for b in data]}")

    return rms_counts, pp_counts, dc_level, freq_hz


# --- RS485 / UART Functions ---
def enable_rs485_transmit():
    """
    Enable RS485 transmit mode by writing to the Modem Control Register (MCR).
    This typically involves setting the appropriate bits to enable the driver.
    """
    global adapter
    reg_value = 0x02  #  binary 0000 0010
    adapter.i2c_write(EXPANDER_I2C_ADDRESS, MCR_REG, [reg_value])

def disable_rs485_transmit():
    """
    Disable RS485 transmit mode by writing 0x00 to the Modem Control Register (MCR).
    """
    global adapter
    reg_value = 0x00
    adapter.i2c_write(EXPANDER_I2C_ADDRESS, MCR_REG, [reg_value])

def rs485_set_baud(baud_rate):
    """
    A reserved register (RSV_REG) is used to set the baud rate for RS485 communication.
    The acceptable values for the register are 0-5
    """
    global adapter
    baud_rate_map = {
        9600: 0x00,
        115200: 0x01,
        19200: 0x02,
        38400: 0x03,
        57600: 0x04,
        31250: 0x05
    }
    reg_value = baud_rate_map.get(baud_rate)
    if reg_value is not None:
        adapter.i2c_write(EXPANDER_I2C_ADDRESS, RSV_REG, [reg_value])
    else:
        print(f"Unsupported baud rate: {baud_rate}. Supported rates are: {list(baud_rate_map.keys())}")

def rs485_flush():
    """
    Flush the expander's UART receive buffer, by writing 0x02 to the FCR register.
    """
    global adapter
    adapter.i2c_write(EXPANDER_I2C_ADDRESS, FCR_REG, [0x02])

def rs485_read(maxcount=1, first_timeout=3, subsequent_timeout=0.5):
    global adapter
    data_read = []

    def read_one_byte(timeout):
        start_time = time.time()

        while time.time() - start_time < timeout:
            adapter.i2c_write(EXPANDER_I2C_ADDRESS, LSR_REG, [])
            lsr_value = adapter.i2c_read(EXPANDER_I2C_ADDRESS, 1)

            if lsr_value is None:
                print("Failed to read LSR")
                time.sleep(0.01)
                continue

            if lsr_value[0] & 0x01:
                adapter.i2c_write(EXPANDER_I2C_ADDRESS, RHR_REG, [])
                data_byte = adapter.i2c_read(EXPANDER_I2C_ADDRESS, 1)

                if data_byte is not None:
                    print(f"Read byte: {data_byte[0]:02X}")
                    return data_byte[0]

            time.sleep(0.01)

        return None

    # First byte gets first_timeout
    byte = read_one_byte(first_timeout)
    if byte is None:
        return data_read

    data_read.append(byte)

    # Each subsequent byte gets its own subsequent_timeout
    while len(data_read) < maxcount:
        byte = read_one_byte(subsequent_timeout)
        if byte is None:
            break
        data_read.append(byte)

    return data_read

def rs485_write(data):
    """
    Write data to the expander's Transmit Holding Register (THR) for RS485 transmission.
    This function assumes that the transmit mode has already been enabled.
    """
    global adapter
    # Ensure data is in byte format and does not exceed the register size
    if isinstance(data, str):
        data = data.encode()  # Convert string to bytes
    elif isinstance(data, list):
        data = bytes(data)  # Convert list of integers to bytes
    # Write data to the THR register
    adapter.i2c_write(EXPANDER_I2C_ADDRESS, THR_REG, list(data))

def rs485_wait_tx_complete(timeout=1):
    """
    Wait for the RS485 transmission to complete by polling the Line Status Register (LSR).
    The function checks bit 6 to determine if the transmission is complete.
    """
    global adapter
    start_time = time.time()
    while time.time() - start_time < timeout:
        # first write the register address to read from, then read the value
        adapter.i2c_write(EXPANDER_I2C_ADDRESS, LSR_REG, [])
        lsr_value = adapter.i2c_read(EXPANDER_I2C_ADDRESS, 1)
        if lsr_value is None:
            print("Failed to read LSR")
            time.sleep(0.1)
            continue
        # print the result as hex for debugging
        print(f"LSR value: {lsr_value[0]:02X}")
        if lsr_value[0] & 0x40:
            return True
        time.sleep(0.1)  # Poll every 100ms
    return False  # Timeout occurred

# --- Test Cases ---
def test1():
    """
    Test 1: Basic RS485 Write Test
    - Write the sequence to send a set of bytes over RS485
    """
    global adapter, uart_test_serial
    test_status = False
    prompt_if_manual("Press Enter to start Test 1: Basic Write Test")
    if uart_test_serial is not None:
        uart_test_serial.reset_input_buffer()
    # Example byte sequence to write (replace with actual data for your expander)
    data_to_write = [0x31, 0x32, 0x33, 0x34]  # ASCII for '1234'
    if DO_MANUAL_485_ENABLE:
        print(f"Enabling RS485 transmit mode...")
        enable_rs485_transmit()
    print(f"Writing data to RS485: {data_to_write}")
    rs485_write(data_to_write)
    print("Waiting for transmission to complete...")
    if rs485_wait_tx_complete():
        print("Transmission completed successfully.")
        if uart_test_serial is not None:
            test_status = uart_test_read("1234", timeout=3)
        else:
            test_status = True
    else:
        print("Transmission did not complete within the timeout period.")
    if DO_MANUAL_485_ENABLE:
        print("Disabling RS485 transmit mode...")
        disable_rs485_transmit()
    print(f"Test 1 Result: {'PASS' if test_status else 'FAIL'}")
    return test_status

def test2():
    """
    Test 2: Basic RS485 Read Test
    - This test will wait for data to be received on the RS485 line and read it back.
    - It will print the received data in hex format.
    """
    global adapter, uart_test_serial
    test_status = False
    prompt_if_manual("Press Enter to start Test 2: Basic Read Test")
    rs485_flush()
    uart_test_write("hello")
    print("Waiting for data to be received on RS485...")
    received_data = rs485_read(maxcount=8, first_timeout=5, subsequent_timeout=1)
    if received_data:
        # print in hex
        print("Received data:")
        for byte in received_data:
            ascii_char = chr(byte) if 32 <= byte <= 126 else '.'  # Printable ASCII range
            print(f"0x{byte:02X} ({ascii_char})")
        test_status = True
    else:
        print("No data received within the timeout period.")
    print(f"Test 2 Result: {'PASS' if test_status else 'FAIL'}")
    return test_status
    
def test3():
    """
    Test 3: RS485 Flush Test
    - This test will flush the UART receive buffer and then attempt to read data, which should not be present.
    """
    global adapter, uart_test_serial
    test_status = False
    prompt_if_manual("Press Enter to start Test 3: RS485 Flush Test")
    if uart_test_serial is None:
        input("Type some characters via the UART, then press Enter")
    else:
        uart_test_write("boo")
    print("Reading just one character to confirm data is present before flush...")
    pre_flush_data = rs485_read(maxcount=1, first_timeout=1, subsequent_timeout=1)
    if uart_test_serial is not None:
        if pre_flush_data:
            if pre_flush_data == [ord('b')]:
                print(f"Received expected data {pre_flush_data} before flush.")
            else:
                print(f"Received unexpected data {pre_flush_data} before flush.")
        else:
            print("No data received before flush, which is unexpected.")
            # flush and abort the test since we cannot confirm the flush behavior
            rs485_flush()
            print(f"Test 3 Result: FAIL (no data to flush)")
            return test_status
    print("Flushing the UART receive buffer...")
    rs485_flush()
    print("Attempting to read data after flush (should be none)...")
    received_data = rs485_read(maxcount=8, first_timeout=1, subsequent_timeout=1)
    if not received_data:
        print("No data received after flush, as expected.")
        test_status = True
    else:
        print("Data was received after flush, which is unexpected:")
        for byte in received_data:
            ascii_char = chr(byte) if 32 <= byte <= 126 else '.'  # Printable ASCII range
            print(f"0x{byte:02X} ({ascii_char})")
    print(f"Test 3 Result: {'PASS' if test_status else 'FAIL'}")
    return test_status

def test100():
    """
    Test 100: ADC Read Test
    - This test will enable the ADC channels and read their values, printing the results.
    """
    global adapter
    test_status = False
    prompt_if_manual("Press Enter to start Test 100: ADC Read Test")
    print("Enabling ADC channels...")
    adc_enable_all_channels()
    print("Reading ADC values for channels 0-2...")
    channel_values = adc_read_all_channels()
    if all(value is not None for value in channel_values):
        for i, value in enumerate(channel_values):
            print(f"Channel {i}: {value} (approximately {(value / 4095) * ADC_SUPPLY_VOLTAGE:.2f} V)")
        # expected values (+- 2%) are: 1024, 2048, 3072 for channels 0, 1, 2 respectively
        expected_values = [1024, 2048, 3072]
        tolerance = 0.02  # 2% tolerance
        # display the error and check if within tolerance
        for i, (value, expected) in enumerate(zip(channel_values, expected_values)):
            error = abs(value - expected) / expected
            print(f"Channel {i}: Value = {value}, Expected = {expected}, Error = {error:.2%}")
            if error > tolerance:
                print(f"Channel {i} value is out of tolerance!")
                test_status = False
                break
        else:
            test_status = True
    else:
        print("Failed to read all ADC channel values.")
        # Print which channels were read successfully
        for i, value in enumerate(channel_values):
            if value is not None:
                print(f"Channel {i}: {value} (approximately {(value / 4095) * ADC_SUPPLY_VOLTAGE:.2f} V)")
            else:
                print(f"Channel {i}: Failed to read value")
    print(f"Test 100 Result: {'PASS' if test_status else 'FAIL'}")
    return test_status


def test101():
    """
    Test 101: AC Measurement Test
    - Issues ACMEAS_START_REG to start a channel-3 AC measurement.
    - Waits 1.5 seconds for the firmware to collect samples.
    - Reads ACMEAS_RESULT_REG and prints RMS, p-p, DC level, and frequency.
    """
    global adapter
    test_status = False
    prompt_if_manual("Press Enter to start Test 101: AC Measurement Test")

    print("Starting AC meas on ADC ch 3...", end="", flush=True)
    acmeas_start()

    print("Wait 1.5 sec to complete...", end="", flush=True)
    time.sleep(1.5)
    print("done.")

    result = acmeas_read_results()

    if result is None:
        print("Failed to read AC measurement results.")
    else:
        rms_counts, pp_counts, dc_level, freq_hz = result
        rms_volts = (rms_counts / 16384) * ADC_SUPPLY_VOLTAGE
        derived_vpp_volts = rms_volts / RMS_PP_CONV  # Convert RMS to peak-to-peak for a sine wave
        pp_volts = (pp_counts / 16384) * ADC_SUPPLY_VOLTAGE
        dc_level_volts = (dc_level / 16384) * ADC_SUPPLY_VOLTAGE
        # print(f"{RED}AC RMS: [{rms_counts}] {rms_volts:.3f}V", end=", ")
        print(f"{YELLOW}AC mV RMS: [{rms_counts}] {rms_volts*1000:.1f} mV (sine {derived_vpp_volts*1000:.1f} mVp-p)", end=", ")
        print(f"{GREEN}p-p: [{pp_counts}] {pp_volts:.3f}V", end=", ")
        print(f"{BLUE}Offset:  [{dc_level}] {dc_level_volts:.3f}V", end=", ")
        print(f"{MAGENTA}Freq: {freq_hz} Hz{RESET}")

        # Treat an all-zero response as not-ready/fail, matching the firmware behavior.
        if rms_counts == 0 and pp_counts == 0 and dc_level == 0 and freq_hz == 0:
            print("AC measurement returned all zeros; measurement may not be complete or no valid signal was detected.")
            test_status = False
        else:
            test_status = True

    print(f"Test 101 Result: {'PASS' if test_status else 'FAIL'}")
    return test_status


def test200():
    """
    Test 200: Timer Capture Test
    - This test will capture the period and duty cycle of a signal on the expander's timer input pin.
    - It will print the captured values.
    """
    global adapter
    test_status = True
    test_length = 2  # seconds
    print("For Test 200, connect a signal generator to the expander's timer input pin.")
    print(f"This test will run for {test_length} seconds.")
    prompt_if_manual("Press Enter to start Test 200: Timer Capture Test")
    start_time = time.time()
    while time.time() - start_time < test_length:
        period = capture_period()
        duty_cycle = capture_duty_cycle()
        if period is not None and duty_cycle is not None:
            frequency_khz = 1.0 / period if period > 0 else 0
            print(f"Frequency: {frequency_khz:.3f} kHz, Period: {period:.6f} ms, Duty Cycle: {duty_cycle:.1f}%")
        else:
            if period is None:
                print("Failed to capture period.")
                test_status = False
            elif duty_cycle is None:
                frequency_khz = 1.0 / period if period > 0 else 0
                print(f"Frequency: {frequency_khz:.3f} kHz, Period: {period:.6f} ms, Duty Cycle: *** OOR ***")
            else:
                frequency_khz = 1.0 / period if period > 0 else 0
                print(f"Frequency: {frequency_khz:.3f} kHz, Period: {period:.6f} ms, Duty Cycle: {duty_cycle:.1f}%")
        time.sleep(0.1)  # Capture every 100ms
    print(f"Test 200 Result: {'PASS' if test_status else 'FAIL'}")
    return test_status

# --- main function ---
def parse_args():
    parser = argparse.ArgumentParser(description="Test harness for an I2C to UART expander")
    parser.add_argument(
        "legacy_uart_test_port",
        nargs="*",
        help="Optional legacy form: uart_test_port COM3",
    )
    parser.add_argument(
        "--uart-test-port",
        dest="uart_test_port",
        help="PC serial port connected to the DUT UART, for example COM3 or /dev/ttyUSB0",
    )
    parser.add_argument(
        "--uart-test-baud",
        type=int,
        default=115200,
        help="Baud rate for the PC UART adapter. Default: 115200",
    )
    args = parser.parse_args()

    # Also accept the suggested command style: python expander_test.py uart_test_port COM3
    if args.legacy_uart_test_port:
        if len(args.legacy_uart_test_port) == 2 and args.legacy_uart_test_port[0] == "uart_test_port":
            args.uart_test_port = args.legacy_uart_test_port[1]
        else:
            parser.error("Use either --uart-test-port COM3 or uart_test_port COM3")

    return args

def open_uart_test_port(port, baud):
    if not port:
        return None
    if serial is None:
        raise RuntimeError("pyserial is not installed. Install it with: pip install pyserial")
    ser = serial.Serial(port=port, baudrate=baud, timeout=0.05, write_timeout=1)
    ser.reset_input_buffer()
    ser.reset_output_buffer()
    print(f"Opened UART test port {port} at {baud} baud.")
    return ser

def main():
    """
    Main function to read .hex files from the input directory,
    identify devices, and write data to them via EasyAdapter using page writes.
    """
    global adapter, uart_test_serial, total_tests, successful_tests
    total_tests = 0
    successful_tests = 0
    args = parse_args()
    try:
        uart_test_serial = open_uart_test_port(args.uart_test_port, args.uart_test_baud)
    except Exception as exc:
        print(f"Failed to open UART test port: {exc}")
        return

    print("Initializing EasyAdapter...")
    init_result = adapter.init(0) 
    if init_result:
        print("EasyAdapter initialized successfully.")
    else:
        print("Failed to initialize EasyAdapter. Please check your connection and drivers.")
        print("Cannot proceed with writing data without a connected adapter.")
        return 
    # print addrSel and the calculated I2C addresses for debugging
    print(f"Address Selection Bits (addrSel): {addrSel:02b}")
    print(f"Using I2C address for UART: 0x{EXPANDER_I2C_ADDRESS:02X}")
    print(f"Using I2C address for ADC: 0x{EXPANDER_I2C_ADC_ADDRESS:02X}")
    # set 115200 baud for RS485 communication
    print("Setting RS485 baud rate to 115200...")
    rs485_set_baud(115200)
    if run_all_test:
        # Run Test 200: Timer Capture Test
        if test200():
            successful_tests += 1
        total_tests += 1
        # Run Test 101: AC Measurement Test
        print("NOTE: First freq result can be inaccurate due to no dc_level established yet.")
        if test101():
            successful_tests += 1
        total_tests += 1
        # Run Test 100: ADC Read Test
        if test100():
            successful_tests += 1
        total_tests += 1
        # Run Test 1: Basic RS485 Write Test
        if test1():
            successful_tests += 1
        total_tests += 1
        # Run Test 2: Basic RS485 Read Test
        if test2():
            successful_tests += 1
        total_tests += 1
        # Run Test 3: RS485 Flush Test
        if test3():
            successful_tests += 1
        total_tests += 1
    else:
        print("NOTE: First freq result can be inaccurate due to no dc_level established yet.")
        for i in range(0,6):
            if test101():
                successful_tests += 1
            total_tests += 1

    if uart_test_serial is not None:
        uart_test_serial.close()
        uart_test_serial = None

    print(f"Total Tests: {total_tests}, Successful Tests: {successful_tests}")
    if successful_tests == total_tests:
        print("All tests passed successfully.")
    else:
        print(f"*** ERROR - {total_tests - successful_tests} TESTS FAILED ***")

if __name__ == "__main__":
    main()