/*
 * Project: M365 Connect
 * Description: Scooter Firmware
 * Board Version: 5
 * Author: Cameron
 * Date: March 25, 2019
 */
 
 //System Configuration
#include <NMEAGPS.h>
#if PLATFORM_ID == 10
#include "Serial5/Serial5.h"
#endif
SYSTEM_THREAD(ENABLED);

//Pin Configuration.
int brakeInput = A5;
int throttleInput = A4;
int RLED = D5;
int GLED = D4;
int BLED = D3;
int buzzer = D2;
int headlight = D6;
int powerSwitch = D7;

//External Variables.
int gpsValid = 0, isPowered = 0, isConnected = 0, hasConnected = 0,
    brakeConnected = 0, throttleConnected = 0, hasBattery = 0;

//Internal Variables.
ApplicationWatchdog wd(60000, System.reset);

Timer inputMonitor(20, inputReader);
retained int minBrake = 995, minSpeed = 995, maxBrake = 3000, maxSpeed = 3000;

Timer messageManager(20, messageCreator);
int messageIndex;
bool messageManagerActive = false;

char jsonDiagnosticBuffer[255];
char jsonMainBuffer[255];
char jsonControlBuffer[255];

unsigned long lastMainPublishTimeStamp;
unsigned long lastControlPublishTimeStamp;
unsigned long lastDiagnosticPublishTimeStamp;
unsigned long lastBatteryCheckTimeStamp;
unsigned long lastValidMessageTimeStamp;

//Command Variables.
retained int publishLocation = 0, publishMain = 0, publishControl = 0, publishDiagnostic = 0;
int powerCommand = 1, resetCommand = 0, safeMode = 0;
retained int alarmCommand = 1, ledCommand = 1, lockCommand = 1;
retained int cruiseCommand, ecoCommand, ecoModeCommand, headCommand, nightCommand, tailCommand;

//Statistic Variables.
int alarmStatus, averageVelocity, battery, beep, cruise, eco, ecoMode, electronBatterySoC,
    led, lock, night, odometer, tail, temperature, velocity;
retained String gpsLocation = "";
    
void setup(){
    delay(1000);
    Serial1.begin(115200);
    Serial1.halfduplex(true);
    
    if(PLATFORM_ID == 10){
        Serial5.begin(9600);
        attachInterrupt(LOW_BAT_UC, batteryInterrupt, FALLING);
    }

    Particle.function("CloudCommand", cloudCommand);
    
    Particle.variable("Version-v6e", "");
    Particle.variable("publishMain", publishMain);
    Particle.variable("publishControl", publishControl);
    Particle.variable("publishLocation", publishLocation);
    Particle.variable("publishDiagnostic", publishDiagnostic);
    
    pinMode(RLED, OUTPUT);
    pinMode(GLED, OUTPUT);
    pinMode(BLED, OUTPUT);
    pinMode(buzzer, OUTPUT);
    pinMode(headlight, OUTPUT);
    pinMode(powerSwitch, OUTPUT);

    Particle.connect();
    waitFor(Particle.connected, 7000);
    inputMonitor.start();
}
void loop(){
    static PMIC pmic;
    static unsigned long lastMessageSentTimeStamp;
    
    if(millis() - lastValidMessageTimeStamp < 3000) isConnected = 1;
    else isConnected = 0;

    if(!(pmic.getSystemStatus() & 0x04) == 0) isPowered = 1;
    else isPowered = 0;

    if(isPowered){ //If powered through USB/VIN
        if(messageManagerActive && !isConnected){ //Was connected, but now is not connected. Powered off, or cord disconnected.
            messageManager.stop();
            messageManagerActive = false;
            Serial.println("Stopping Timer.");
        }
        else if(!messageManagerActive && isConnected && millis() - lastMessageSentTimeStamp > 50){ //Timer was off, but just received a message.
            messageManager.start();
            messageManagerActive = true;
            Serial.println("Starting Timer.");
        }
        else if(!messageManagerActive && !isConnected && millis() - lastMessageSentTimeStamp > 3000){ //Timer is not active, and are not connected.
            messageIndex = 4;
            messageManager.start();
            lastMessageSentTimeStamp = millis();
            Serial.println("Sending Message.");
            if(millis() - lastMessageSentTimeStamp > 40) messageManager.stop();
        }
    }
    else{
        backgroundProcess();
        if(powerCommand && millis() - lastMessageSentTimeStamp > 1000 && millis() > 10000){
            delay(450);
            digitalWrite(powerSwitch, HIGH);
            delay(100);
            digitalWrite(powerSwitch, LOW);
            delay(450);
        }
    }
    
    wd.checkin();
}

