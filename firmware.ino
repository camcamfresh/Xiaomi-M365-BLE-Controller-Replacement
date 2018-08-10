SYSTEM_THREAD(ENABLED); //Prevents loss of cell signal from interupting the program.

char speed, brake, ck1, ck2, ck3, ck4;
unsigned char motor[] = {0x55, 0xAA, 0x7, 0x20, 0x65, 0x0, 0x4, speed, brake, 0x0, 0x0, ck1, ck2};
unsigned char motor1[] = {0x55, 0xAA, 0x9, 0x20, 0x64, 0x0, 0x6, speed, brake, 0x0, 0x0, 0x72, 0x0, ck3, ck4};
unsigned char lock [] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x70, 0x1, 0x0, 0x67, 0xFF};
unsigned char unlock[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x71, 0x1, 0x0, 0x66, 0xFF};
unsigned char tailon[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x7D, 0x2, 0x0, 0x59, 0xFF};
unsigned char tailoff[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x7D, 0x0, 0x0, 0x5B, 0xFF};
unsigned char cruiseon[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x7C, 0x1, 0x0, 0x5B, 0xFF};
unsigned char cruiseoff[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x7C, 0x0, 0x0, 0x5C, 0xFF};

void setup() {
    Serial1.begin(115200); //ESC Serial Start
    Serial1.halfduplex(true); //ESC One Wire Communication
    Particle.function("CloudCommand", cloudCommand);
    pinMode(A1, INPUT); //Brake Lever Sensor
    pinMode(A2, INPUT); //Motor Throttle Sensor
    pinMode(D0, INPUT); //Power Monitor
    pinMode(D1, OUTPUT); //Power    
}
void loop() {
    esccontrol();
    powercheck();
}
void esccontrol(){
    int control = analogRead(A1); //Get Brake Values and Store into Packets (HEX)
    brake = map(control, 685, 3000, 0, 0xB0); //Converts Brake Lever Signal into HEX Value
    if(brake <= 0x26) brake = 0x26;

    motor[8] = brake; //Input Brake lever HEX value to the command.
    motor1[8] = brake; 
    
    int value = analogRead(A2); //Get Throttle Values and Store into Packets (HEX)
    speed = map(value, 685, 3000, 0, 0xB0); //Converts Throttle Lever Signal into HEX Value
    if(speed <= 0x26) speed = 0x26;

    motor[7] = speed; //Input Throttle lever HEX value to the command.
    motor1[7] = speed;
    
    //Calculates CK0 and CK1 for motor
    uint16_t calccrc =  (motor[2] + motor[3] + motor[4] + motor[5] + motor[6] + motor[7] + motor[8] + motor[9] + motor[10]) ^ 0xFFFF;
    String crc = String(calccrc, HEX);
    ck1 = hexstring(crc.substring(2,4)); //Reverses Result and Converts to HEX Value
    ck2 = hexstring(crc.substring(0,2));
    motor[11] = ck1; //Input CK HEX value to the command/
    motor[12] = ck2;
    
    //Calcculate CRC for motor1
    uint16_t calccrc1 = (motor1[2] + motor1[3] + motor1[4] + motor1[5] + motor1[6] + motor1[7] + motor1[8] + motor1[9] + motor1[10] + motor1[11] + motor1[12]) ^ 0xFFFF;
    String crc1 = String(calccrc1, HEX);
    ck3 = hexstring(crc1.substring(2,4));
    ck4 = hexstring(crc1.substring(0,2));
    motor1[13] = ck3; //Input CK HEX value to the command/
    motor1[14] = ck4;

    //Send Packets to ESC
    Serial1.write(motor1, sizeof(motor1)); delay(10);
    Serial1.write(motor, sizeof(motor)); delay(10);
    Serial1.write(motor, sizeof(motor)); delay(10);
    Serial1.write(motor, sizeof(motor)); delay(10);
    Serial1.write(motor, sizeof(motor)); delay(10);    
}
/* The scooter will automatically turn off every 5 minutes, when not in use. This ensures power is always on. If the power
turns off, a battery connected to the replacement microcontroller provides enough power to activate a transistor and restore power. */
void powercheck(){
    if(digitalRead(D0) == LOW){
        digitalWrite(D1, HIGH);
        delay(100);
        digitalWrite(D1, LOW);
    }
}
int cloudCommand(String com){
    if(com == "unlock"){
        Serial1.write(unlock, sizeof(unlock));
        return 0;
    }
    else if(com == "lock"){
        Serial1.write(lock, sizeof(lock));
        return 1;
    }
    else if(com == "tailoff"){
        Serial1.write(tailoff, sizeof(tailoff));
        return 2;
    }
    else if(com == "tailon"){
        Serial1.write(tailon, sizeof(tailon));
        return 3;
    }
    else if(com == "cruiseon"){
        Serial1.write(cruiseon, sizeof(cruiseon));
        return 4;
    }
    else if(com == "cruiseoff"){
        Serial1.write(cruiseoff, sizeof(cruiseoff));
        return 5;
    }
    else if(com == "power"){
        digitalWrite(D1, HIGH);
        delay(2000);
        digitalWrite(D1, LOW);
        return 6;
    }
} //Performs Commands from Internet.
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
        if(data[1] == '1') hex = 0x01;
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
    if(data.length() < 2){
        if(data.charAt(0) == '1') hex = 0x1;
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
} //Converts a String to actual HEX.
