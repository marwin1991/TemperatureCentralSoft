#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <fstream>
#include <thread> 
#include <csignal>
#include <RF24/RF24.h>

using namespace std;

RF24 radio(22,0);

bool radioNumber = 1;

#define R_PI {10,10,10,10,10,10}
#define SENSOR {21,22,23,24,25,26}
const uint8_t pipes[10][6] = {R_PI,SENSOR};
int freeSlots = 8;

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
		tmpID = (short) (rand() % 9000) + 1000;   // ID from 1000 - 9999
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
		cout << "Sending message: " << msg << " to sensor..." << endl;
	} else {
		cout << "Failed sending message!" << endl;
	}
}

void printRaspberryTemperatue(){
		while(true){
			ifstream file;
			string line;
			file.open("/sys/bus/w1/devices/28-0317233eb7ff/w1_slave");
			
			if(file.is_open()){
				getline(file, line);
				getline(file, line);
				
				int k = (int) line.find("t=") + 2;
				
				string temp = line.substr(k);
				float f_temp = atof(temp.c_str());
				
				time_t time1 = time(NULL);
				struct tm *tm = localtime(&time1);
				char s[64];
				strftime(s, sizeof(s), "%c", tm);
				
				cout << fixed;
				cout.precision(2);
				cout << "Time: " << s << " ID: RaspberryPiStation Temperature: " << f_temp/1000 << endl;
			}
			delay(32000);
		}
}

void signal_handler(int signal){
  cout << endl << "Exiting piSoft for central temperature sensor base..." << endl;
  exit(0);
}

void decodeMessageTempOrError(unsigned long msg){
	if(msg == 4200000001){
		if(canRegister == true){			
			cout << "Received request for ID from new sensor...!" << endl;
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
        cout << "Some error occures on device with ID: " << nanoId << endl;
        return;
	}
    if(type == 3){
		//In future message types can be extende
		cout << "TYPE 3: " << msg << endl;
	}
    else{
		if(t%2 == 1)
			t *= -1;
        time_t time1 = time(NULL);
        struct tm *tm = localtime(&time1);
        char s[64];
        strftime(s, sizeof(s), "%c", tm);
		float temp = (t * 1.0) / 100;
		cout << fixed;
		cout.precision(2);
        cout << "Time: " << s  <<" ID: " << nanoId << " Battery: " << (batteryStatus+1)*10 << "% Temperature: " << temp << endl;
	}
}

int main(int argc, char** argv){
    cout << "Temperature Central Soft " << endl;
    radio.begin();
    radio.setRetries(15,15);
    radio.setDataRate(RF24_250KBPS);
    radio.printDetails();
  
    if(!bcm2835_init()) {
       cout << "GPIO initialization failed!" << endl;
       return 1;
    }
    cout << "GPIO initailization succesed!" << endl;

    radio.openWritingPipe(pipes[1]);
    radio.openReadingPipe(1,pipes[0]);
  
	radio.startListening();
	
	pinMode(gpioButton,BCM2835_GPIO_FSEL_INPT);
	pinMode(ledPin,BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_pud(gpioButton);
	
	thread t1 (printRaspberryTemperatue);
	signal(SIGINT, signal_handler);

	while (1){	
			int btnState = bcm2835_gpio_lev((uint8_t) gpioButton);
			if(btnState == 0){ // button was cliked
				int i = 0;
				cout << "Keep button pressed for 5 seconds..." << endl;
				for (i=0; i<=50; i++){
					if(bcm2835_gpio_lev((uint8_t) gpioButton) == 1)
						break;
					delay(100);
				}
				if( i >= 50 ){
					canRegister = true;
					bcm2835_gpio_write(ledPin,1);
					cout << "Waiting for sensor to connect!" << endl;
				}		
				
			}
			if(canRegister == true){
				canRegisterTime++;
				delay(100);
				if(canRegisterTime == 300){
					canRegisterTime = 0;
					canRegister = false;
					bcm2835_gpio_write(ledPin,0);
					cout << "Time for registration ended!" << endl;
				}
			}
			
			if ( radio.available() ){
				unsigned long got_time;

				while(radio.available()){
					radio.read( &got_time, sizeof(unsigned long) );
				}

				decodeMessageTempOrError(got_time);
				radio.startListening();
				
				delay(925); //Delay to minimize RPi CPU time
			}
	}

  return 0;
}