void batteryInterrupt(){
    if(millis() - lastBatteryCheckTimeStamp < 100) hasBattery = 0;
    else hasBattery = 1;
    lastBatteryCheckTimeStamp = millis();
}
bool getBattery(){
    if(millis() - lastBatteryCheckTimeStamp < 100) return hasBattery;
    else return 1;
}

void inputReader(){
    static int brakeAverage[99], speedAverage[99], inputIndex = 0;
    brakeAverage[inputIndex] = analogRead(brakeInput); //Record to array of 255 from input every 5 miliseconds.
    speedAverage[inputIndex] = analogRead(throttleInput);
    
    inputIndex++;
    if(inputIndex >= 99){
        inputIndex = 0;
        inputProcessor(brakeAverage, speedAverage);
    }
}
void inputProcessor(int brakeAverage[99], int speedAverage[99]){
    static long lastBrakeDisconnectionTimeStamp = 5000, lastSpeedDisconnectionTimeStamp = 5000;
    long average = 0;
    int divisor = 0, max = 0, min = 9999;

    for(int x = 0; x < 100; x++){
        if(brakeAverage[x] <= 5000){
            average += brakeAverage[x];
            divisor++;
            if(brakeAverage[x] < min) min = brakeAverage[x];
            if(brakeAverage[x] > max) max = brakeAverage[x];
        }
    }
    average /= divisor;

    if(average < 15){
        brakeConnected = 0;
        minBrake = 1200;
        maxBrake = 3000;
        lastBrakeDisconnectionTimeStamp = millis() + 2000;
    }
    else{
        brakeConnected = 1;
        if(average < minBrake || millis() - lastBrakeDisconnectionTimeStamp < 3000) minBrake = average;
        if(average > maxBrake) maxBrake = average;
    }
    
    /*Serial.print("Read: "); Serial.print(analogRead(brakeInput));
    Serial.print(" Min: "); Serial.print(min); 
    Serial.print(" Max: "); Serial.print(max);
    Serial.print(" Average: "); Serial.print(average); 
    Serial.print(" MinBrake: "); Serial.print(minBrake);
    Serial.print(" MaxBrake: "); Serial.print(maxBrake);
    Serial.print(" Connected: "); Serial.println(brakeConnected);*/
    
    average = 0;
    divisor = 0, max = 0, min = 9999;

    for(int x = 0; x < 100; x++){
        if(speedAverage[x] <= 5000){
            average += speedAverage[x];
            divisor++;
            if(speedAverage[x] < min) min = speedAverage[x];
            if(speedAverage[x] > max) max = speedAverage[x];
        }
    }
    average /= divisor;

    if(average < 15){
        throttleConnected = 0;
        minSpeed = 1200;
        maxSpeed = 3000;
        lastSpeedDisconnectionTimeStamp = millis() + 2000;
    }
    else{
        throttleConnected = 1;
        if(average < minSpeed || millis() - lastSpeedDisconnectionTimeStamp < 3000) minSpeed = average;
        if(average > maxSpeed) maxSpeed = average;
    }
    
    Serial.print("Read: "); Serial.print(analogRead(throttleInput));
    Serial.print(" Min: "); Serial.print(min); 
    Serial.print(" Max: "); Serial.print(max);
    Serial.print(" Average: "); Serial.print(average); 
    Serial.print(" MinSpeed: "); Serial.print(minSpeed);
    Serial.print(" MaxSpeed: "); Serial.print(maxSpeed);
    Serial.print(" Connected: "); Serial.println(throttleConnected);
    
}

