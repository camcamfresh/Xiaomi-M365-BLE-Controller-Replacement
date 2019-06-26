# Project Goal
The goal of this project is to make a replacement board for the Xiaomi M365’s Bluetooth (BLE) Controller. The replacement board will use a Particle Electron with a Neo-GPS to remotely control & locate the scooter.

# Project Statuses
- Work In Progress:
  - Create custom iOS & Android apps to control the scooter.
- Successes:
  - Can transmit messages to scooter, including commands.
  - Can receive messages from the scooter.
  - Can locate via Neo-GPS Module
  - Can toggle scooter’s power button.
  - Can activate headlight with use of boost converter (5V to 6V).

# Hardware Setup for Particle Electron
- Remove factory BLE controller and make the following connections to the wires:
  - Connect Particle Electron's GND Pin to Ground.
  - Connect Particle Electron's TX Pin to Data Line (~2.7V)
  - Connect Particle Electron's VIN Pin to Power (5V)
  - Connect Particle Electron's D7 Pin to the base of a transistor (2N2222 recommended). Depending on your transistor type (NPN or PNP) connect the Ground and Power (40V) lines to the resistor's collector and emitter such that when the transistor is saturated (D7 is pulled high) the Ground and Power (40V) lines connect.
- Connect Brake & Throttle Inputs:
  - Connect Brake & Throttle GND to Ground Line.
  - Connect Brake & Throttle Positive/Hot Line to Power (5V) Line.
  - Connect Brake Input Wire to 1.6K Resistor and the other side of the resistor to Particle Electron's A5 Pin.
  - Connect Throttle Input Wire to 1.6K Resistor and the other side of the resistor to Particle Electron's A4 Pin.
  - Connect a 3.3K Resistor to the Ground Line and the other side of the resistor to the Particle Electron A5 Pin.
  - Connect a 3.3K Resistor to the Ground Line and the other side of the resistor to the Particle Electron A4 Pin.
- Connect Buzzer (Optional):
  - Connect Buzzer to 5V Line.
  - Connect Particle Electron's D2 Pin to the base of a transistor (2N2222 recommended). Depending on your transistor type (NPN or PNP) connect the Buzzer's Ground & Ground Line to the resistor's collector and emitter such that when the transistor is saturated (D2 is pulled high) the Buzzer's Ground and Ground Line connects.
- Connect Headlight (Optional):
  - Connect Headlight's Ground (White) and Positive/Hot (Yellow) wires to the boost converter's respective output.
  - Connect boost converter's Positive/Hot Input to the Power (5V) Line.
  - Connect Particle Electron's D6 Pin to the base of a transistor (2N2222 recommended). Depending on your transistor type (NPN or PNP) connect the boost converter's Ground & Ground Line to the resistor's collector and emitter such that when the transistor is saturated (D2 is pulled high) the Boost converter's Ground and Ground Line connects.
- Connect GPS (Optional):
  - Connect Particle Electron's 3.3V Pin to Neo-GPS 3.3V Pin
  - Connect Particle Electron's GND Pin to Neo-GPS GND Pin
  - Connect Particle Electron's C0 Pin to Neo-GPS TX Pin
  - Connect Particle Electron's C1 Pin to Neo-GPS RX Pin

# M365 Library Setup for Particle Electron
- The M365 Library handles all communication between the Particle Electron and Xiaomi M365 Scooter. It is available through Particle's Community Libraries.
  - In order to use the library, one must instantiate a M365 object, specify the serial port and analog/digital pins in setup(), and call the process() method whenever serial data is available. 
  - The following methods are used for setup:
    - setup(Serial1, "brakePin", "throttlePin", "buzzerPin", "rled", "gled", "bled", "headlightPin", powerPin);
	- setBrake("brakePin")
	- setThrottle("throttlePin")
	- setBuzzer("buzzerPin")
	- setRGB("rled", "gled", "bled")
	- setHeadlight("headlightPin")
	- setPower("powerPin")
  - The M365 Library provides two structures for interacting with the M365 object: stats_t & command_t. In order to get/set the stats/commands, one must pass a reference of a preexisting stats_t or commands_t structure to the getStats() & setCommand() methods. (e.g. getStats(&exampleStats); ).
  - Check the basic_usage.ino for example.

