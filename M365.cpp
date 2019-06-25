#include "M365.h"
M365::M365(){
    messageTimer = new Timer(20, &M365::send, *this);
}
M365::~M365(){ delete messageTimer; }

void M365::setup(USARTSerial & USARTPort, String brake, String throttle, String buzzer, 
        String rled, String gled, String bled, String headlight, String power){
    setSerial(USARTPort);
    setBrake(brake);
    setThrottle(throttle);
    setBuzzer(buzzer);
    setRGB(rled, gled, bled);
    setHeadlight(headlight);
    setPower(power);
}
void M365::setSerial(USARTSerial & USARTPort){
    serial = &USARTPort;
    serial->begin(115200);
    serial->halfduplex(true);
    messageTimer->start();
}
void M365::setBrake(String brake){
    if(brake.startsWith("A")) input.brake = setAnalogPin(brake.charAt(1) - '0');
}
void M365::setThrottle(String throttle){
    if(throttle.startsWith("A")) input.throttle = setAnalogPin(throttle.charAt(1) - '0');
}
void M365::setBuzzer(String buzzer){
    if(buzzer.startsWith("D")) input.buzzer = setDigitalPin(buzzer.charAt(1) - '0');
}
void M365::setHeadlight(String headlight){
    if(headlight.startsWith("D")) input.headlight = setDigitalPin(headlight.charAt(1) - '0');
}
void M365::setRGB(String r, String g, String b){
    if(r.startsWith("D")) input.rled = setDigitalPin(r.charAt(1) - '0');
    if(g.startsWith("D")) input.gled = setDigitalPin(g.charAt(1) - '0');
    if(b.startsWith("D")) input.bled = setDigitalPin(b.charAt(1) - '0');
}
void M365::setPower(String power){
    if(power.startsWith("D")) input.power = setDigitalPin(power.charAt(1) - '0');
}

void M365::process(){
    while(serial->available())
        read(serial->read());
}
void M365::setCommand(const command_t * newCommands){
    command = *newCommands;
}
void M365::getStats(stats_t * oldStats){
    *oldStats = stats;
}

void M365::send(){
    static uint8_t messageType = 0;
    unsigned char brake = getBrake();
    unsigned char speed = getThrottle();
    
    if(!isConnected()) messageType = 4;

    switch(messageType++){
        case 0:
        case 1:
        case 2:
        case 3:{
            unsigned char message[] = {0x55, 0xAA, 0x7, 0x20, 0x65, 0x0, 0x4, speed, brake, 0x0, stats.beep, 0x0, 0x0};
            addSum(message, sizeof(message));
            transmit(message, sizeof(message));
            if(stats.beep) stats.beep = 0;
            break;
        }
        case 4:{
            unsigned char message[] = {0x55, 0xAA, 0x9, 0x20, 0x64, 0x0, 0x6, speed, brake, 0x0, stats.beep, 0x72, 0x0, 0x0, 0x0};
            addSum(message, sizeof(message));
            transmit(message, sizeof(message), true);
            if(stats.beep) stats.beep = 0;
            break;
        }
        case 5:{
            unsigned char message[] = {0x55, 0xAA, 0x6, 0x20, 0x61, 0xB0, 0x20, 0x02, speed, brake, 0x0, 0x0};
            addSum(message, sizeof(message));
            transmit(message, sizeof(message));
            break;
        }
        case 6:{
            unsigned char message[] = {0x55, 0xAA, 0x6, 0x20, 0x61, 0x7B, 0x4, 0x2, speed, brake, 0x0, 0x0};
            addSum(message, sizeof(message));
            transmit(message, sizeof(message));
            break;
        }
        case 7:{
            unsigned char message[] = {0x55, 0xAA, 0x6, 0x20, 0x61, 0x7D, 0x2, 0x2, speed, brake, 0x0, 0x0};
            addSum(message, sizeof(message));
            transmit(message, sizeof(message));
            messageType = 0;
    		break;
        }
    }
}
void M365::addSum(unsigned char * message, int size){
    unsigned long cksm = 0;
    for(int i = 2; i < size - 2; i++) cksm += message[i];
    cksm ^= 0xFFFF;
    message[size - 2] = (unsigned char)(cksm&0xFF);
    message[size - 1] = (unsigned char)((cksm&0xFF00) >> 8);
}
void M365::transmit(const unsigned char * message, int size, bool override){
    if(isConnected() || override){
        serial->write(message, size);
        serial->flush();
    }
}