void backgroundProcess(){
    static long lastBackgroundProcessTimeStamp;
    static long lastConnectionPublishTimeStamp;
    
    Serial.println("Background Called");
    
    //Process LED Changes.
    if(!ledCommand){
        digitalWrite(RLED, LOW);
        digitalWrite(GLED, LOW);
        digitalWrite(BLED, LOW);
    }
    else{
        if(!Particle.connected()){
            digitalWrite(RLED, LOW);
            digitalWrite(GLED, LOW);
            digitalWrite(BLED, HIGH);
        }
        else{
            if(lock){
                digitalWrite(RLED, HIGH);
                digitalWrite(GLED, LOW);
                digitalWrite(BLED, LOW);
            }
            else{
                digitalWrite(RLED, LOW);
                digitalWrite(GLED, HIGH);
                digitalWrite(BLED, LOW);
            }
        }
        
    }

    //Get Electron Battery Statistics.
    FuelGauge fuel;
    electronBatterySoC = (int) fuel.getSoC();
    hasBattery = getBattery();

    //Process Time Sensitive Commands.
    if(millis() - lastBackgroundProcessTimeStamp > 1000){
        //Process Reset Command.
        if(resetCommand) System.reset();
        
        //Process Safe Mode Command.
        if(safeMode){
            System.enterSafeMode();
            safeMode = 0;
        }

        //Process Power Off Procedure.
        if(isPowered && !powerCommand){
            if(messageManagerActive) messageManager.stop();
            messageManagerActive = false;
            delay(20);
            digitalWrite(powerSwitch, HIGH);
            delay(2000);
            digitalWrite(powerSwitch, LOW);
        }


        //Process Nightmode Changes.
        if((night && !nightCommand) || (!night && nightCommand)){
            digitalWrite(powerSwitch, HIGH);
            delay(100);
            digitalWrite(powerSwitch, LOW);
        }

        //Process Eco Changes.
        if(((!eco || eco == 0x01) && ecoCommand) || ((eco == 0x02 || eco == 0x03) && !ecoCommand)){
            digitalWrite(powerSwitch, HIGH);
            delay(100);
            digitalWrite(powerSwitch, LOW);
            delay(25);
            digitalWrite(powerSwitch, HIGH);
            delay(100);
            digitalWrite(powerSwitch, LOW);
        }

        //Process Headlight Changes.
        if((headCommand || night) && !lock) digitalWrite(headlight, HIGH);
        else digitalWrite(headlight, LOW);

        lastBackgroundProcessTimeStamp = millis();
    }
    
    //Publish Disconnection Alert
    if(millis() - lastConnectionPublishTimeStamp > 600000 && hasConnected && !isConnected && powerCommand){
        Particle.publish("disconnected");
    }
    
    //Publish statistics for App Main
    if(publishMain){
        if(millis() - lastMainPublishTimeStamp > 30000){
            sprintf(jsonMainBuffer, "{\"0\": %u, \"1\": %u, \"2\": %u, \"3\": %u, \"4\": %u, \"5\": \"%s\"}", 
            isPowered, isConnected, lockCommand, battery, gpsValid, (const char*) gpsLocation);
            
            Particle.publish("main", jsonMainBuffer, 300, PRIVATE);
            lastMainPublishTimeStamp = millis();
        }
    }
    
    //Publish statistics for App Control
    if(publishControl){
        if(millis() - lastControlPublishTimeStamp > 30000){
            sprintf(jsonControlBuffer, "{\"0\": %u, \"1\": %u, \"2\": %u, \"3\": %u, \"4\": %u, \"5\": %u, \"6\": %u, \"7\": %u, \"8\": %u, \"9\": %u, \"10\": %u, \"11\": %u, \"12\": %u, \"13\": %u, \"14\": %u, \"15\": %u, \"16\": %u, \"17\": %u, \"18\": %u, \"19\": %u, \"20\": %u}",
                isPowered, isConnected, hasConnected, hasBattery, brakeConnected, throttleConnected,
                alarmCommand, cruiseCommand, ecoCommand, ecoModeCommand, nightCommand, headCommand, tailCommand,
                ledCommand, lockCommand, battery, electronBatterySoC, velocity, averageVelocity,
                odometer, temperature);
            
            Particle.publish("control", jsonControlBuffer, 300, PRIVATE);
            lastControlPublishTimeStamp = millis();
        }
    }

    //Publish Diagnostic
    if(millis() - lastDiagnosticPublishTimeStamp > 5000 && publishDiagnostic){
        sprintf(jsonDiagnosticBuffer, "{\"0\": %u, \"1\": %u, \"2\": %u, \"3\": %u, \"4\": %u, \"5\": %u, \"6\": %u, \"7\": %u, \"8\": %u, \"9\": %u, \"10\": %u, \"11\": %u}", 
            isPowered, isConnected, hasConnected, hasBattery, brakeConnected, throttleConnected, minBrake, maxBrake, analogRead(brakeInput), minSpeed, maxSpeed, analogRead(throttleInput));
            
        Particle.publish("diagnostic", jsonDiagnosticBuffer, 300, PRIVATE);
        lastDiagnosticPublishTimeStamp = millis();
    }

    //Process Cloud Changes.
    Particle.process();

}




