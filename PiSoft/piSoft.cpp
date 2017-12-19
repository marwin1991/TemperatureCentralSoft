#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <fstream>
#include <thread> 
#include <RF24/RF24.h>

using namespace std;

/****************** Raspberry Pi ***********************/

RF24 radio(22,0);

bool radioNumber = 1;


//const uint8_t pipes[][6] = {"1Node","2Node"};

#define R_PI {10,10,10,10,10,10}
#define SENSOR {21,22,23,24,25,26}
const uint8_t pipes[10][6] = {R_PI,SENSOR};
int freeSlots = 8;

void addNewSensorAddress(unsigned long elderPart, unsigned long youngerPart){}

bool canRegister = false;
int canRegisterTime = 0;
int gpioButton = 15;
int ledPin = 14;
short * sensorsIds;
short sensorIdsLeftSize = 0;
short sensorIdsSize = 0;

void expandSensorsIdsTab(){
	short * tmp = (short*)malloc(sizeof(short) * (sensorIdsSize + 6));
	copy(sensorsIds, sensorsIds + sensorIdsSize, tmp);
	delete [] sensorsIds;
	sensorsIds = tmp;
	tmp = NULL;
	delete tmp;
	sensorIdsLeftSize = 6;
	sensorIdsSize +=6;
}

void respondHello(){
	
	if(sensorIdsLeftSize == 0)
		expandSensorsIdsTab();
	
	short tmpID;
	bool isIdBad = true;
	srand(time(NULL));
	while(isIdBad){
		isIdBad = false;
		tmpID = (short) rand() % 9998 + 1;   // ID from 1 - 9998
		for(int i=0; i<sensorIdsSize; i++)
			if(tmpID == sensorsIds[i])
				isIdBad = true;
	}
	
	sensorsIds[sensorIdsSize-sensorIdsLeftSize] = tmpID;
	sensorIdsLeftSize--;
	
	radio.stopListening();
	unsigned long msg =4200000000;
	msg += tmpID;
	
	if(radio.write( &msg, sizeof(unsigned long) )){
		printf("Sending ID: %lu\n", msg);
	} else {
		printf("Failed sending ID!\n");
	}
}

void printRaspberryTemperatue(){
		while(true){
			ifstream file;
			string line;
			file.open("/sys/bus/w1/devices/28-0317233eb7ff/w1_slave");
			
			if(file.is_open()){
				while(getline(file, line)){
					cout << line << endl;
				}
			}
			
			delay(10000);
		}
}

void decodeMessageTempOrError(unsigned long msg){
	if(msg == 4200000001){
		if(canRegister == true){			
			printf("Got hello messege!\n");
			respondHello();
		}
		return;
	}
    short t = msg%10000;
	msg /= 10000;
 
    short batteryStatus = msg%10;
    msg /= 10;

    short nanoId = msg%10000;
    msg /= 10000;

    short type = msg%10;
 
    if(type == 2){
        printf("Some error occures on device with ID: %d, please check file: ...\n", nanoId);
        return;
	}
    if(type == 3){
		printf("TYPE 3: %lu\n", msg);
	}
    else{
		if(t%2 == 1)
			t *= -1;
        time_t time1 = time(NULL);
        struct tm *tm = localtime(&time1);
        char s[64];
        strftime(s, sizeof(s), "%c", tm);
		float temp = (t * 1.0) / 100;
        printf("Time: %s ID: %d Battery: %d%% Temperature: %.2f\n", s, nanoId, (batteryStatus+1)*10, temp);
	}
}


int main(int argc, char** argv){

  cout << "Temperature Central Soft \n";
  radio.begin();
  radio.setRetries(15,15);
  radio.setDataRate(RF24_250KBPS);
  radio.printDetails();
  
  if(!bcm2835_init()) {
      printf("GPIO initialization failed!\n");
      return 1;
   }
   printf("GPIO initailization succesed!\n");

/***********************************/
  // This simple sketch opens two pipes for these two nodes to communicate
  // back and forth.

   
      radio.openWritingPipe(pipes[1]);
      radio.openReadingPipe(1,pipes[0]);
  
	
	radio.startListening();
	
	pinMode(gpioButton,BCM2835_GPIO_FSEL_INPT);
	pinMode(ledPin,BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_pud(gpioButton);
	
	thread t1 (printRaspberryTemperatue);
	
	// forever loop
	while (1)
	{	
			int btnState = bcm2835_gpio_lev((uint8_t) gpioButton);
			//printf("%d", btnState);
			if(btnState == 0) // it was cliked
			{
				printf("Cliked!!\n");
				int i = 0;
				for (i=0; i<=50; i++){
					if(bcm2835_gpio_lev((uint8_t) gpioButton) == 1)
						break;
					delay(100);
				}
				if( i >= 50 ) 
				{
					canRegister = true;
					bcm2835_gpio_write(ledPin,1);
					printf("Waiting for sensor to connect!\n");
				}		
				
			}
			if(canRegister == true){
				canRegisterTime++;
				delay(100);
				if(canRegisterTime == 300){
					canRegisterTime = 0;
					canRegister = false;
					bcm2835_gpio_write(ledPin,0);
					printf("Time for registration ended!\n");
				}
			}
			
			// if there is data ready
			if ( radio.available() )
			{
				// Dump the payloads until we've gotten everything
				unsigned long got_time;

				// Fetch the payload, and see if this was the last one.
				while(radio.available()){
					radio.read( &got_time, sizeof(unsigned long) );
				}

				decodeMessageTempOrError(got_time);
				radio.startListening();
				
				delay(925); //Delay after payload responded to, minimize RPi CPU time

			}

	} // forever loop

  return 0;
}