void M365::read(char current_byte){
    serialBuffer[serialIndex] = current_byte;
        
    if(serialIndex && serialBuffer[serialIndex - 1] == 0x55 && serialBuffer[serialIndex] == 0xAA){
        if(check(serialBuffer, serialIndex - 1)) translate(serialBuffer, serialIndex - 3);
        serialIndex = 0;
    }
    else serialIndex > 255 ? serialIndex = 0 : serialIndex++;
}
bool M365::check(const char * message, int size){
    unsigned long cksm = 0;
    for(int i = 0; i < size - 2; i++) cksm += message[i];
    cksm ^= 0xFFFF;
    
    if(cksm == message[size - 2] + (message[size - 1] << 8)){
        if(message[1] == 0x20) return false;
        lastValidMessageTimeStamp = millis();        
        return true;
    }
    return false;
}
void M365::translate(const char * message, int size){
    switch(message[1]){
        case 0x21:
            switch(message[2]){
                case 0x01:
                    if(message[2] == 0x61) stats.tail = message[3];
                    break;
                case 0x64:
                    stats.eco = message[4];
                    stats.led = message[5];
                    stats.night = message[6];
                    stats.beep = message[7];
                    break;
            }
            break;
        case 0x23:
            switch(message[3]){
                case 0x7B:
                    stats.ecoMode = message[4];
                    stats.cruise = message[6];
                    break;
                case 0x7D:
                    stats.tail = message[4];
                    break;
                case 0xB0:
                    stats.alarmStatus = message[6];
                    stats.lock = message[8];
                    stats.battery = message[12];
                    stats.velocity = ( message[14] + (message[15] * 256)) / 1000 / 1.60934;
                    stats.averageVelocity = (message[16] + (message[17] * 265)) / 1000 / 1.60934;
                    stats.odometer = (message[18] + (message[19] * 256) + (message[20] * 256 * 256)) / 1000 / 1.60934;
                    stats.temperature = ((message[26] + (message[27] * 256)) / 10 * 9 / 5) + 32;
                    if(stats.alarmStatus) tone(input.buzzer, 20, 400);
                    break;
            }
    }
    compare();
}
void M365::compare(){
    static unsigned long lastDelayedBackgroundProcessTimeStamp = 0;
    
    //Process LED Changes
    if(!command.led){
        digitalWrite(input.rled, LOW);
        digitalWrite(input.gled, LOW);
        digitalWrite(input.bled, LOW);
    }
    else{
        if(stats.lock){
            digitalWrite(input.rled, HIGH);
            digitalWrite(input.gled, LOW);
            digitalWrite(input.bled, LOW);
        }
        else{
            digitalWrite(input.rled, LOW);
            digitalWrite(input.gled, HIGH);
            digitalWrite(input.bled, LOW);
        }
    }
    
    //Process Headlight Changes
    if((command.head || stats.night) && !stats.lock) digitalWrite(input.headlight, HIGH);
    else digitalWrite(input.headlight, LOW);
    
    //Process Power Switch Commands (One at a Time with a Delay)
    if(millis() - lastDelayedBackgroundProcessTimeStamp > 1000 && isConnected()){
        //Process Power Off Command
        if(!command.power && isConnected()){
            if(command.led) command.led = 0; //Turn Off Light
            if(command.lock) command.lock = 0; //Required to be unlocked to be manually turned off.
            else{
                digitalWrite(input.power, HIGH);
                delay(2000);
                digitalWrite(input.power, LOW);
            }
        }
        //Process Night Mode Changes
        else if((stats.night && !command.night) || (!stats.night && command.night)){
            digitalWrite(input.power, HIGH);
            delay(100);
            digitalWrite(input.power, LOW);
        }
        
        //Process Eco On/Off Changes
        else if(((!stats.eco || stats.eco == 1) && command.eco) || ((stats.eco == 2 || stats.eco == 3) && !command.eco)){
            digitalWrite(input.power, HIGH);
            delay(100);
            digitalWrite(input.power, LOW);
            delay(50);
            digitalWrite(input.power, HIGH);
            delay(100);
            digitalWrite(input.power, LOW);
        }
        
        //Process Headlight Status when 0 (usually indicates error state).
        else if(!stats.led && stats.battery > 20){
            digitalWrite(input.power, HIGH);
            delay(100);
            digitalWrite(input.power, LOW);
        }
        
        lastDelayedBackgroundProcessTimeStamp = millis();
    } 
    
    //Process Beep Request
    if(stats.beep == 1 && !stats.alarmStatus) tone(input.buzzer, 20, 50);
    else if(stats.beep == 2){
        tone(input.buzzer, 20, 100);
        stats.beep = 1;
    }
    else if(stats.beep == 3){
        tone(input.buzzer, 20, 50);
        stats.beep = 1;
    }
    
    //Process Cruise Change
    if(!command.cruise && stats.cruise){
        unsigned char message[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x7C, 0x0, 0x0, 0x5C, 0xFF};
        transmit(message, sizeof(message));
    }
    else if(command.cruise && !stats.cruise){
        unsigned char message[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x7C, 0x1, 0x0, 0x5B, 0xFF};
        transmit(message, sizeof(message));
    }
    
    //Process Tail Change
    if(stats.tail && !command.tail){
        unsigned char message[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x7D, 0x0, 0x0, 0x5B, 0xFF};
        transmit(message, sizeof(message));
    }
    else if(!stats.tail && command.tail){
        unsigned char message[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x7D, 0x2, 0x0, 0x59, 0xFF};
        transmit(message, sizeof(message));
    }
    
    //Process Eco Mode Change
    if(!stats.ecoMode){
        if(command.ecoMode == 1){
            unsigned char message[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x7B, 0x01, 0x0, 0x5C, 0xFF};
            transmit(message, sizeof(message));
        }
        else if(command.ecoMode == 2){
            unsigned char message[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x7B, 0x02, 0x0, 0x5B, 0xFF};
            transmit(message, sizeof(message));
        }
    }
    else if(stats.ecoMode == 1){
        if(!command.ecoMode){
            unsigned char message[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x7B, 0x00, 0x0, 0x5D, 0xFF};
            transmit(message, sizeof(message));
        }
        else if(command.ecoMode == 2){
            unsigned char message[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x7B, 0x02, 0x0, 0x5B, 0xFF};
            transmit(message, sizeof(message));
        }
    }
    else if(stats.ecoMode ==2){
        if(!command.ecoMode){
            unsigned char message[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x7B, 0x00, 0x0, 0x5D, 0xFF};
            transmit(message, sizeof(message));
        }
        else if(command.ecoMode == 1){
            unsigned char message[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x7B, 0x01, 0x0, 0x5C, 0xFF};
            transmit(message, sizeof(message));
        }
    }
    
    //Process Lock Change
    if(stats.lock && !command.lock){
        unsigned char message[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x71, 0x1, 0x0, 0x66, 0xFF};
        transmit(message, sizeof(message));
    }
    else if(!stats.lock && command.lock){
        unsigned char message[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x70, 0x1, 0x0, 0x67, 0xFF};
        transmit(message, sizeof(message));
    }

}

