#include <NMEAGPS.h>
#include "Serial4/Serial4.h"
SYSTEM_THREAD(ENABLED);

int alarmCommand = 1, batteryLevel, beepConfirmation, driveStatus, headPowerCommand = 0, ledPowerCommand = 1, lockCommand = 1, nightModeStatus, powerCommand = 1;
char alarmStatus, beepRequest, lockStatus, receiverIndex, receiverMessageIndex, receiverMessageLength;
NMEAGPS gps;
PMIC pmic;
String gpsUrl, gpsValid;
uint16_t calc;
unsigned long lastAlarmMessage, lastPowerAlert, lastCommAttempt;

void setup() {
    Serial1.begin(115200);
    Serial1.halfduplex(true);
    Serial4.begin(9600);

    Particle.function("CloudCommand", cloudCommand);
    //Particle.variable("AvgVelocity", averageVelocity);
    Particle.variable("BatteryLevel", batteryLevel);
    Particle.variable("Coordinates", gpsValid);
    Particle.variable("Location", gpsUrl);
    //Particle.variable("Odometer", odometer);
    //Particle.variable("Temperature", temperature);
    //Particle.variable("Velocity", velocity);
    Particle.variable("DriveStatus", driveStatus);
    Particle.variable("LightsOn", nightModeStatus);
    
    pinMode(A3, INPUT); //Brake Lever Sensor
    pinMode(A4, INPUT); //Motor Throttle Sensor
    pinMode(D0, OUTPUT); //Power Switch
    pinMode(D2, OUTPUT); //Buzer
    pinMode(D3, OUTPUT); //Red (Lock)
    pinMode(D4, OUTPUT); //Green (Unlock)
    pinMode(D5, OUTPUT); //Blue (Connecting)
    pinMode(D6, OUTPUT); //Headlight tied to transistor that connects 5V Power Line to a buck converter to produce 6V-7V.

}
void loop() {
    escControl();
    backgroundProcesses();
    gpsProcess();
}
void escControl(){
    unsigned char motor[] = {0x55, 0xAA, 0x7, 0x20, 0x65, 0x0, 0x4, 0x26, 0x26, 0x0, 0x0, 0x0, 0x0};
    unsigned char motor1[] = {0x55, 0xAA, 0x9, 0x20, 0x64, 0x0, 0x6, 0x26, 0x26, 0x0, 0x0, 0x72, 0x0, 0x0, 0x0};
    unsigned char inforequest[] = {0x55, 0xAA, 0x6, 0x20, 0x61, 0xB0, 0x20, 0x02, 0x28, 0x26, 0x58, 0xFE};
    
    int control = analogRead(A3);
    char brake = map(control, 685, 4095, 0x26, 0xC2);
    motor[8] = brake; 
    motor1[8] = brake;
    
    int value = analogRead(A4);
    char speed = map(value, 685, 4095, 0x26, 0xC2);
    motor[7] = speed;
    motor1[7] = speed;
    
    motor[10] = beepConfirmation;
    motor1[10] = beepConfirmation;
    
    inforequest[8] = speed;
    inforequest[9] = brake;
    
    uint16_t calc = (motor[2] + motor[3] + motor[4] + motor[5] + motor[6] + motor[7] + motor[8] + motor[9] + motor[10]) ^ 0xFFFF;
    char ck1 = hexstring(String(calc, HEX).substring(2,4));
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

    Serial1.write(motor, sizeof(motor)); delay(10);
    Serial1.write(motor, sizeof(motor)); delay(10);
    Serial1.write(motor, sizeof(motor)); delay(10);
    Serial1.write(motor, sizeof(motor)); delay(10);
    if(Serial1.available()) escReader();
    Serial1.write(motor1, sizeof(motor1)); delay(10);
    if(Serial1.available()) escReader();
    Serial1.write(inforequest, sizeof(inforequest)); delay(10);
    if(Serial1.available()) escReader();
    
}
char hexstring(String checkSum){
    char hex = 0;
    if(checkSum.length() == 2){
        if(checkSum.charAt(0) == '1') hex = 0x10;
            else if(checkSum.charAt(0) == '2') hex = 0x20;
            else if(checkSum.charAt(0) == '3') hex = 0x30;
            else if(checkSum.charAt(0) == '4') hex = 0x40;
            else if(checkSum.charAt(0) == '5') hex = 0x50;
            else if(checkSum.charAt(0) == '6') hex = 0x60;
            else if(checkSum.charAt(0) == '7') hex = 0x70;
            else if(checkSum.charAt(0) == '8') hex = 0x80;
            else if(checkSum.charAt(0) == '9') hex = 0x90;
            else if(checkSum.charAt(0) == 'a') hex = 0xA0;
            else if(checkSum.charAt(0) == 'b') hex = 0xB0;
            else if(checkSum.charAt(0) == 'c') hex = 0xC0;
            else if(checkSum.charAt(0) == 'd') hex = 0xD0;
            else if(checkSum.charAt(0) == 'e') hex = 0xE0;
            else if(checkSum.charAt(0) == 'f') hex = 0xF0;
        if(checkSum[1] == '0') hex = 0x0 + hex;
            else if(checkSum.charAt(1) == '1') hex = 0x1 + hex;
            else if(checkSum.charAt(1) == '2') hex = 0x2 + hex;
            else if(checkSum.charAt(1) == '3') hex = 0x3 + hex;
            else if(checkSum.charAt(1) == '4') hex = 0x4 + hex;
            else if(checkSum.charAt(1) == '5') hex = 0x5 + hex;
            else if(checkSum.charAt(1) == '6') hex = 0x6 + hex;
            else if(checkSum.charAt(1) == '7') hex = 0x7 + hex;
            else if(checkSum.charAt(1) == '8') hex = 0x8 + hex;
            else if(checkSum.charAt(1) == '9') hex = 0x9 + hex;
            else if(checkSum.charAt(1) == 'a') hex = 0xA + hex;
            else if(checkSum.charAt(1) == 'b') hex = 0xB + hex;
            else if(checkSum.charAt(1) == 'c') hex = 0xC + hex;
            else if(checkSum.charAt(1) == 'd') hex = 0xD + hex;
            else if(checkSum.charAt(1) == 'e') hex = 0xE + hex;
            else if(checkSum.charAt(1) == 'f') hex = 0xF + hex;
    }
    if(checkSum.length() == 1){
        if(checkSum.charAt(0) == '0') hex = 0x0;
            else if(checkSum.charAt(0) == '1') hex = 0x1;
            else if(checkSum.charAt(0) == '2') hex = 0x2;
            else if(checkSum.charAt(0) == '3') hex = 0x3;
            else if(checkSum.charAt(0) == '4') hex = 0x4;
            else if(checkSum.charAt(0) == '5') hex = 0x5;
            else if(checkSum.charAt(0) == '6') hex = 0x6;
            else if(checkSum.charAt(0) == '7') hex = 0x7;
            else if(checkSum.charAt(0) == '8') hex = 0x8;
            else if(checkSum.charAt(0) == '9') hex = 0x9;
            else if(checkSum.charAt(0) == 'a') hex = 0xA;
            else if(checkSum.charAt(0) == 'b') hex = 0xB;
            else if(checkSum.charAt(0) == 'c') hex = 0xC;
            else if(checkSum.charAt(0) == 'd') hex = 0xD;
            else if(checkSum.charAt(0) == 'e') hex = 0xE;
            else if(checkSum.charAt(0) == 'f') hex = 0xF;
    }
    return hex;
}
void escReader(){
    unsigned char start[] = {0x55, 0xAA, 0xF, 0x24, 0x1, 0x0, 0x4D, 0x49, 0x53, 0x63, 0x6F, 0x6F, 0x74, 0x65, 0x72, 0x35, 0x32, 0x31, 0x39, 0x85, 0xFB}, receiver[64];
    while(Serial1.available()){
        char data = Serial1.read();
        switch(receiverIndex){
            case 0:
                if(data == 0x55){
                    receiver[0] = data;
                    receiverIndex++;
                }
                break;
            case 1:
                if(data == 0xAA){
                    receiver[1] = data;
                    receiverIndex++;
                }
                break;
            case 2:
                receiver[2] = data;
                receiverMessageLength = data + 5;
                receiverIndex++;
                receiverMessageIndex = 3;
                calc = data;
                break;
            case 3:
                receiver[dataIndex] = data;
                dataIndex++;
                if(dataIndex <= receiver[2] + 4) calc += data;
                if(dataIndex == receiver[2] + 6) receiverIndex++;
                break;
            case 4:
                calc ^= 0xFFFF;
                if(calc == (receiver[receiver[2] + 4] + (receiver[receiver[2] + 5] << 8))) receiverIndex++;
                else receiverIndex = 10;
                break;
            case 5: //Separates packets by address.
                switch(receiver[3]){
                    case 0x21:
                        if(receiver[2] == 0x02) Serial1.write(start, sizeof(start));
                        else if(receiver[2] == 0x06){
                            beepRequest = receiver[9];
                            driveStatus = (String(receiver[6], DEC)).toInt();
                            nightModeStatus = (String(receiver[8], DEC)).toInt();
                            if(beepRequest == 0x01){ //Scooter reqest a short beep when BLE controller is connected, the scooter's alarm is activated, or charging begins.
                                tone(D2, 20, 50);
                                beepConfirmation = 0x01;
                            }
                            else if(beepRequest == 0x02){//Scooter request a long beep when cruise is enabled and activated.
                                tone(D2, 20, 100);
                                beepConfirmation = 0x01;
                            }
                            else if(beepRequest == 0x03){//Scooter request a short beep when battery is fully charged.
                                tone(D2, 20, 50);
                                beepConfirmation = 0x01;
                            }
                            else beepConfirmation = 0x0;
                        }
                        receiverIndex = 10;
                        break;
                    case 0x23:
                        if(receiver[2] == 0x22){
                            alarmStatus = receiver[8]; //Reads 0x09 when alarm is active.
                            lockStatus = receiver[10]; //Reads 0x00 when unlocked, 0x02 when locked, and 0x06 when the alarm is active.
                            batteryLevel = (String(receiver[14], DEC)).toInt();
                            char velocity = ((receiver[17] * 256) + receiver[16]) / 1000 / 1.60934; //Reads speed (which is in HEX), converts to mph.
                            char averageVelocity = ((receiver[19] * 265) + receiver[18]) / 1000 / 1.60934;
                            char odometer = ((receiver[22] * 256 * 256) + (receiver[21] * 256) + receiver[20]) / 1000 / 1.60934;
                            char temperature = (((receiver[29] * 256) + receiver[28]) / 10 * 9 / 5) + 32; //Reads temperature, converts to F.
                            if(alarmStatus == 0x09 && alarmCommand == 1){
                                tone(D2, 20, 1000);
                                if(millis() - lastAlarmMessage >= 5000){
                                    Particle.publish("Alarm", "Alarm Alert!");
                                    lastAlarmMessage = millis();
                                }
                            }
                        }
                        receiverIndex = 10;
                        break;
                    default:
                        receiverIndex = 10;
                        break;
                }
            case 10: //Erases receiver array.
                for(int erase = 0; erase <= receiverMessageLength; erase++) receiver[erase] = 0;
                receiverIndex = 0;
                break;
            default:
                receiverIndex = 10;
                break;
        }
    }
}
int cloudCommand(String command){
    unsigned char tailon[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x7D, 0x2, 0x0, 0x59, 0xFF};
    unsigned char tailoff[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x7D, 0x0, 0x0, 0x5B, 0xFF};
    unsigned char cruiseon[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x7C, 0x1, 0x0, 0x5B, 0xFF};
    unsigned char cruiseoff[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x7C, 0x0, 0x0, 0x5C, 0xFF};
    if(command == "unlock"){
        Serial1.write(tailon, sizeof(tailon));
        headPowerCommand = 1;
        ledPowerCommand = 1;
        lockCommand = 0;
        return 0;
    }
    else if(command == "lock"){
        headPowerCommand = 0;
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
        headPowerCommand = 1;
        return 6;
    }
    else if(command == "headoff"){
        headPowerCommand = 0;
        return 7;
    }
    else if(command == "ledon"){
        ledPowerCommand = 1;
        return 8;
    }
    else if(command == "ledoff"){
        ledPowerCommand = 0;
        return 9;
    }
    else if(command == "poweroff"){
        powerCommand = 0;
        return 10;
    }
    else if(command == "poweron"){
        powerCommand = 1;
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
        ledPowerCommand = 0;
        return 16;
    }
    else if(command == "reset"){
        System.reset();
        return 100;
    }
    else return -1;
}
void backgroundProcesses(){
    unsigned char unlock[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x71, 0x1, 0x0, 0x66, 0xFF};
    unsigned char lock [] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x70, 0x1, 0x0, 0x67, 0xFF};
    if((pmic.getSystemStatus() & 0x04) == 0 && powerCommand){
        digitalWrite(D0, HIGH);
        delay(100);
        digitalWrite(D0, LOW);
        Particle.publish("ScooterOff", "Electron Reads No Power, Jump Start Initiated...");
    }//Turns Scooter Power On
    else if(!(pmic.getSystemStatus() & 0x04) == 0 && !powerCommand){ 
        if(lockStatus == 0x02 || lockStatus == 0x06) Serial1.write(unlock, sizeof(unlock));
        if(ledPowerCommand == 1) ledPowerCommand = 0;
        if(headPowerCommand == 1) headPowerCommand = 0;
        digitalWrite(D0, HIGH);
        delay(2000);
        digitalWrite(D0, LOW);
    }//Turns Scooter Power Off (Will not turn Power Off if Electron is connected to USB)
    if(nightModeStatus == 0x64){ //Checks if Night Mode is on and turns off if it is.
        digitalWrite(D0, HIGH);
        delay(100);
        digitalWrite(D0, LOW);
    }//Keeps Scooter's Night Mode Off
    if(driveStatus == 0x02){
        digitalWrite(D0, HIGH);
        delay(100);
        digitalWrite(D0, LOW);
        delay(25);
        digitalWrite(D0, HIGH);
        delay(100);
        digitalWrite(D0, LOW);
    }//Keeps Scooter Out of ECO Mode
    if(headPowerCommand == 1){
        digitalWrite(D6, HIGH);
    }
    else if(headPowerCommand == 0){
        digitalWrite(D6, LOW);
    }
    if(lockStatus == 0x00 && lockCommand == 0x01){
        Serial1.write(lock, sizeof(lock));
    }
    else if((lockStatus == 0x02 || lockStatus == 0x06) && lockCommand == 0x00){
        Serial1.write(unlock, sizeof(unlock));
    }
    if(lockStatus == 0x0 && Particle.connected() && ledPowerCommand == 1){
        digitalWrite(D3, LOW);
        digitalWrite(D4, HIGH);
        digitalWrite(D5, LOW);
    }
    else if(lockStatus == 0x02 && Particle.connected() && ledPowerCommand == 1){
        //digitalWrite(D3, HIGH); LED broken
        digitalWrite(D4, LOW);
        digitalWrite(D5, LOW);
    }
    else if(lockStatus == 0x06 && Particle.connected() && ledPowerCommand == 1){
        digitalWrite(D3, LOW);
        digitalWrite(D4, LOW);
        digitalWrite(D5, HIGH);
    }
    else if(!Particle.connected() && ledPowerCommand == 1){
        digitalWrite(D3, LOW);
        digitalWrite(D4, LOW);
        digitalWrite(D5, HIGH);
    }
    else if(ledPowerCommand == 0){
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
                gpsUrl = "https://maps.google.com/?q=" + String(fix.latitude(), 6) + "," + String(fix.longitude(), 6);
                gpsValid = "V";
            }
            else{
                gpsValid = "I";
            }
        }
    }
}
