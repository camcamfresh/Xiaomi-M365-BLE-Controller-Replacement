/* Project: M365 Connect
 * Description: Scooter Firmware
 * Board Version: 5
 * Author: Cameron
 * Date: September 27, 2019
 */
 
#include <NMEAGPS.h>
#include "Serial5/Serial5.h"
SYSTEM_THREAD(ENABLED);

//Pin Configuration.
int BRAKE = A5;
int THROTTLE = A4;
int RLED = D5;
int GLED = D4;
int BLED = D3;
int BUZZER = D2;
int HEADLIGHT = D6;
int POWER = D7;

//Internal Variables
struct STATISTICS
{
    uint8_t alarm,
        averageVelocity,
        battery,
        beep,
        cruise,
        eco,
        ecoMode,
        led,
        lock,
        night,
        odometer,
        tail,
        temperature,
        velocity;
} stats;
struct COMMANDS
{
    uint8_t alarm = 1,
        cruise = 1,
        eco = 0,
        ecoMode = 0,
        head = 0,
        led = 1,
        lock = 0,
        night = 0,
        power = 1,
        sound = 1,
        tail = 1;
} command;

unsigned char unlock[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x71, 0x1, 0x0, 0x66, 0xFF}
   , lock[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x70, 0x1, 0x0, 0x67, 0xFF}
   , tailoff[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x7D, 0x0, 0x0, 0x5B, 0xFF}
   , tailon[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x7D, 0x2, 0x0, 0x59, 0xFF}
   , cruiseoff[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x7C, 0x0, 0x0, 0x5C, 0xFF}
   , cruiseon[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x7C, 0x1, 0x0, 0x5B, 0xFF}
   , ecolow[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x7B, 0x00, 0x0, 0x5D, 0xFF}
   , ecomed[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x7B, 0x01, 0x0, 0x5C, 0xFF}
   , ecohigh[] = {0x55, 0xAA, 0x4, 0x20, 0x3, 0x7B, 0x02, 0x0, 0x5B, 0xFF};

int gpsValid = 0
  , isPowered = 0
  , isConnected = 0
  , hasConnected = 0
  , brakeConnected = 0
  , throttleConnected = 0;

ApplicationWatchdog watchDog(60000, System.reset);
Timer connectionTimer(5, connectionMonitor);
Timer messageTimer(20, messageConstructor);
Timer powerTimer(60000, powerCheck);

retained int minBrake = 1200, minSpeed = 1200, maxBrake = 1600, maxSpeed = 1600;
unsigned long lastReceivedMessageTimeStamp = 0;
retained String gpsLocation, gpsLink;
int resetCommand = 0
    , safeMode = 0;
    
void setup()
{
    Serial1.begin(115200);
    Serial1.halfduplex(true);
    Serial5.begin(9600);

    Particle.function("CloudCommand", cloudCommand);
    
    Particle.variable("Version-v9e", "");
    Particle.variable("isConnected", isConnected);
    Particle.variable("Battery", stats.battery);
    Particle.variable("gpsValid", gpsValid);
    Particle.variable("gpsLocation", gpsLocation);
    Particle.variable("gpsLink", gpsLink);
    
    pinMode(RLED, OUTPUT);
    pinMode(GLED, OUTPUT);
    pinMode(BLED, OUTPUT);
    pinMode(BUZZER, OUTPUT);
    pinMode(HEADLIGHT, OUTPUT);
    pinMode(POWER, OUTPUT);

    Particle.connect();
    connectionTimer.start();
    messageTimer.start();
}

void loop()
{
    while(Serial1.available())
        readMessage(Serial1.read());
    
    updateGPS();
    
    backgroundProcess();
    
    watchDog.checkin();
}

