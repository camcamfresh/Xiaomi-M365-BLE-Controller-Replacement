#include <NMEAGPS.h> //Part of NeoGPS Library
#include "Serial4/Serial4.h" //Enables GPS Serial Port
SYSTEM_THREAD(ENABLED); //Prevents unwanted interuptions in program due to cellular disconnection.

char alarm, beep, locked, messagelength, recieverindex, messageindex;
unsigned char lock [] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x70, 0x1, 0x0, 0x67, 0xFF};
unsigned char unlock[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x71, 0x1, 0x0, 0x66, 0xFF};
unsigned char tailon[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x7D, 0x2, 0x0, 0x59, 0xFF};
unsigned char tailoff[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x7D, 0x0, 0x0, 0x5B, 0xFF};
unsigned char cruiseon[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x7C, 0x1, 0x0, 0x5B, 0xFF};
unsigned char cruiseoff[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x7C, 0x0, 0x0, 0x5C, 0xFF};
int batterylevel, driveStatus, escHeadLedStatus, calc, lockCommand = 1, ledpower = 1, powerControl = 1, alarmCommand = 1, averageVelocity, temperature, odometer, velocity;
NMEAGPS gps;
PMIC pmic;
String gpsurl, gpsvalid;


void setup() {
    Serial.begin(); //Computer Serial Monitor
    Serial1.begin(115200); //Scooter Serial Port
    Serial1.halfduplex(true); //Half Duplex Serial Communication
    Serial4.begin(9600); //GPS Serial Port
    
    Particle.function("CloudCommand", cloudCommand);
    Particle.variable("AvgVelocity", averageVelocity);
    Particle.variable("BatteryLevel", batterylevel);
    Particle.variable("Coordinates", gpsvalid);
    Particle.variable("Location", gpsurl);
    Particle.variable("Odometer", odometer);
    Particle.variable("Temperature", temperature);
    Particle.variable("Velocity", velocity);
    Particle.variable("DriveStatus", driveStatus);
    Particle.variable("LightsOn", escHeadLedStatus);
    
    pinMode(A3, INPUT); //Brake Lever Sensor
    pinMode(A4, INPUT); //Motor Throttle Sensor
    pinMode(D0, OUTPUT); //Power Switch
    pinMode(D2, OUTPUT); //Buzer
    pinMode(D3, OUTPUT); //Red (Lock)
    pinMode(D4, OUTPUT); //Green (Unlock)
    pinMode(D5, OUTPUT); //Blue (Connecting)
    pinMode(D6, OUTPUT); //Headlight tied to 3.3V to 6V converter.
}
void loop() {
    escControl();
    backgroundProcess();
    gpsProcess();
    Particle.process();
    if(!Particle.connected() && lockCommand == 1) Particle.connect();
}
void escControl(){
    //Create packets that will be sent to scooter.
    unsigned char motor[] = {0x55, 0xAA, 0x7, 0x20, 0x65, 0x0, 0x4, 0x26, 0x26, 0x0, 0x0, 0x0, 0x0};
    unsigned char motor1[] = {0x55, 0xAA, 0x9, 0x20, 0x64, 0x0, 0x6, 0x26, 0x26, 0x0, 0x0, 0x72, 0x0, 0x0, 0x0};
    unsigned char inforequest[] = {0x55, 0xAA, 0x6, 0x20, 0x61, 0xB0, 0x20, 0x02, 0x28, 0x26, 0x58, 0xFE};
    
    //Capture Brake and Throttle Values and Store into Packets above.
    char brake = map(analogRead(A3), 685, 4095, 0, 0xC2);
    if(brake <= 0x26) brake = 0x26;
    motor[8] = brake; 
    motor1[8] = brake;
    
    char speed = map(analogRead(A4), 685, 4095, 0, 0xC2);
    if(speed <= 0x26) speed = 0x26;
    motor[7] = speed;
    motor1[7] = speed;
    
    motor[10] = beep; //Provides confirmation to the scooter that we made a beep.
    motor1[10] = beep;
    
    inforequest[8] = speed; //This is a information request for various values such as speed, odeometer, temperature, etc.
    inforequest[9] = brake;
    
    //Calculate checksums for all three packets.
    uint16_t calc =  (motor[2] + motor[3] + motor[4] + motor[5] + motor[6] + motor[7] + motor[8] + motor[9] + motor[10]) ^ 0xFFFF;
    char ck1 = hexstring(String(calc, HEX).substring(2,4)); //The two checksum values are reversed.
    char ck2 = hexstring(String(calc, HEX).substring(0,2));
    motor[11] = ck1;
    motor[12] = ck2;
    
    uint16_t calc1 = (motor1[2] + motor1[3] + motor1[4] + motor1[5] + motor1[6] + motor1[7] + motor1[8] + motor1[9] + motor1[10] + motor1[11] + motor1[12]) ^ 0xFFFF;
    char ck3 = hexstring(String(calc1, HEX).substring(2,4));
    char ck4 = hexstring(String(calc1, HEX).substring(0,2));
    motor1[13] = ck3;
    motor1[14] = ck4;

    uint16_t calc2 = (inforequest[2] + inforequest[3] + inforequest[4] + inforequest[5] + inforequest[6] + inforequest[7] + inforequest[8] + inforequest[9]) ^ 0xFFFF;
    char ck5 = hexstring(String(calc2, HEX).substring(2,4));
    char ck6 = hexstring(String(calc2, HEX).substring(0,2));
    inforequest[10] = ck5;
    inforequest[11] = ck6;
    
    //Send Packets to Scooter
    Serial1.write(motor, sizeof(motor)); delay(10); //Sends control values to scooter.
    Serial1.write(motor, sizeof(motor)); delay(10);
    Serial1.write(motor, sizeof(motor)); delay(10);
    Serial1.write(motor, sizeof(motor)); delay(10);
    if(Serial1.available()) escReader(); //Clears Serial Buffer; it seems to recieve the packets we transmit.
    Serial1.write(motor1, sizeof(motor1)); delay(10); //Sends control values and request X1 structure from scooter.
    if(Serial1.available()) escReader(); //Read X1 Structure from the Scooter.
    Serial1.write(inforequest, sizeof(inforequest)); delay(10); //Sends control values and request scooter trip information.
    if(Serial1.available()) escReader(); //Read scooter trip information.
    
}
char hexstring(String data){
    char hex = 0;
    if(data.length() == 2){
        if(data.charAt(0) == '1') hex = 0x10;
            else if(data.charAt(0) == '2') hex = 0x20;
            else if(data.charAt(0) == '3') hex = 0x30;
            else if(data.charAt(0) == '4') hex = 0x40;
            else if(data.charAt(0) == '5') hex = 0x50;
            else if(data.charAt(0) == '6') hex = 0x60;
            else if(data.charAt(0) == '7') hex = 0x70;
            else if(data.charAt(0) == '8') hex = 0x80;
            else if(data.charAt(0) == '9') hex = 0x90;
            else if(data.charAt(0) == 'a') hex = 0xA0;
            else if(data.charAt(0) == 'b') hex = 0xB0;
            else if(data.charAt(0) == 'c') hex = 0xC0;
            else if(data.charAt(0) == 'd') hex = 0xD0;
            else if(data.charAt(0) == 'e') hex = 0xE0;
            else if(data.charAt(0) == 'f') hex = 0xF0;
        if(data[1] == '0') hex = 0x0 + hex;
            else if(data.charAt(1) == '1') hex = 0x1 + hex;
            else if(data.charAt(1) == '2') hex = 0x2 + hex;
            else if(data.charAt(1) == '3') hex = 0x3 + hex;
            else if(data.charAt(1) == '4') hex = 0x4 + hex;
            else if(data.charAt(1) == '5') hex = 0x5 + hex;
            else if(data.charAt(1) == '6') hex = 0x6 + hex;
            else if(data.charAt(1) == '7') hex = 0x7 + hex;
            else if(data.charAt(1) == '8') hex = 0x8 + hex;
            else if(data.charAt(1) == '9') hex = 0x9 + hex;
            else if(data.charAt(1) == 'a') hex = 0xA + hex;
            else if(data.charAt(1) == 'b') hex = 0xB + hex;
            else if(data.charAt(1) == 'c') hex = 0xC + hex;
            else if(data.charAt(1) == 'd') hex = 0xD + hex;
            else if(data.charAt(1) == 'e') hex = 0xE + hex;
            else if(data.charAt(1) == 'f') hex = 0xF + hex;
    }
    if(data.length() == 1){
        if(data.charAt(0) == '0') hex = 0x0;
            else if(data.charAt(0) == '1') hex = 0x1;
            else if(data.charAt(0) == '2') hex = 0x2;
            else if(data.charAt(0) == '3') hex = 0x3;
            else if(data.charAt(0) == '4') hex = 0x4;
            else if(data.charAt(0) == '5') hex = 0x5;
            else if(data.charAt(0) == '6') hex = 0x6;
            else if(data.charAt(0) == '7') hex = 0x7;
            else if(data.charAt(0) == '8') hex = 0x8;
            else if(data.charAt(0) == '9') hex = 0x9;
            else if(data.charAt(0) == 'a') hex = 0xA;
            else if(data.charAt(0) == 'b') hex = 0xB;
            else if(data.charAt(0) == 'c') hex = 0xC;
            else if(data.charAt(0) == 'd') hex = 0xD;
            else if(data.charAt(0) == 'e') hex = 0xE;
            else if(data.charAt(0) == 'f') hex = 0xF;
    }
    return hex;
} //Converts a String up to 2 characters long to a HEX value.
void escReader(){
    unsigned char start[] = {0x55, 0xAA, 0xF, 0x24, 0x1, 0x0, 0x4D, 0x49, 0x53, 0x63, 0x6F, 0x6F, 0x74, 0x65, 0x72, 0x35, 0x32, 0x31, 0x39, 0x85, 0xFB}, reciever[64];
    while(Serial1.available()){
        char data = Serial1.read();
        switch(recieverindex){
            case 0: //Read Header
                if(data == 0x55){ 
                    reciever[0] = data;
                    recieverindex++;
                }
                break;
            case 1: //Read Header
                if(data == 0xAA){
                    reciever[1] = data;
                    recieverindex++;
                }
                break;
            case 2: //Read Message Length
                reciever[2] = data;
                messagelength = data + 5;
                recieverindex++;
                messageindex = 3;
                calc = data;
                break;
            case 3: //Read Message
                reciever[messageindex] = data;
                calc += data;
                messageindex++;
                if(messageindex == messagelength) recieverindex++;
                break;
            case 4: //Read Last Byte and verify checksum.
                reciever[messageindex] = data;
                calc += data;
                calc ^= 0xFFFF;
                if(calc == (reciever[messagelength - 1] + reciever[messagelength])) recieverindex++;
                    else recieverindex = 10;
            case 5: //Separates packets by address.
                switch(reciever[3]){
                    case 0x21:
                        if(reciever[2] == 0x02) Serial1.write(start, sizeof(start));
                        else if(reciever [2] == 0x06){
                            beep = reciever[9];
                            driveStatus =  (String(reciever[6], DEC)).toInt();
                            escHeadLedStatus =  (String(reciever[8], DEC)).toInt();
                            if(beep == 0x01){ //Scooter reqest a short beep when BLE controller is connected, the scooter's alarm is activated, or charging begins.
                                tone(D2, 20, 50);
                            }
                            else if(beep == 0x02){//Scooter request a long beep when cruise is enabled and activated.
                                tone(D2, 20, 100);
                                beep = 0x01;
                            }
                            else if(beep == 0x03){//Scooter request a short beep when battery is fully charged.
                                tone(D2, 20, 50);
                                beep = 0x01;
                            }
                        }
                        recieverindex = 10;
                        break;
                    case 0x23:
                        if(reciever[2] == 0x22){
                            alarm = reciever[8]; //Reads 0x09 when alarm is active.
                            locked = reciever[10]; //Reads 0x00 when unlocked, 0x02 when locked, and 0x06 when the alarm is active.
                            batterylevel = (String(reciever[14], DEC)).toInt();
                            velocity = ((reciever[17] * 256) + reciever[16]) / 1000 / 1.60934; //Reads speed (which is in HEX), converts to mph.
                            averageVelocity = ((reciever[19] * 265) + reciever[18]) / 1000 / 1.60934;
                            odometer = ((reciever[22] * 256 * 256) + (reciever[21] * 256) + reciever[20]) / 1000 / 1.60934;
                            temperature = (((reciever[29] * 256) + reciever[28]) / 10 * 9 / 5) + 32; //Reads temperature, converts to F.
                            if(alarm == 0x09 && alarmCommand == 1){
                                Particle.publish("Alarm", "Alarm Alert!");
                                tone(D2, 20, 1000);
                            }
                            if(locked == 0x00 && lockCommand == 0x01){
                                Serial1.write(lock, sizeof(lock));
                            }
                            else if((locked == 0x02 || locked == 0x06) && lockCommand == 0x00){
                                Serial1.write(unlock, sizeof(unlock));
                            }
                        }
                        recieverindex = 10;
                        break;
                    default:
                        recieverindex = 10;
                        break;
                }
            case 10: //Erases reciever array.
                for(int erase = 0; erase <= messagelength; erase++) reciever[erase] = 0;
                recieverindex = 0;
                break;
            default:
                recieverindex = 10;
                break;
        }
    }
}
void backgroundProcess(){
    if((pmic.getSystemStatus() & 0x04) == 0 && powerControl){ //Checks PMIC Function in Particle.h for power status.
        digitalWrite(D0, HIGH); //D0 connects to a transistor to switch power on, if it is off.
        delay(100);
        digitalWrite(D0, LOW); //D0 transistor connected to scooter 40V line and ground.
    }
    if(escHeadLedStatus == 0x64){
        digitalWrite(D0, HIGH);
        delay(100);
        digitalWrite(D0, LOW);
    }
    if(driveStatus == 0x02){
        digitalWrite(D0, HIGH);
        delay(100);
        digitalWrite(D0, LOW);
        delay(25);
        digitalWrite(D0, HIGH);
        delay(100);
        digitalWrite(D0, LOW);
    }
    if(locked == 0x0 && Particle.connected() && ledpower == 1){
        digitalWrite(D3, LOW);
        digitalWrite(D4, HIGH);
        digitalWrite(D5, LOW);
    }
    else if(locked == 0x02 && Particle.connected() && ledpower == 1){
        //digitalWrite(D3, HIGH); LED broken
        digitalWrite(D4, LOW);
        digitalWrite(D5, LOW);
    }
    else if(locked == 0x06 && Particle.connected() && ledpower == 1){
        digitalWrite(D3, LOW);
        digitalWrite(D4, LOW);
        digitalWrite(D5, HIGH);
    }
    else if(!Particle.connected() && ledpower == 1){
        digitalWrite(D3, LOW);
        digitalWrite(D4, LOW);
        digitalWrite(D5, HIGH);
    }
    else if(ledpower == 0){
        digitalWrite(D3, LOW);
        digitalWrite(D4, LOW);
        digitalWrite(D5, LOW);
    }
}
void gpsProcess(){
    if(Serial4.available() >= 0){
        while(gps.available(Serial4)){
            gps_fix fix = gps.read();
            if(fix.valid.location){
                gpsurl = "https://maps.google.com/?q=" + String(fix.latitude(), 6) + "," + String(fix.longitude(), 6);
                gpsvalid = "V";
                if((String(fix.latitude()).startsWith("**.***") || String(fix.latitude()).startsWith("**.**")) && String(fix.longitude()).startsWith("**.**")){ //Home 
                    digitalWrite(D6, LOW);
                }
                else if(lockCommand == 0){
                    ledpower = 1;
                    digitalWrite(D6, HIGH);
                }
                else{
                    ledpower = 1;
                    digitalWrite(D6, LOW);
                }
            }
            else{
                gpsvalid = "I";
            }
        }
    }
}
int cloudCommand(String command){
    if(command == "unlock"){
        Serial1.write(tailon, sizeof(tailon));
        digitalWrite(D6, HIGH);
        ledpower = 1;
        lockCommand = 0;
        return 0;
    }
    else if(command == "lock"){
        digitalWrite(D6, LOW);
        lockCommand = 1;
        return 1;
    }
    else if(command == "tailon"){
        Serial1.write(tailon, sizeof(tailon));
        return 2;
    }
    else if(command == "tailoff"){
        Serial1.write(tailoff, sizeof(tailoff));
        return 3;
    }
    else if(command == "cruiseon"){
        Serial1.write(cruiseon, sizeof(cruiseon));
        return 4;
    }
    else if(command == "cruiseoff"){
        Serial1.write(cruiseoff, sizeof(cruiseoff));
        return 5;
    }
    else if(command == "headon"){
        digitalWrite(D6, HIGH);
        return 6;
    }
    else if(command == "headoff"){
        digitalWrite(D6, LOW);
        return 7;
    }
    else if(command == "ledon"){
        ledpower = 1;
        return 8;
    }
    else if(command == "ledoff"){
        ledpower = 0;
        return 9;
    }
    else if(command == "poweroff"){
        digitalWrite(D0, HIGH);
        delay(2000);
        digitalWrite(D0, LOW);
        powerControl = 0;
        return 10;
    }
    else if(command == "poweron"){
        digitalWrite(D0, HIGH);
        delay(2000);
        digitalWrite(D0, LOW);
        powerControl = 0;
        return 11;
    }
    else if(command == "alarm"){
        tone(D2, 20, 5000);
        return 12;
    }
    else if(command == "sleep"){
        System.sleep(SLEEP_MODE_DEEP, 600, SLEEP_NETWORK_STANDBY);
        return 13;
    }
    else if(command == "enablealarm"){
        alarmCommand = 1;
        return 14;
    }
    else if(command == "disablealarm"){
        alarmCommand = 0;
        return 15;
    }
    else if(command == "home"){
        Serial1.write(tailoff, sizeof(tailoff));
        digitalWrite(D6, LOW);
        ledpower = 0;
        return 16;
    }
    else if(command == "reset"){
        System.reset();
        return 100;
    }
    else return -1;
} //Performs Lock/Unlock Commands from Cloud.
