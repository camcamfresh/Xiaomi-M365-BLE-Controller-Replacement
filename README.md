# Project Goal
The goal of this project is to simulate/imitate data received by the BLE Controller on the Xiaomi M365 Scooter with a microcontroller with cellular capabilities (Particle Electron).

# Project Information

Sucess:
- Can transmit commands to scooter: lock/unlock, tail light on/off, cruise on/off.
- Can transmit control values to scooter (brake and throttle values).
- Can recieve messages from the scooter, including: beep request, alarm alerts, speed, average speed, odometer values, temperature values, and battery level.
- Can locate via GPS Chip.
- Can ensure scooter is powering Particle Electron.

Work in Progress/Future Work:
- Add a boost converter (from 3.3V) for the 6V headlight led.
- Add converter to charge Particle Electron's battery (40V to 6-12V)
- Add option for eco mode.
- Add energy recovery commands.

Hardware Implementation
- To ensure continuous power:
  - Connect the data line to the Particle Electron's TX pin.
  - Connect the 5V power line to both the Particle Electron's VIN pin and D0 pin.
  - Connect the ground line to both the Particle Electron's ground pin and to a transistor (I use a 2N2222A transistor).
  - Connect the 40V hot line to the transistor.
  - Connect a wire to the transistor and D1 (when this wire is pulled high, the tranisitor (if properly wired) will connect the 40V hot line turning on the scooter. 
  - This is necessary as the scooter will automatically turn off after 5 minutes when unlocked. If the scooter does not supply power continuously, the Particle Electron's battery could die, which would not be good for locating it when lost.
- For data communication only:
  - Connect the data line to the Particle Electron's TX pin.
  - Connect the ground line to the Particle Electron's ground pin.
  - Note: you will need to ensure the scooter's motor controller is on (the red light) before Serial communication will be successful.
- Throttle and Brake Input:
  - Connect ground wires to Particle Electron's ground pin.
  - Connect hot wires to Particle Electron's 3.3V pin.
  - Connect brake wire to Particle Electron's A3 pin.
  - Connect throttle wire to Particle Electron's A4 pin.

# How the Xiaomi M365 Operates:
- The scooter is composed of 3 microcontrollers: the Bluetooth (BLE) controller, the motor controller, and the battery management system (BMS) controller.
  - The BLE Controller:
    - Can send single commands to the motor controller (tail light on/off, cruise control on/off, lock/unlock),
    - Continuously sends values of the brake and throttle levers,
    - Turns the power to the scooter on by touching ground and 40V hot to each other for less than a second,
    - Turns on the headlight led and tail light led by touching ground and 40V hot to each other for less than a second after powered on,
    - Is connected to motor controller by four wires (ground [black], data [yellow], and two hot lines: 5V [red] & 40V [green]), and
    - communicates via a serial connection at half-duplex (115200bps, 8 data bits, no parity bit, one stop bit (8n1)).
  - The Motor Controller:
    - solely operates the brushless D.C. (BLDC) motor,
    - connects to the ESC controller by four wires (mentioned above),
    - connects to the BMS controller by three wires (ground, data, and tail light power [not sure about the colors]),
    - connects to the BLDC motor with three big power wires and five other wires (the three big wires are power wires that switch on and off in specific orders to move the BLDC motor, while (I think) the five other wires monitor the position of the motor),
  - The BMS Controller:
    - monitors battery levels and keeps batteries in health condition,
    - sends values (upon request) to BLE including overall & individual cell battery levels, temperatures, etc.
# Packet Protocol:
- The scooter's controllers communicate to each other using HEX protocols. For example, a single packet looks like this:
  - | 0x55 | 0xAA | L | D | T | C | ... | Ck0 | Ck1 |
  - 0x55 and 0xAA are fixed headers that signal a packet is being transmitted.
  - L is the byte size of the message (this includes the bytes: T, C, and ...).
  - D is device: 0x20 = BLE to motor controller, 0x21 = motor controller to BLE, 0x22 BLE to BMS, 0x23 motor controller to BLE, and 0x25 BMS to motor controller.
  - T is the message type: 0x01 = Read, and 0x03 = Write (Some messages use 0x64 & 0x65).
  - C is the command type (e.g. lock, unlock, information, etc.).
  - ... is the message which varies in size.
  - Ck0 and Ck1 are the checksum values to confirm the integrity of the message: to calculate this value we add all of the previous bytes together except 0x55 and 0xAA. We then take this sum and preform an XOR operation (^) with the sum and 0xFFFF. The result of this calculation contains Ck1 and Ck0, respectively. To reverse the order, I convert the result into a string and use substring. The resulting Ck0 and Ck1 strings are then converted back to HEX.
  - Source of Communication Protocol Information: https://github.com/CamiAlfa/M365-BLE-PROTOCOL/blob/master/protocolo
  
# Message Information:
- The X1 Structure looks like: | 0x55 | 0xAA | 0x06 | 0x21 | 0x64 | 0 | D | L | H | B | Ck0 | Ck1 |
  - 0x55 and 0xAA are fixed headers that signal a packet is being transmitted.
  - 0x06 is the size of the message
  - 0x21 is the sender of the message (the X1 structure).
  - 0x64 is the command of the message.
  - 0 is a 0.
  - D is the drive mode: 0x0 = Inactive, 0x01 = Active, 0x02 = Eco Inactive, 0x03 = Eco Active (Drive mode is active when the wheel is moving, however does not mean the motor is on).
  - L is the amount of LED that should be lit on the BLE dashboard.
  - H is the headlight led: 0x0 = off, 0x64 = on.
  - B is the beep reqeuest (the motor controller expects a reply confirming that the BLE beeped).
  - Ck0 and Ck1 are the checksum values.
- The Information Structure looks like: | 0x55 | 0xAA | 0x22 | 0x23 | ... | Ck0 | Ck1 |
  - If it were in an array:
    - array[0] = 0x55;
    - array[8] = alarm alert; 0x0 = alarm off, 0x09 = alarm on.
    - array[10] = lock status; 0x0 = unlocked, 0x02 = locked, 0x06 = alarm on (locked).
    - array[14] = battery level; 0x64 = 100%.
    - array[16], and [17] is the current speed in kph.
    - array[18], and [19] is the average speed in kph.
    - array[20], [21], and [22] is the odometer reading in km.
    - array[23], [24], and [25] is the single millage reading in km.
    - array[26], and [27] is the motor controls timer.
    - array[28] is the temperature in C. 