void messageCreator(){
	char brake = map(analogRead(brakeInput), minBrake, maxBrake, 0x26, 0xC2);
	char speed = map(analogRead(throttleInput), minSpeed, maxSpeed, 0x26, 0xC2);
	uint16_t calc = 0;

	if(!brakeConnected || brake < 0x26) brake = 0x26;
	if(!throttleConnected || brake < 0x26) speed = 0x26;
	
	//Serial.print("Brake Sent: "); Serial.print(brake, HEX);
	//Serial.print(" Speed Sent: "); Serial.println(speed, HEX);

	switch(messageIndex){
        case 0:
        case 1:
        case 2:
        case 3:{
            unsigned char message[] = {0x55, 0xAA, 0x7, 0x20, 0x65, 0x0, 0x4, speed, brake, 0x0, beep, 0x0, 0x0};
            for(int x = 2; x < sizeof(message) - 2; x++) calc += message[x];
            calc ^= 0xffff;
            message[11] = (uint8_t)(calc&0xff);
            message[12] = (uint8_t)((calc&0xff00) >> 8);
            Serial1.write(message, sizeof(message));
            Serial1.flush();
            if(beep == 1) beep = 0;
            messageIndex++;
            break;
        }
        case 4:{
            unsigned char message[] = {0x55, 0xAA, 0x9, 0x20, 0x64, 0x0, 0x6, speed, brake, 0x0, beep, 0x72, 0x0, 0x0, 0x0};
            for(int x = 2; x < sizeof(message) - 2; x++) calc += message[x];
            calc ^= 0xffff;
            message[13] = (uint8_t)(calc&0xff);
            message[14] = (uint8_t)((calc&0xff00) >> 8);
            Serial1.write(message, sizeof(message));
            Serial1.flush();
            if(beep == 1) beep = 0;
            messageIndex++;
            break;
        }
        case 5:{
            unsigned char message[] = {0x55, 0xAA, 0x6, 0x20, 0x61, 0xB0, 0x20, 0x02, speed, brake, 0x0, 0x0};
            for(int x = 2; x < sizeof(message) - 2; x++) calc += message[x];
            calc ^= 0xffff;
            message[10] = (uint8_t)(calc&0xff);
            message[11] = (uint8_t)((calc&0xff00) >> 8);
            Serial1.write(message, sizeof(message));
            Serial1.flush();
            messageIndex++;
            break;
        }
        case 6:{
            unsigned char message[] = {0x55, 0xAA, 0x6, 0x20, 0x61, 0x7B, 0x4, 0x2, speed, brake, 0x0, 0x0};
            for(int x = 2; x < sizeof(message) - 2; x++) calc += message[x];
            calc ^= 0xffff;
            message[10] = (uint8_t)(calc&0xff);
            message[11] = (uint8_t)((calc&0xff00) >> 8);
            Serial1.write(message, sizeof(message));
            Serial1.flush();
            messageIndex++;
            break;
        }
        case 7:{
            unsigned char message[] = {0x55, 0xAA, 0x6, 0x20, 0x61, 0x7D, 0x2, 0x2, speed, brake, 0x0, 0x0};
            for(int x = 2; x < sizeof(message) - 2; x++) calc += message[x];
            calc ^= 0xFFFF;
            message[10] = (uint8_t)(calc&0xff);
            message[11] = (uint8_t)((calc&0xff00) >> 8);
            Serial1.write(message, sizeof(message));
            Serial1.flush();
            messageIndex = 0;
			break;
        }
    }
}
void serialEvent1(){
    static char readerIndex, dataIndex;
    static unsigned char message[64];
    static unsigned long lastAlarmMessageTimeStamp = 0;
    static uint16_t readerCalc;
    
    char data = Serial1.read();
    switch(readerIndex){
        case 0:
            if(data == 0x55) readerIndex++;
            break;
        case 1:
            if(data == 0xAA) readerIndex++;
            else readerIndex = 0;
            break;
        case 2:
            message[2] = data;
            readerCalc = data;
            dataIndex = 3;
            readerIndex++;
            break;
        case 3:
            message[dataIndex] = data;
            dataIndex++;
            if(dataIndex <= message[2] + 4) readerCalc += data;
            if(dataIndex == message[2] + 6) readerIndex++;
            if(message[3] == 0x20) readerIndex = 10;
            break;
        case 4:
            readerCalc ^= 0xFFFF;
            if(readerCalc == (message[message[2] + 4] + (message[message[2] + 5] << 8))) readerIndex++;
            else readerIndex = 10;
            break;
        case 5: //Separates packets by address.
            if(!hasConnected) hasConnected = 1;
            lastValidMessageTimeStamp = millis();
            
            for(int x = 0; x < message[2] + 6; x++){
                Serial.print(message[x], HEX); Serial.print(" ");
            } Serial.println();
            
            switch(message[3]){
                case 0x21:
                    switch(message[4]){
                        case 0x01:{
                            switch(message[5]){
                                case 0x0:{
                                    unsigned char start[] = {0x55, 0xAA, 0xF, 0x24, 0x1, 0x0, 0x4D, 0x49, 0x53, 0x63, 0x6F, 0x6F, 0x74, 0x65, 0x72, 0x35, 0x32, 0x31, 0x39, 0x85, 0xFB};
                                    if(messageManagerActive) messageManager.stop();
                                    delay(10);
                                    Serial1.write(start, sizeof(start));
                                    delay(10);
                                    if(messageManagerActive) messageManager.start();
                                    break;
                                }
                                case 0x61:
                                    tail = message[6];
                                    break;
                            }
                            readerIndex = 10;
                            break;
                        }
                        case 0x64:
                            eco = message[6];
                            night = message[8];
                            beep = message[9];

                            if(!message[7]){
                                if(messageManagerActive) messageManager.stop();
                                digitalWrite(powerSwitch, HIGH);
                                delay(100);
                                digitalWrite(powerSwitch, LOW);
                                if(messageManagerActive) messageManager.start();
                            }

                            backgroundProcess();
                            readerIndex = 10;
                            break;
                        default:
                            readerIndex = 10;
                            break;
                    }
                    readerIndex = 10;
                    break;
                case 0x23:
                    switch(message[5]){
                        case 0x7B:
                            ecoMode = message[6];
                            cruise = message[8];

                            //Process Cruise Control Changes.
                            if(!cruiseCommand && cruise) command(4);
                            else if(cruiseCommand && !cruise) command(5);

                            //Process Eco Mode Changes.
                            if(!ecoMode){
                                if(ecoModeCommand == 1) command(7);
                                else if(ecoModeCommand == 2) command(8);
                            }
                            else if(ecoMode == 1){
                                if(!ecoModeCommand) command(6);
                                else if(ecoModeCommand == 2) command(8);
                            }
                            else if(ecoMode == 2){
                                if(!ecoModeCommand) command(6);
                                else if(ecoModeCommand == 1) command(7);
                            }
                            readerIndex = 10;
                            break;
                        case 0x7D:
                            tail = message[6];

                            //Process Taillight Changes.
                            if(tail && !tailCommand) command(2);
                            else if(!tail && tailCommand) command(3);

                            readerIndex = 10;
                            break;
                        case 0xB0:
                            alarmStatus = message[8];
                            lock = message[10];
                            battery = message[14];
                            velocity = ( message[16] + (message[17] * 256)) / 1000 / 1.60934;
                            averageVelocity = (message[18] + (message[19] * 265)) / 1000 / 1.60934;
                            odometer = (message[20] + (message[21] * 256) + (message[22] * 256 * 256)) / 1000 / 1.60934;
                            temperature = ((message[28] + (message[29] * 256)) / 10 * 9 / 5) + 32;

                            //Process Lock Changes.
                            if(lock && !lockCommand) command(0);
                            else if(!lock && lockCommand) command(1);

                            //Process Alarm Alert
                            if(alarmStatus == 0x09 && alarmCommand){
                                tone(buzzer, 20, 400);
                                if(millis() - lastAlarmMessageTimeStamp >= 2000){
                                    Particle.publish("Alarm", "Alarm Alert!");
                                    lastAlarmMessageTimeStamp = millis();
                                }
                            }

                            backgroundProcess();
                            readerIndex = 10;
                            break;
                        default:
                            readerIndex = 10;
                            break;
                    }
                    readerIndex = 10;
                    break;
                default:
                    readerIndex = 10;
                    break;
            }
        case 10: //Erases message array.
            for(int erase = 0; erase <= message[2] + 5; erase++) message[erase] = 0;
            readerIndex = 0;
            break;
        default:
            readerIndex = 10;
            break;
    }
}
void serialEvent5(){
    static NMEAGPS gps;
    static long lastLocationMessageTimeStamp = millis();
    static String lastLocation = "";
    
    while(gps.available(Serial5)){
        gps_fix fix = gps.read();
        if(fix.valid.location){
            gpsValid = 1;
            gpsLocation = String(fix.latitude(), 4) + "," + String(fix.longitude(), 4);
                
            if(millis() - lastLocationMessageTimeStamp > 60000 && publishLocation){
                if(lastLocation != gpsLocation || millis() - lastLocationMessageTimeStamp > 240000){
                    
                    /*char jsonLocation[64];
                    sprintf(jsonLocation, "{\"0\": %s, \"1\": %s}", 
                        String(fix.latitude(), 4), String(fix.longitude(), 4));
                    Particle.publish("gpsTrace", jsonLocation);*/
                    Particle.publish("gpsTrace", gpsLocation);
                    
                    lastLocation = gpsLocation;
                    lastLocationMessageTimeStamp = millis();
                }
            }
        }
        else{
            gpsValid = 0;
            gpsLocation = "";
        }
    }
}