void connectionMonitor()
{
    static PMIC pmic;
    static int brakeSum = 0, throttleSum = 0, index = 0;
    static long lastBrakeDisconnectionTimeStamp = 0
        , lastThrottleDisconnectionTimeStamp = 0;
    
    if(millis() < lastReceivedMessageTimeStamp + 3000)
        isConnected = 1;
    else
        isConnected = 0;

    if(!(pmic.getSystemStatus() & 0x04) == 0) 
        isPowered = 1;
    else
        isPowered = 0;
    
    brakeSum += analogRead(BRAKE);
    throttleSum += analogRead(THROTTLE);
    
    if(index < 256)
        index++;
    else
    {
        int brakeAvg = brakeSum >> 8;
        int throttleAvg = throttleSum >> 8;
        
        if(brakeAvg < 15)
        {
            brakeConnected = 0;
            minBrake = 1200;
            maxBrake = 1600;
            lastBrakeDisconnectionTimeStamp = millis();
        }
        else if(millis() > lastBrakeDisconnectionTimeStamp + 3000)
        {
            brakeConnected = 1;
            if(brakeAvg < minBrake)
                minBrake = brakeAvg;
            else if(brakeAvg > maxBrake)
                maxBrake = brakeAvg;
        }
        
        if(throttleAvg < 15)
        {
            throttleConnected = 0;
            minSpeed = 1200;
            maxSpeed = 1600;
            lastThrottleDisconnectionTimeStamp = millis();
        }
        else if(millis() > lastThrottleDisconnectionTimeStamp + 3000)
        {
            throttleConnected = 1;
            if(throttleAvg < minSpeed)
                minSpeed = throttleAvg;
            else if(throttleAvg > maxSpeed)
                maxSpeed = throttleAvg;
        }
        
        brakeSum = 0;
        throttleSum = 0;
        index = 0;
    }
}

void messageConstructor()
{
    static int messageIndex = 0;
	char brake = map(analogRead(BRAKE), minBrake, maxBrake, 0x26, 0xC2);
	char speed = map(analogRead(THROTTLE), minSpeed, maxSpeed, 0x26, 0xC2);

	if(!brakeConnected || brake < 0x26)
		brake = 0x26;
	if(!throttleConnected || speed < 0x26)
		speed = 0x26;
	if(!isConnected)
		messageIndex = 4;

	switch(messageIndex++){
        case 0:
        case 1:
        case 2:
        case 3:
		{
            unsigned char message[] = {0x55, 0xAA, 0x7, 0x20, 0x65, 0x0, 0x4, speed, brake, 0x0, stats.beep, 0x0, 0x0};
            checkAndSend(message, sizeof(message));
            if(stats.beep == 1)
				stats.beep = 0;
            break;
        }
        case 4:
		{
            unsigned char message[] = {0x55, 0xAA, 0x9, 0x20, 0x64, 0x0, 0x6, speed, brake, 0x0, stats.beep, 0x72, 0x0, 0x0, 0x0};
            checkAndSend(message, sizeof(message));
            if(stats.beep == 1)
				stats.beep = 0;
            break;
        }
        case 5:
		{
            unsigned char message[] = {0x55, 0xAA, 0x6, 0x20, 0x61, 0xB0, 0x20, 0x02, speed, brake, 0x0, 0x0};
            checkAndSend(message, sizeof(message));
            break;
        }
        case 6:
		{
            unsigned char message[] = {0x55, 0xAA, 0x6, 0x20, 0x61, 0x7B, 0x4, 0x2, speed, brake, 0x0, 0x0};
            checkAndSend(message, sizeof(message));
            break;
        }
        case 7:
		{
            unsigned char message[] = {0x55, 0xAA, 0x6, 0x20, 0x61, 0x7D, 0x2, 0x2, speed, brake, 0x0, 0x0};
            checkAndSend(message, sizeof(message));
            messageIndex = 0;
			break;
        }
    }
}

void checkAndSend(unsigned char * message, int size)
{
	unsigned long cksm = 0;
	for(int i = 2; i < size - 2; i++)
		cksm += message[i];
    cksm ^= 0xFFFF;

	message[size - 1] = (unsigned char) ((cksm & 0xFF00) >> 8);
	message[size - 2] = (unsigned char) (cksm & 0xFF);

	Serial1.write(message, size);
	Serial1.flush();
}

void readMessage(unsigned char data)
{
	static unsigned char message[64];
	static char readIndex = 0, dataIndex;
	static long cksm;
	
	switch(readIndex)
	{	
		case 0:
			if(data == 0x55)
				readIndex++;
			break;
		case 1:
			if(data == 0xAA)
				readIndex++;
			else
				readIndex = 0;
			break;
		case 2:
			message[0] = data;
			cksm = data;
			dataIndex = 1;
			readIndex++;
			break;
		case 3:
			message[dataIndex] = data;
			
			if(dataIndex < message[0] + 2)
			    cksm += data;
			
			if(dataIndex < message[0] + 3)
			{
			    dataIndex++;
			    break;
			}

		    cksm ^= 0xFFFF;
	    
		    if(message[dataIndex - 1] == (cksm & 0xFF)
		    && message[dataIndex] == (cksm & 0xFF00) >> 8)
		    {
		        processMessage(message, dataIndex);
		        hasConnected = 1;
		        lastReceivedMessageTimeStamp = millis();
		    }

		default:
			for(int erase = 0; erase <= message[0] + 3; erase++)
				message[erase] = 0;
            readIndex = 0;
            break;
	}
}

