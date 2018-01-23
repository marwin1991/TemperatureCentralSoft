/******************Remote Sensor Temperature ****************************/
/* Author:       Piotr Zmilczak and Arkadiusz Dudzik                    */
/* Date:         02.01.2018                                             */
/* Board:        Arduino Mini Pro 5V, 16MHz -(Tested)                   */
/* Board:        Arduino Mini Pro 3V, 8MHz -(Not tested but better)     */
/************************************************************************/
#include <SPI.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "RF24.h"
#include <printf.h>
#include "LowPower.h"
#include <EEPROM.h>
/******************LED - green ******************************************/
#define GREEN_LED_PIN 4
/******************Button - reset****************************************/
#define RESET_PIN 3
/******************Thermometer Config and functions**********************/
#define DS18B20_PIN 2
#define TEMP_RESOLUTION 9 
OneWire oneWireDS18B20(DS18B20_PIN);
DallasTemperature temperatureSensors(&oneWireDS18B20);
DeviceAddress thermometerAdress; //  arrays to hold device address
bool temperatureOverflowFlag = false; // T>100 C
bool temperatureUnderflowFlag = false; // -100 C > T C

float getTemp(){
  temperatureSensors.requestTemperatures();
  return temperatureSensors.getTempC(thermometerAdress);
}
/* 4 - digits for temp -100 < T < 100 C
   if the last digit odd -100<T<0 C
   if even 0<=T<100 C  */
short prepareTemp(float temp){
    if (temp > 100){
        temperatureOverflowFlag = true;  
        return 0001;
    }

    if (temp < -100){
        temperatureUnderflowFlag = true;
        return 0002;
    }

    bool isMinus = false;
    if(temp < 0){
        isMinus = true;
        temp *= -1;
    }

    short t;
    int i;
    for(i = 0; i < 2; i++){
        temp *= 10;
        t = (short) temp;
    }

    if(isMinus){
        if(t%2 == 0)
            t++;
    } else {
        if(t%2 == 1)
            t++;
    }
    return t;
}
/****************Battery Status***********************************/
byte batteryStatus = 9; // 9 - full, 0 - empty

void checkBattery() {
  
  float Vout = analogRead(A1) * (5.0/1023.0);
  float Vin = 2*Vout;
  if(Vin >= 9 )
    Vin = 9;
  batteryStatus = (byte) Vin;
  
}
/****************ID of this sensor***********************************/
unsigned short miniProID = 0; //0-9999
int id_mem_addr0 = 0;
int id_mem_addr1 = 1;
byte ret0;
byte ret1;
/****************Messege preparation***********************************/
unsigned long prepareMessage(short preparedTemp){
    unsigned long msg = 1; // bez bledu

    if(temperatureOverflowFlag || temperatureUnderflowFlag)
        msg = 2; // 2 jako blad chyba lepsza bo wszystkie wiadomosci bd mialy taka sama dlugosc

    msg *= 10000;
    msg += miniProID;
    msg *= 10;
    checkBattery();
    msg += batteryStatus;
    msg *= 10000;
    msg += preparedTemp;
    return msg;
}
/****************** Radio Configurator ***************************/
RF24 radio(7,8);

#define R_PI  {10,10,10,10,10,10}
#define ME {21,22,23,24,25,26}
byte addresses[][6] = {R_PI,ME};