int cloudCommand(String command){
    if(command.equals("unlock")){
        lockCommand = 0;
        return 0;
    }
    else if(command.equals("lock")){
        lockCommand = 1;
        return 1;
    }
    else if(command.equals("tailoff")){
        tailCommand = 0;
        return 2;
    }
    else if(command.equals("tailon")){
        tailCommand = 1;
        return 3;
    }
    else if(command.equals("cruiseoff")){
        cruiseCommand = 0;
        return 4;
    }
    else if(command.equals("cruiseon")){
        cruiseCommand = 1;
        return 5;
    }
    else if(command.equals("ecolow")){
        ecoModeCommand = 0;
        return 6;
    }
    else if(command.equals("ecomed")){
        ecoModeCommand = 1;
        return 7;
    }
    else if(command.equals("ecohigh")){
        ecoModeCommand = 2;
        return 8;
    }
    else if(command.equals("ecooff")){
        ecoCommand = 0;
        return 9;
    }
    else if(command.equals("ecoon")){
        ecoCommand = 1;
        return 10;
    }
    else if(command.equals("nightoff")){
        nightCommand = 0;
        return 11;
    }
    else if(command.equals("nighton")){
        nightCommand = 1;
        return 12;
    }
    else if(command.equals("poweroff")){
        lockCommand = 0;
        powerCommand = 0;
        return 13;
    }
    else if(command.equals("poweron")){
        powerCommand = 1;
        return 14;
    }
    else if(command.equals("reset")){
        resetCommand = 1;
        return 15;
    }
    else if(command.equals("headoff")){
        headCommand = 0;
        return 16;
    }
    else if(command.equals("headon")){
        headCommand = 1;
        return 17;
    }
    else if(command.equals("ledoff")){
        ledCommand = 0;
        return 18;
    }
    else if(command.equals("ledon")){
        ledCommand = 1;
        return 19;
    }
    else if(command.equals("alarmoff")){
        alarmCommand = 0;
        return 20;
    }
    else if(command.equals("alarmon")){
        alarmCommand = 1;
        return 21;
    }
    else if(command.equals("alarm")){
        tone(buzzer, 20, 5000);
        return 22;
    }
    else if(command.equals("controlon")){
          publishControl = 1;
          publishMain = 0;
          lastControlPublishTimeStamp = 0;
          return 23;
    }
    else if(command.equals("controloff")){
        publishControl = 0;
        return 24;
    }
    else if(command.equals("safemode")){
        Particle.disconnect();
        safeMode = 1;
        return 25;
    }
    else if(command.equals("gpson")){
        publishLocation = 1;
        if(!gpsLocation.equals("")) Particle.publish("gpsTrace", gpsLocation);
        return 26;
    }
    else if(command.equals("gpsoff")){
        publishLocation = 0;
        return 27;
    }
    else if(command.equals("diagnosticon")){
        publishDiagnostic = 1;
        lastDiagnosticPublishTimeStamp = 0;
        return 28;
    }
    else if(command.equals("diagnosticoff")){
        publishDiagnostic = 0;
        return 29;
    }
    else if(command.equals("mainoff")){
        publishMain = 0;
        return 30;
    }
    else if(command.equals("mainon")){
        publishMain = 1;
        publishControl = 0;
        lastMainPublishTimeStamp = 0;
        return 31;
    }
    else return -1;
}
void command(int command){
    if(messageManagerActive) messageManager.stop();
    delay(10);
    switch(command){
        case 0:{
            unsigned char message[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x71, 0x1, 0x0, 0x66, 0xFF};
            Serial1.write(message, sizeof(message));
            Serial1.flush();
            break;
        }
        case 1:{
            unsigned char message[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x70, 0x1, 0x0, 0x67, 0xFF};
            Serial1.write(message, sizeof(message));
            Serial1.flush();
            break;
        }
        case 2:{
            unsigned char message[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x7D, 0x0, 0x0, 0x5B, 0xFF};
            Serial1.write(message, sizeof(message));
            Serial1.flush();
            break;
        }
        case 3:{
            unsigned char message[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x7D, 0x2, 0x0, 0x59, 0xFF};
            Serial1.write(message, sizeof(message));
            Serial1.flush();
            break;
        }
        case 4:{
            unsigned char message[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x7C, 0x0, 0x0, 0x5C, 0xFF};
            Serial1.write(message, sizeof(message));
            Serial1.flush();
            break;
        }
        case 5:{
            unsigned char message[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x7C, 0x1, 0x0, 0x5B, 0xFF};
            Serial1.write(message, sizeof(message));
            Serial1.flush();
            break;
        }
        case 6:{
            unsigned char message[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x7B, 0x00, 0x0, 0x5D, 0xFF};
            Serial1.write(message, sizeof(message));
            Serial1.flush();
            break;
        }
        case 7:{
            unsigned char message[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x7B, 0x01, 0x0, 0x5C, 0xFF};
            Serial1.write(message, sizeof(message));
            Serial1.flush();
            break;
        }
        case 8:{
            unsigned char message[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x7B, 0x02, 0x0, 0x5B, 0xFF};
            Serial1.write(message, sizeof(message));
            Serial1.flush();
            break;
        }
    }
    delay(10);
    if(messageManagerActive) messageManager.start();
}