# Xiaomi M365 Operation Basics
- The scooter is composed of 3 microcontrollers. All three microcontrollers communicate on a shared UART half-duplex serial line (115200bps, 8 data bits, no parity bit, one stop bit (8n1).
  - The Bluetooth (BLE) Controller
    - connects to the motor controller by four wires (GND, DATA, 5V, & ~40V),
	- continuously sends values of the brake and throttle levers to the motor controller whenever the scooter is powered on,
	- is responsible for communicating with Xiaomi's BLE applications,
	- is responsible for sending commands to the motor controller,
	- automatically turns the scooter off after 5 minutes when unlocked, and 10 minutes when locked, and 
	- contains a power button responsible for toggling the power, night mode, & eco mode by shorting the 40V line with GND for different amounts of time:
	  - power on (100 ms), power off (2000 ms)
	  - toggle night mode (100 ms)
	  - toggle eco mode (100 ms, with 50 ms delay, 100 ms)
  - The Motor Controller
	- connects to the BLE controller,
	- connects to the BMS controller,
	- can send statistics upon request such as alarm, lock, cruise, & tail light statuses, odometer values, and more.
	- solely operates the brushless D.C. (BLDC) motor via three big power wires & five smaller monitoring wires.
  - The Battery Management System (BMS) Controller
    - monitors individual battery cells and keeps batteries in healthy condition, and
	- can send statistics upon request such as overall & individual cell levels, temperatures, etc.
  
# M365 Packet Protocol
- All three of the scooter's microcontrollers communicate on the same data wire.
- The BLE microcontroller sends messages every 20 ms; the other controllers reply within 1-2 ms.
- Each packet varies in size and destination. A single packet looks like this in HEX:
  - | 0x55 | 0xAA | L | D | T | C | ... | ck0 | ck1 |
  - 0x55 and 0xAA are fixed headers that signal a packet is being transmitted.
  - L is the message length in bytes (from T to ...)
  - D is the message destination:
    - 0x20 = BLE to motor controller
	- 0x21 = motor controller to BLE
	- 0x22 BLE to BMS
	- 0x23 motor controller to BLE
	- 0x25 BMS to motor controller
  - T is the message type: 0x01 = Read, and 0x03 = Write (Some messages use 0x64 & 0x65).
  - C is the command type: (e.g. lock, unlock, information, etc.).
  - ... is the message data which varies on packet.
  - ck0 & ck1 are checksum values to confirm the integrity of the message. To calculate this value, we take the sum all of the previous bytes together except 0x55 & 0xAA. We then preform a XOR operation with 0xFFFF. The resulting value contains ck1 & ck0, respectively.
  - Source of Communication Protocol Information: https://github.com/CamiAlfa/M365-BLE-PROTOCOL/blob/master/protocolo
  
# Packet Information:
- The X1 Structure:
  - Request: | 0x55 | 0xAA | 0x9 | 0x20 | 0x65 | 0x0 | 0x4 | T | B | S | ck0 | ck1 |
    - T is the throttle value.
    - B is the brake value.
	- S is the beep confirmation value.
  - Response: | 0x55 | 0xAA | 0x06 | 0x21 | 0x64 | 0 | D | L | N | B | ck0 | ck1 |
    - D is the drive mode: 
	  - 0x0 = Eco Disabled, Wheel Stationary
	  - 0x01 = Eco Disabled, Wheel Moving
	  - 0x02 = Eco Enabled, Wheel Stationary
	  - 0x03 = Eco Enabled, Wheel Moving
    - L is the amount of LED's that should be lit on the BLE dashboard.
	  - A value of 0 may indicate an error state.
    - N is the night mode: 
	  - 0x0 = off
	  - 0x64 = on
    - B is the beep reqeuest (expects beep confirmation).

- The Main Information Structure:
  - Request: | 0x55 | 0xAA | 0x06 | 0x20 | 0x61 | 0xB0 | 0x20 | 0x02 | T | B | ck0 | ck1 |
    - T is the throttle value.
    - B is the brake value.
  - Response: | 0x55 | 0xAA | 0x22 | 0x23 | ... | ck0 | ck1 |
  - If it were in an array:
    - array[0] = 0x55, which is the first part of the header,
    - array[8] = alarm alert; 0x0 = alarm off, 0x09 = alarm on.
    - array[10] = lock status; 0x0 = unlocked, 0x02 = locked, 0x06 = alarm on (therefore also locked).
    - array[14] = battery level in percent; ex: 0x64 = 100%.
    - array[16], and [17] is the current speed in kph*.
    - array[18], and [19] is the average speed in kph*.
    - array[20], [21], and [22] is the odometer reading in km*.
    - array[23], [24], and [25] is the single millage reading in km*.
    - array[26], and [27] is the motor controls timer*.
    - array[28] is the temperature in C.
    - *These values are calculated in HEX, where the first character in the array represents the one's place, the second character represents the ten's place, and, if applicable, the third character represents the hundreds place. Since the values are calculated in HEX, one must multiple the ten's place by 256 and the hundred's place by 256^2. For example, an odometer readeing of [0xC6, 0x4E, 0x02] would be the decimal equivelent of 151,238 (198[0xC6] + (78[0x4E] * 256) + (2[0x02] * 256 * 256)). In this example, the odometer reads 151.238 km. Note that we divided by 1000 to get the result in km, and this divisor will change for different unit types. For example, temperature (in C) is divided by 10 instead of 1000.

- Additional Information Structure:
  - Request: | 0x55 | 0xAA | 0x06 | 0x20 | 0x61 | 0x7B | 0x04 | 0x02 | T | B | ck0 | ck1 |
  - T is the throttle value.
    - B is the brake value.
  - Response: | 0x55 | 0xAA | 0x06 | 0x23 | 0x01 | 0x7B | E | 0x0 | C | 0 | ck0 | Ck1 |
  - E is the Engergy Recovery Strength: 0x0 = low, 0x01 = med, 0x02 = high.
  - C is the cruise control setting: 0x0 = cruise control disabled, 0x01 = cruise control enabled (this does not mean cruies control is active).
  - This Information Structuer is a reply to: | 0x55 | 0xAA | 0x06 | 0x20 | 0x61 | 0x7B | 0x04 | 0x02 | T | B | Ck0 | Ck1 |
    
