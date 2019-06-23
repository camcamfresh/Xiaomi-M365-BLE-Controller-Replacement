#ifndef SCOOTER_CONNECT_M365_H
#define SCOOTER_CONNECT_M365_H

#include "Particle.h"

struct stats_t{
    uint8_t alarmStatus,
        averageVelocity,
        battery,
        beep,
        brakeConnected,
        cruise,
        eco,
        ecoMode,
        hasConnected,
        isConnected,
        led,
        lock,
        night,
        odometer,
        tail,
        temperature,
        throttleConnected,
        velocity;
};
struct command_t{
    uint8_t alarm = 1,
        cruise = 1,
        eco = 0,
        ecoMode = 0,
        head = 0,
        led = 1,
        lock = 1,
        night = 0,
        power = 1,
        sound = 1,
        tail = 1;
};

class M365{
    private:
        USARTSerial * serial;
        char serialBuffer[256], serialIndex = 0;
        Timer * messageTimer;

        stats_t stats;
        command_t command;
        struct input_t{
            uint8_t brake = 0,
                throttle = 0,
                buzzer = 0,
                headlight = 0,
                power = 0,
                rled = 0,
                bled = 0,
                gled = 0;
        } input;
        struct input_stats_t{
            uint16_t minBrake = 1600,
                maxBrake = 2400,
                minSpeed = 1600,
                maxSpeed = 2400;
        } input_stats;
        
        unsigned long lastMessageSentTimeStamp = 0;
        unsigned long lastValidMessageTimeStamp = 0;
        
        int setAnalogPin(const int);
        int setDigitalPin(const int);
        bool isConnected();
        unsigned char getBrake();
        unsigned char getThrottle();
        
        void read(char);
        bool check(const char * message, int size);
        void translate(const char * message, int size);
        void compare();
        
        void send();
        void addSum(unsigned char *, int);
        void transmit(const unsigned char *, int, bool = false);

    public:
        M365();
        ~M365();
        
        void setup(USARTSerial &, String, String, String, 
                String, String, String, String, String);
        void setSerial(USARTSerial &);
        void setBrake(String);
        void setThrottle(String);
        void setBuzzer(String);
        void setRGB(String, String, String);
        void setHeadlight(String);
        void setPower(String);
        
        void process();
        void setCommand(const command_t *);
        void getStats(stats_t *);
};

template<class T>
String serializeStructure(T object){
    String result = "";
    int size = sizeof(object);
    uint8_t array[size];
    
    memcpy(array, &object, size);
    for(int i = 0; i < size; i++){
        result += (int) array[i];
        if(i != size - 1) result += ",";
    }
    return result;
}

#endif