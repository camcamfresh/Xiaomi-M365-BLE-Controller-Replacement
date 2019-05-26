/*
 * Project: Scooter Connect
 * Description: Program used to test circuit board and connection to M365 scooter.
 * Board Version: 5
 * Author: camcamfresh
 * Date: May 20, 2019
 */
 
 //System Configuration
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

Timer led(1000, changeled);

unsigned char x1structure[] = {0x55, 0xAA, 0x9, 0x20, 0x64, 0x0, 0x6, 0x26, 0x26, 0x0, 0x0, 0x68, 0x0, 0xB8, 0xFE};

void setup(){
    Serial.begin(9600);
    Serial1.begin(115200);
    Serial1.halfduplex(true);
    
    pinMode(brakeInput, INPUT_PULLDOWN);
    pinMode(throttleInput, INPUT_PULLDOWN);
    pinMode(RLED, OUTPUT);
    pinMode(GLED, OUTPUT);
    pinMode(BLED, OUTPUT);
    pinMode(buzzer, OUTPUT);
    pinMode(headlight, OUTPUT);
    pinMode(powerSwitch, OUTPUT);

    digitalWrite(powerSwitch, HIGH);
    delay(100);
    digitalWrite(powerSwitch, LOW);
    
    tone(50, buzzer);

    led.start();
}

void changeled(){
    static int x = 0;
    if(x == 0){
        Serial.print("\nA5 Read: "); Serial.print(analogRead(A5)); Serial.print(" ");
        Serial.print("A4 Read: "); Serial.println(analogRead(A4));
        x++;
    }
    else if(x == 1){
        Serial.println("Testing Blue LED");
        digitalWrite(BLED, HIGH);
        x++;
    }
    else if(x == 2){
        Serial.println("Testing Green LED");
        digitalWrite(BLED, LOW);
        digitalWrite(GLED, HIGH);
        x++;
    }
    else if(x == 3){
        Serial.println("Testing Red LED");
        digitalWrite(GLED, LOW);
        digitalWrite(RLED, HIGH);
        x++;
    }
    else if(x == 4){
        Serial.println("Testing Headlight");
        digitalWrite(RLED, LOW);
        digitalWrite(headlight, HIGH);
        x++;
    }
    else{
        Serial.println("Sending Message to Scooter");
        digitalWrite(headlight, LOW);
        Serial1.write(x1structure, sizeof(x1structure));
        x = 0;
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
            Serial.println("Scooter Packet Received.");
        case 10: //Erases message array.
            for(int erase = 0; erase <= message[2] + 5; erase++) message[erase] = 0;
            readerIndex = 0;
            break;
        default:
            readerIndex = 10;
            break;
    }
}