bool M365::isConnected(){
    if(millis() < 1000) return false;
    else if(millis() - lastValidMessageTimeStamp < 1000){
        stats.isConnected = 1;
        stats.hasConnected = 1;
        messageTimer->changePeriod(20);
        return true;
    }
    else if(command.power){
        digitalWrite(input.power, HIGH);
        delay(100);
        digitalWrite(input.power, LOW);
    }
    stats.hasConnected = 0;
    messageTimer->changePeriod(1000);
    return false;
}
int M365::setAnalogPin(int pin){
    if(pin > 0 && pin < 8){
        pinMode(pin, INPUT);
        return pin;
    }
}
int M365::setDigitalPin(int pin){
    if(pin > 0 && pin < 8){
        pinMode(pin, OUTPUT);
        return pin;
    }
}
unsigned char M365::getBrake(){
    uint16_t brake = analogRead(input.brake);
    if(!input.brake || brake < 15){
        stats.brakeConnected = 0;
        input_stats.minBrake = 1600;
        input_stats.maxBrake = 2400;
        return 0x26;
    }
    else{
        stats.brakeConnected = 1;
        if(input_stats.minBrake > brake) input_stats.minBrake = brake;
        if(input_stats.maxBrake < brake) input_stats.maxBrake = brake;
        return map(brake, input_stats.minBrake, input_stats.maxBrake, 0x26, 0xC2);
    }
}
unsigned char M365::getThrottle(){
    uint16_t speed = analogRead(input.throttle);
    if(!input.throttle || speed < 15){
        stats.throttleConnected = 0;
        input_stats.minSpeed = 1600;
        input_stats.maxSpeed = 2400;
        return 0x26;
    }
    else{
        stats.throttleConnected = 1;
        if(input_stats.minSpeed > speed) input_stats.minSpeed = speed;
        if(input_stats.maxSpeed < speed) input_stats.maxSpeed = speed;
        return map(speed, input_stats.minSpeed, input_stats.maxSpeed, 0x26, 0xC2);
    }
}

