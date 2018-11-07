# Project Goal
The goal of this project is to simulate/imitate data communication between the BLE Controller and the motor controller on the Xiaomi M365 Scooter with a microcontroller with cellular capabilities, the Particle Electron. BasicMotorControl only commands the scooter's motor to work. MotorControl commands the scooter's motor to work and includes ability to read messages from scooter's motor controller including the battery level, odometer, velocity, etc. Firmware.ino is (likely) the latest version that I am using, which contains a gps class and a backgroundProcesses class that preforms many commands.  

# Project Information

Sucess:
- Can transmit commands to scooter: lock/unlock, tail light on/off, cruise on/off.
- Can transmit control values to scooter (brake and throttle values).
- Can recieve messages from the scooter, including: beep request, alarm alerts, speed, average speed, odometer values, temperature values, and battery level.
- Can locate via GPS Chip.
- Can ensure scooter is powering Particle Electron.
- Can activate 6V headlight via a buck converter that turns the 3.3V to 6V.
- Can ensure the Particle Electron's battery in continiously powered (when the scooter is on, even if it is locked).

Work in Progress/Future Work:
- Add option for eco mode (probably not going to implement).
- Add energy recovery commands (probably not going to implement).
- Currently experiencing some problems when locking the scooter on firmware.ino. Sometimes when the scooter is locked and the alarm will always be active (the wheel will not stop vibrating). My current solution is to unlock the scooter, power the scooter's motor controller off and back on again, however this is only a temporary solution.
- I am currently working on using a bitwise shift operator (>> or <<) and multiple XOR operations (0x00FF & 0xFF00) to avoid using the hexstring class in my program. 

Hardware Implementation
- To ensure continuous power:
  - Connect the data line to the Particle Electron's TX pin.
  - Connect the 5V power line to the Particle Electron's VIN pin, Brake, and Throttle.
  - Connect the ground line to both the Particle Electron's ground pin and to a transistor (I use a 2N2222 transistor).
  - Connect the 40V hot line to the transistor.
  - Connect a wire to the transistor and D0 (when this wire is pulled high, the tranisitor (if properly wired) will connect the 40V hot line turning on the scooter. 
  - This is necessary as the scooter will automatically turn off after 5 minutes when unlocked. I believe the scooter also automatically turns off when lock, although it takes hours for this to happen. If the scooter does not supply power continuously, the Particle Electron's battery could die, which would not be good for locating it when lost.
- For data communication only:
  - Connect the data line to the Particle Electron's TX pin.
  - Connect the ground line to the Particle Electron's ground pin.
  - Note: you will need to ensure the scooter's motor controller is on (the red light) before Serial communication will be successful.
- Throttle and Brake Input:
  - Connect ground wires to ground.
  - Connect hot wires to motor controller's 5V line.
  - Connect brake wire to motor controller's 5V line.
  - Connect brake and throttle wire to Particle Electron's A3 and A4 pins (check the comments in the program to see which one is correct).
- Headlight Connection:
  - Connect headlight wires to a buck converter (5V to 6.2V).
  - Connect buck converter's hot line to transistor (I use the 2n2222 transistor again).
  - Connect buck converter's ground to ground.
  - Connect the transistor to 5V power supply and D1 pin to switch headlight on and off.
- Note: When attempting to turn the scooter's motor controller off it is important to not have the Particle Electron powered via USB. It seems that the USB power will keep the scooter's motor controller on. (It took me forever to figure this out as I usually flash the device and use the USB serial monitor to track the program's failures.)
  
# How the Xiaomi M365 Operates:
- The scooter is composed of 3 microcontrollers: the Bluetooth (BLE) controller, the motor controller, and the battery management system (BMS) controller.
  - The BLE Controller:
    - Can send single commands to the motor controller (tail light on/off, cruise control on/off, lock/unlock),
    - Continuously sends values of the brake and throttle levers (even when the scooter is locked),
    - Turns the power to the scooter on by touching ground and 40V hot to each other for less than a second, 
    - Turns on the headlight led and tail light led by touching ground and 40V hot to each other for less than a second after powered on, and
    - Is connected to motor controller by four wires (ground [black], data [yellow], and two hot lines: 5V [red] & 40V [green]), and
    - communicates via a serial connection at half-duplex (115200bps, 8 data bits, no parity bit, one stop bit (8n1)).
  - The Motor Controller:
    - solely operates the brushless D.C. (BLDC) motor,
    - connects to the ESC controller by four wires (mentioned above),
    - connects to the BMS controller by three wires (ground, data, and tail light power [not sure about the colors]), and
    - connects to the BLDC motor with three big power wires and five other wires (the three big wires are power wires that switch on and off in specific orders to move the BLDC motor, while (I think) the five other wires monitor the position of the motor),
  - The BMS Controller:
    - monitors battery levels and keeps batteries in a healthy condition, and
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
  - The X1 Structure is a reply to the this packet motor1 in the program.
  - 0x55 and 0xAA are fixed headers that signal a packet is being transmitted.
  - 0x06 is the size of the message
  - 0x21 is the sender of the message (the X1 structure).
  - 0x64 is the command of the message.
  - 0 is a 0.
  - D is the drive mode: 0x0 = Inactive, 0x01 = Active, 0x02 = Eco Inactive, 0x03 = Eco Active (Drive mode is active when the wheel is moving, however does not mean the motor is on).
  - L is the amount of LED's that should be lit on the BLE dashboard.
  - H is the headlight led: 0x0 = off, 0x64 = on (I think this turns on the tail light too).
  - B is the beep reqeuest (the motor controller expects a reply confirming that the BLE beeped).
  - Ck0 and Ck1 are the checksum values.
- One of the Information Structures looks like: | 0x55 | 0xAA | 0x22 | 0x23 | ... | Ck0 | Ck1 |
  - This Information Stucture is a reply to inforequest in the program
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
    - *These values are calculated in HEX, where the first character in the array represents the one's place, the second character represents the ten's place, and, if applicable, the third character represents the hundreds place. Since the values are calculated in HEX, one must multiple the ten's place by 256 and the hundred's place by 256^2. For example, an odometer readeing of [0xC6, 0x4E, 0x02] would be the decimal equivelent of 151,238 (198[0xC6] + (78[0x4E] * 256) + (2[0x02] * 256 * 256)). In this example, the odometer reades 151.238 km. Note that we divided by 1000 to get the result in km, and this divisor will change for different unit types. For example, temperature (in C) is divided by 10 instead of 1000.
- Another Informationn Structure looks like: | 0x55 | 0xAA | 0x06 | 0x23 | 0x01 | 0x7B | E | 0x0 | C | 0 | Ck0 | Ck1 |
  - E is the Engergy Recovery Strength: 0x0 = low, 0x01 = med, 0x02 = high.
  - C is the cruise control setting: 0x0 = cruise control disabled, 0x01 = cruise control enabled (this does not mean cruies control is active).
  - Not sure what the 0's represent.
  - This Information Structuer is a reply to: | 0x55 | 0xAA | 0x06 | 0x20 | 0x61 | 0x7B | 0x04 | 0x02 | T | B | Ck0 | Ck1 |
    - T is the throttle value.
    - B is the brake value.
