# Test harness for an I2C to UART expander, using the Pi Pico I2C EasyAdapter
# Setup: PC --> USB Cable --> Pi Pico (running EasyAdapter firmware) --> I2C to UART expander

import easyadapter as ea
import os
import time # Import time for delays/polling

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

ADC_SUPPLY_VOLTAGE = 3.3  # ADC reference voltage for converting raw values to volts
CAPTURE_CLOCK_HZ = 4_000_000  # 32 MHz / 8 (prescaler) = 4 MHz

DO_MANUAL_485_ENABLE = False  # Set to True if you want to manually control the RS485 transmit enable pin in the tests

adapter = ea.EasyAdapter()
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
    global adapter
    test_status = False
    input("Press Enter to start Test 1: Basic Write Test")
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
        test_status = True
    else:
        print("Transmission did not complete within the timeout period.")
    if DO_MANUAL_485_ENABLE:
        print("Disabling RS485 transmit mode...")
        disable_rs485_transmit()
    print(f"Test 1 Result: {'PASS (But check the UART output to confirm!)' if test_status else 'FAIL'}")

def test2():
    """
    Test 2: Basic RS485 Read Test
    - This test will wait for data to be received on the RS485 line and read it back.
    - It will print the received data in hex format.
    """
    global adapter
    test_status = False
    input("Press Enter to start Test 2: Basic Read Test")
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
    print(f"Test 2 Result: {'PASS (But check the UART output to confirm!)' if test_status else 'FAIL'}")

def test3():
    """
    Test 3: RS485 Flush Test
    - This test will flush the UART receive buffer and then attempt to read data, which should not be present.
    """
    global adapter
    test_status = False
    input("Press Enter to start Test 3: RS485 Flush Test")
    input("Type some characters via the UART, then press Enter")
    print("Reading just one character to confirm data is present before flush...")
    pre_flush_data = rs485_read(maxcount=1, first_timeout=1, subsequent_timeout=1)
    print("Flushing the UART receive buffer...")
    rs485_flush()
    print("Attempting to read data after flush (should be none)...")
    received_data = rs485_read(maxcount=8, first_timeout=2, subsequent_timeout=1)
    if not received_data:
        print("No data received after flush, as expected.")
        test_status = True
    else:
        print("Data was received after flush, which is unexpected:")
        for byte in received_data:
            ascii_char = chr(byte) if 32 <= byte <= 126 else '.'  # Printable ASCII range
            print(f"0x{byte:02X} ({ascii_char})")
    print(f"Test 3 Result: {'PASS' if test_status else 'FAIL'}")

def test100():
    """
    Test 100: ADC Read Test
    - This test will enable the ADC channels and read their values, printing the results.
    """
    global adapter
    test_status = False
    input("Press Enter to start Test 100: ADC Read Test")
    print("Enabling ADC channels...")
    adc_enable_all_channels()
    print("Reading ADC values for channels 0-2...")
    channel_values = adc_read_all_channels()
    if all(value is not None for value in channel_values):
        for i, value in enumerate(channel_values):
            print(f"Channel {i}: {value} (approximately {(value / 4095) * ADC_SUPPLY_VOLTAGE:.2f} V)")
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
    input("Press Enter to start Test 200: Timer Capture Test")
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

# --- main function ---
def main():
    """
    Main function to read .hex files from the input directory,
    identify devices, and write data to them via EasyAdapter using page writes.
    """
    global adapter
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
    # Run Test 200: Timer Capture Test
    test200()
    # Run Test 100: ADC Read Test
    test100()
    # Run Test 1: Basic RS485 Write Test
    test1() 
    # Run Test 2: Basic RS485 Read Test
    test2()
    # Run Test 3: RS485 Flush Test
    test3()

if __name__ == "__main__":
    main()