void processMessage(unsigned char * message, int size)
{
    switch(message[1])
	{
        case 0x21:
            if(message[2] == 0x64)
			{
                stats.eco = message[4];
                stats.led = message[5];
                stats.night = message[6];
                stats.beep = message[7];
				backgroundProcess();
            }
            break;
        case 0x23:
            switch(message[3])
			{
                case 0x7B:
                    stats.ecoMode = message[4];
                    stats.cruise = message[6];
                    break;
                case 0x7D:
                    stats.tail = message[4];
                    break;
                case 0xB0:
                    stats.alarm = message[6];
                    stats.lock = message[8];
                    stats.battery = message[12];
                    stats.velocity = ( message[14] + (message[15] * 256)) / 1000 / 1.60934;
                    stats.averageVelocity = (message[16] + (message[17] * 265)) / 1000 / 1.60934;
                    stats.odometer = (message[18] + (message[19] * 256) + (message[20] * 256 * 256)) / 1000 / 1.60934;
                    stats.temperature = ((message[26] + (message[27] * 256)) / 10 * 9 / 5) + 32;
                    if(stats.alarm)
						tone(BUZZER, 20, 400);
                    break;
            }
			processChanges();
    }
}

void backgroundProcess()
{
    static long lastBackgroundProcessTimeStamp = 0;
	
	updateLED();
	
	if(millis() > 1000 + lastBackgroundProcessTimeStamp)
	{
	    if(!isPowered && command.power)
        {
            delay(500);
            togglePower();
            messageTimer.start();
        }
        
        if(safeMode)
        {
            Particle.process();
            delay(100);
            safeMode = 0;
            System.enterSafeMode();
        }
        
        if(resetCommand)
        {
            Particle.process();
            delay(100);
            resetCommand = 0;
            System.reset();
        }
        
        if((command.head || stats.night) && !stats.lock)
    			digitalWrite(HEADLIGHT, HIGH);
        else
    		digitalWrite(HEADLIGHT, LOW);
	    
	    if(isConnected)
	    {
    		if(!command.power && !stats.lock || stats.battery < 3)
    		{
    		    messageTimer.stop();
    		    digitalWrite(POWER, HIGH);
                delay(2000);
                digitalWrite(POWER, LOW);
                isConnected = 0;
                if(stats.battery < 3)
                {
                    Cellular.off();
                    System.sleep(SLEEP_MODE_DEEP);
                }
    		}
    		else if((command.night && !stats.night) || (!command.night && stats.night))
                togglePower();
    		else if(((stats.eco == 0x00 || stats.eco == 0x01) && command.eco) 
    			 || ((stats.eco == 0x02 || stats.eco == 0x03) && !command.eco))
    		{
                togglePower();
                delay(25);
                togglePower();
            }
            lastBackgroundProcessTimeStamp = millis();
        }
	}
}

void processChanges()
{
	if(!command.lock && stats.lock)
		checkAndSend(unlock, sizeof(unlock));
    else if(command.lock && !stats.lock)
		checkAndSend(lock, sizeof(lock));
	
	if(!command.cruise && stats.cruise)
		checkAndSend(cruiseoff, sizeof(cruiseoff));
    else if(command.cruise && !stats.cruise)
		checkAndSend(cruiseon, sizeof(cruiseon));
    
    if(!command.tail && stats.tail)
		checkAndSend(tailoff, sizeof(tailoff));
    else if(command.tail && !stats.tail)
		checkAndSend(tailon, sizeof(tailon));

	if(command.ecoMode != stats.ecoMode)
	{
		if(command.ecoMode == 0)
			checkAndSend(ecolow, sizeof(ecolow));
		else if(command.ecoMode == 1)
			checkAndSend(ecomed, sizeof(ecomed));
		else if(command.ecoMode == 2)
			checkAndSend(ecohigh, sizeof(ecohigh));
	}
	
	if(stats.beep && !stats.alarm)
	{
		tone(BUZZER, 20, stats.beep == 2 ? 250 : 100);
		stats.beep = 1;
	}
}