/********************Registration*********************************/
void sendHello(){
  digitalWrite(GREEN_LED_PIN, HIGH); // when green led is ON nonstop it means that we are in registrating process
  unsigned long t = 4200000001;
  bool iGotMyId = false;
  while(!iGotMyId){
    radio.stopListening();
    while(!radio.write( &t, sizeof(unsigned long) )){
        Serial.println(F("Failed sending hello message! Waits 4s for next try!"));
        delay(4000);
    }
    radio.startListening(); 
    Serial.println(F("Succesfully send a hello message."));
    Serial.println(F("Starts waiting for my ID."));  
    unsigned long started_waiting_at = micros();               // Set up a timeout period, get the current microseconds
    boolean timeout = false;  
    while ( !radio.available() and ! timeout){                             // While nothing is received
        if (micros() - started_waiting_at > 200000 ){            // If waited longer than 5000ms, indicate timeout and exit while loop
            timeout = true;
        }
    }      
    if ( timeout ){                                             // Describe the results
          Serial.println(F("Failed waiting for ID!!!."));
          delay(100);
    }else{
          unsigned long msg;                                 // Grab the response, compare, and send to debugging spew
          radio.read( &msg, sizeof(unsigned long) );
          if(msg/100000000 == 42){
            Serial.print(F("Got my new ID: "));
            miniProID = msg % 10000;
            unsigned short tempID = miniProID;
            ret0 = (byte)(tempID & 0xff);
            ret1 = (byte)((tempID >> 8) & 0xff);
            EEPROM.write(id_mem_addr0,ret0);
            EEPROM.write(id_mem_addr1,ret1);
            Serial.println(miniProID);
            iGotMyId = true;
            delay(100);
          }
          delay(3000);
    }
    delay(4000);
  }
  Serial.println(F("Successfully registration on central station!"));
  radio.stopListening();  
  digitalWrite(GREEN_LED_PIN, LOW);  
}
/****************** Reseting ID******************************************/
void resetId(){
  Serial.print(F("Actual id: "));
  Serial.println(miniProID);
  if (miniProID == 0){
     return;
  }else{
     Serial.println(F("Reseting ID"));
     EEPROM.write(id_mem_addr0,0);
     EEPROM.write(id_mem_addr1,0);
     miniProID = 0;
  }
}

void resetInteruptHandler(){
   static unsigned long last_interrupt_time = 0;
   unsigned long interrupt_time = millis();
   if (interrupt_time - last_interrupt_time > 20) 
   {
       resetId();
   }
   last_interrupt_time = interrupt_time;
}
/****************Board setup*********************************************/
void setup() {
  Serial.begin(115200);
  Serial.println(F("Arduino Mini Pro - Temperature sensor via NRF24L01"));

  temperatureSensors.begin();
  if (!temperatureSensors.getAddress(thermometerAdress, 0)) 
    Serial.println("Unable to find address of thermometer"); 
  temperatureSensors.setResolution(thermometerAdress, TEMP_RESOLUTION);
  Serial.println(getTemp());

  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RESET_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(RESET_PIN), resetInteruptHandler, RISING);
  
  printf_begin();
  radio.begin();
  radio.setDataRate(RF24_250KBPS);
  radio.setPALevel(RF24_PA_MAX);
  radio.openWritingPipe(addresses[0]);
  radio.openReadingPipe(1,addresses[1]);
  radio.printDetails();
  miniProID = (unsigned short) ((EEPROM.read(id_mem_addr1)<<8) | (EEPROM.read(id_mem_addr0)));
  Serial.print(F("Restored id: "));
  Serial.println(miniProID);
  if( miniProID == 0 ){
     sendHello();
  }
}

/***********************Main loop*******************************************/
void loop() {
    if( miniProID == 0 ){
       sendHello();
    }
    digitalWrite(GREEN_LED_PIN, HIGH);
    unsigned long msg = prepareMessage(prepareTemp(getTemp()));                             // Take the time, and send it.  This will block until complete
    if (!radio.write( &msg, sizeof(unsigned long))){
      Serial.print(F("Failed sending messege: "));
      Serial.println(msg);
    } else {
      Serial.print(F("Send messege: "));
      Serial.println(msg);
    }
    digitalWrite(GREEN_LED_PIN, LOW);
    delay(50);
    if( miniProID != 0 )LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
    if( miniProID != 0 )LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
    if( miniProID != 0 )LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
    if( miniProID != 0 )LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
}

