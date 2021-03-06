#include <SPI.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "RF24.h"
#include <printf.h>
#include "LowPower.h"
/******************LED - green ******************************************/
#define GREEN_LED_PIN 4

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


#define Z1 4100.0
#define Z2 4700.0

void checkBattery() {
  // put your main code here, to run repeatedly:
  float Vout = analogRead(A0) * (5.0/1023.0);
  float Vin = (Z1 + Z2)/(Z2) * Vout;
  batteryStatus = (byte) Vin;
}
/****************ID of this sensor***********************************/
unsigned short nanoID = 0; //0-9999
/****************Messege preparation***********************************/
unsigned long prepareMessage(short preparedTemp){
    unsigned long msg = 1; // bez bledu

    if(temperatureOverflowFlag || temperatureUnderflowFlag)
        msg = 2; // 2 jako blad chyba lepsza bo wszystkie wiadomosci bd mialy taka sama dlugosc

    msg *= 10000;
    msg += nanoID;
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
        delay(3000);
        //LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);                   // sending hello every 4s untill success
    }
    radio.startListening(); 
    Serial.print(F("Succesfully send a hello message."));
    Serial.print(F("Starts waiting for my ID."));  
    unsigned long started_waiting_at = micros();               // Set up a timeout period, get the current microseconds
    boolean timeout = false;  
    while ( ! radio.available() and ! timeout){                             // While nothing is received
        if (micros() - started_waiting_at > 500000 ){            // If waited longer than 5000ms, indicate timeout and exit while loop
            timeout = true;
        }
    }      
    if ( timeout ){                                             // Describe the results
          Serial.println(F("Failed waiting for ID!!!."));
          delay(100);
    }else{
          unsigned long msg;                                 // Grab the response, compare, and send to debugging spew
          radio.read( &msg, sizeof(unsigned long) );
          Serial.println(F("AAA"));
          if(msg/100000000 == 42){
            Serial.print(F("Got my new ID: "));
            //Serial.println(msg);
            nanoID = msg%10000;
            Serial.println(nanoID);
            iGotMyId = true;
            delay(100);
          }
    }
    //LowPower.powerDown(SLEEP_4S, ADC_OFF, BOD_OFF);
    delay(3000);
  }
  Serial.println(F("Successfully registration on central station!"));
}

void setup() {
  Serial.begin(115200);
  Serial.println(F("Arduino Nano V 3.0 - Temperature sensor via NRF24L01"));
  
  temperatureSensors.begin();
  if (!temperatureSensors.getAddress(thermometerAdress, 0)) /* Get address of DS18B20 */
    Serial.println("Unable to find address of thermometer"); 
  temperatureSensors.setResolution(thermometerAdress, TEMP_RESOLUTION);
  Serial.println(getTemp());

  pinMode(GREEN_LED_PIN, OUTPUT);
  printf_begin();
  radio.begin();
  radio.setDataRate(RF24_250KBPS);
  radio.setPALevel(RF24_PA_MAX);
  radio.openWritingPipe(addresses[0]);
  radio.openReadingPipe(1,addresses[1]);
  radio.printDetails();
  sendHello();
}
void loop() {
    radio.stopListening();                                    // First, stop listening so we can talk.
    Serial.println(F("Now sending"));

    unsigned long msg = prepareMessage(prepareTemp(getTemp()));                             // Take the time, and send it.  This will block until complete
     if (!radio.write( &msg, sizeof(unsigned long) )){
       Serial.println(F("failed"));
     }
    Serial.print(F("Send messege: "));
    Serial.println(msg);
        
    digitalWrite(GREEN_LED_PIN, HIGH);   // turn the LED on (HIGH is the voltage level)
    delay(50);
    LowPower.powerDown(SLEEP_1S, ADC_OFF, BOD_OFF);                    // wait for a second
    digitalWrite(GREEN_LED_PIN, LOW);    // turn the LED off by making the voltage LOW  
    //Try again 5s later
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
}