void updateLED()
{
	digitalWrite(RLED, LOW);
    digitalWrite(GLED, LOW);
    digitalWrite(BLED, LOW);
	
	if(!command.led || !isPowered)
		return;
	
	if(!Particle.connected())
		digitalWrite(BLED, HIGH);
	else if(stats.lock)
		digitalWrite(RLED, HIGH);
    else
		digitalWrite(GLED, HIGH);		
}

void togglePower()
{
	digitalWrite(POWER, HIGH);
    delay(100);
    digitalWrite(POWER, LOW);
}

void powerCheck()
{
    static FuelGauge fuel;
    
    if(fuel.getSoC() < 50)
    {
        command.power = 1;
    }
    else
    {
        command.power = 0;
    }
}

void updateGPS()
{
    static NMEAGPS gps;
    while(gps.available(Serial5))
    {
        gps_fix fix = gps.read();
        if(fix.valid.location)
        {
            gpsValid = 1;
            gpsLocation = String(fix.latitude(), 4) + "," + String(fix.longitude(), 4);
            gpsLink = "https://maps.google.com/?q=" + gpsLocation;
        }
        else
            gpsValid = 0;
    }
}

int cloudCommand(String userInput)
{
    int confirmation = -1;
    
    if(userInput.equals("unlock"))
    {
        command.lock = 0;
        confirmation = 0;
    }
    else if(userInput.equals("lock"))
    {
        command.lock = 1;
        confirmation = 1;
    }
    else if(userInput.equals("tailoff"))
    {
        command.tail = 0;
        confirmation = 2;
    }
    else if(userInput.equals("tailon"))
    {
        command.tail = 1;
        confirmation = 3;
    }
    else if(userInput.equals("cruiseoff"))
    {
        command.cruise = 0;
        confirmation = 4;
    }
    else if(userInput.equals("cruiseon"))
    {
        command.cruise = 1;
        confirmation = 5;
    }
    else if(userInput.equals("ecolow"))
    {
        command.ecoMode = 0;
        confirmation = 6;
    }
    else if(userInput.equals("ecomed"))
    {
        command.ecoMode = 1;
        confirmation = 7;
    }
    else if(userInput.equals("ecohigh"))
    {
        command.ecoMode = 2;
        confirmation = 8;
    }
    else if(userInput.equals("ecooff"))
    {
        command.eco = 0;
        confirmation = 9;
    }
    else if(userInput.equals("ecoon"))
    {
        command.eco = 1;
        confirmation = 10;
    }
    else if(userInput.equals("nightoff"))
    {
        command.night = 0;
        confirmation = 11;
    }
    else if(userInput.equals("nighton"))
    {
        command.night = 1;
        confirmation = 12;
    }
    else if(userInput.equals("poweroff"))
    {
        powerTimer.start();
        command.lock = 0;
        command.power = 0;
        confirmation = 13;
    }
    else if(userInput.equals("poweron"))
    {
        powerTimer.stop();
        command.power = 1;
        confirmation = 14;
    }
    else if(userInput.equals("reset"))
    {
        resetCommand = 1;
        confirmation = 15;
    }
    else if(userInput.equals("headoff"))
    {
        command.head = 0;
        confirmation = 16;
    }
    else if(userInput.equals("headon"))
    {
        command.head = 1;
        confirmation = 17;
    }
    else if(userInput.equals("ledoff"))
    {
        command.led = 0;
        confirmation = 18;
    }
    else if(userInput.equals("ledon"))
    {
        command.led = 1;
        confirmation = 19;
    }
    else if(userInput.equals("alarmoff"))
    {
        command.alarm = 0;
        confirmation = 20;
    }
    else if(userInput.equals("alarmon"))
    {
        command.alarm = 1;
        confirmation = 21;
    }
    else if(userInput.equals("alarm"))
    {
        tone(BUZZER, 20, 5000);
        confirmation = 22;
    }
    else if(userInput.equals("safemode"))
    {
        Particle.disconnect();
        safeMode = 1;
        confirmation = 23;
    }
    else if(userInput.equals("gpsoff"))
    {
        confirmation = 24;
    }
    else if(userInput.equals("gpson"))
    {
        if(!gpsLocation.equals("")) Particle.publish("gpsTrace", gpsLocation);
        confirmation = 25;
    }
    
    return confirmation;
}
