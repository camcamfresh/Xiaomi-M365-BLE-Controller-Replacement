#include "M365.h"
SYSTEM_THREAD(ENABLED);

M365 scooter;
command_t command;
stats_t stats;
String stats_str, command_str;

void setup(){
    scooter.setup(Serial1, "A5", "A4", "D2", "D5", "D4", "D3", "D6", "D7");
    Particle.function("cloudCommand", cloudCommand);
    Particle.variable("stats", stats_str);
    Particle.variable("commands", command_str);
}


void loop(){
    static unsigned long loopDelay = 0;
    if(millis() - loopDelay > 1000){
        scooter.getStats(&stats);
        stats_str = serializeStructure(stats);
        command_str = serializeStructure(command);
        loopDelay = millis();
    }
}
void serialEvent1(){
    scooter.process();
}

int cloudCommand(String userCommand){
    int confirmation = -1;
    if(userCommand.equals("unlock")){
        command.lock = 0;
        confirmation = 0;
    }
    else if(userCommand.equals("lock")){
        command.lock = 1;
        confirmation = 1;
    }
    else if(userCommand.equals("tailoff")){
        command.tail = 0;
        confirmation = 2;
    }
    else if(userCommand.equals("tailon")){
        command.tail = 1;
        confirmation = 3;
    }
    else if(userCommand.equals("cruiseoff")){
        command.cruise = 0;
        confirmation = 4;
    }
    else if(userCommand.equals("cruiseon")){
        command.cruise = 1;
        confirmation = 5;
    }
    else if(userCommand.equals("ecolow")){
        command.ecoMode = 0;
        confirmation = 6;
    }
    else if(userCommand.equals("ecomed")){
        command.ecoMode = 1;
        confirmation = 7;
    }
    else if(userCommand.equals("ecohigh")){
        command.ecoMode = 2;
        confirmation = 8;
    }
    else if(userCommand.equals("ecooff")){
        command.eco = 0;
        confirmation = 9;
    }
    else if(userCommand.equals("ecoon")){
        command.eco = 1;
        confirmation = 10;
    }
    else if(userCommand.equals("nightoff")){
        command.night = 0;
        confirmation = 11;
    }
    else if(userCommand.equals("nighton")){
        command.night = 1;
        confirmation = 12;
    }
    else if(userCommand.equals("poweroff")){
        command.lock = 0;
        command.power = 0;
        confirmation = 13;
    }
    else if(userCommand.equals("poweron")){
        command.power = 1;
        confirmation = 14;
    }
    else if(userCommand.equals("reset")){
        System.reset();
        confirmation = 15;
    }
    else if(userCommand.equals("headoff")){
        command.head = 0;
        confirmation = 16;
    }
    else if(userCommand.equals("headon")){
        command.head = 1;
        confirmation = 17;
    }
    else if(userCommand.equals("ledoff")){
        command.led = 0;
        confirmation = 18;
    }
    else if(userCommand.equals("ledon")){
        command.led = 1;
        confirmation = 19;
    }
    else if(userCommand.equals("alarmoff")){
        command.alarm = 0;
        confirmation = 20;
    }
    else if(userCommand.equals("alarmon")){
        command.alarm = 1;
        confirmation = 21;
    }
    else if(userCommand.equals("alarm")){
        tone(D2, 20, 5000);
        confirmation = 22;
    }
    scooter.setCommand(&command);
    return confirmation;
}
