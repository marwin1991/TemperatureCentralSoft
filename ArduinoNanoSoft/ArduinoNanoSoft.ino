#include <SPI.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "RF24.h"
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
/****************Messege preparation***********************************/
byte batteryStatus = 9; // 9 - full, 0 - empty

unsigned short nanoID = 0; 

unsigned long prepareMessage(short preparedTemp){
    unsigned long msg = 1; // bez bledu

    if(temperatureOverflowFlag || temperatureUnderflowFlag)
        msg = 2; // 2 jako blad chyba lepsza bo wszystkie wiadomosci bd mialy taka sama dlugosc

    msg *= 10000;
    msg += nanoID;
    msg *= 10;
    msg += batteryStatus;
    msg *= 10000;
    msg += preparedTemp;
    return msg;
}
/****************** Radio Configurator ***************************/
RF24 radio(7,8);

#define R_PI  10,10,10,10,10,10
#define ME 20,20,20,20,20,20
byte addresses[][6] = {R_PI,ME};

void setup() {
  Serial.begin(115200);
  Serial.println(F("Arduino Nano V 3.0 - Temperature sensor via NRF24L01"));
  /*for(int i =0; i < 6; i++){
    Serial.println(addresses[0][i]);
  }*/

  temperatureSensors.begin();
  if (!temperatureSensors.getAddress(thermometerAdress, 0)) /* Get address of DS18B20 */
    Serial.println("Unable to find address of thermometer"); 
  temperatureSensors.setResolution(thermometerAdress, TEMP_RESOLUTION);
  Serial.println(getTemp());

  pinMode(GREEN_LED_PIN, OUTPUT);
  
  radio.begin();

  // Set the PA Level low to prevent power supply related issues since this is a
 // getting_started sketch, and the likelihood of close proximity of the devices. RF24_PA_MAX is default.
  radio.setPALevel(RF24_PA_MAX);
  
  // Open a writing and reading pipe on each radio, with opposite addresses
  radio.openWritingPipe(addresses[0]);
  radio.openReadingPipe(1,addresses[1]);
  
  // Start the radio listening for data
  radio.startListening();
}
// Used to control whether this node is sending or receiving
bool role = 1;
void loop() {
  
  
/****************** Ping Out Role ***************************/  
if (role == 1)  {
    
    radio.stopListening();                                    // First, stop listening so we can talk.
    
    
    Serial.println(F("Now sending"));

    unsigned long msg = prepareMessage(prepareTemp(getTemp()));                             // Take the time, and send it.  This will block until complete
     if (!radio.write( &msg, sizeof(unsigned long) )){
       Serial.println(F("failed"));
     }
    Serial.print(F("Send messege: "));
    Serial.println(msg);
        
    radio.startListening();                                    // Now, continue listening
    
    unsigned long started_waiting_at = micros();               // Set up a timeout period, get the current microseconds
    boolean timeout = false;                                   // Set up a variable to indicate if a response was received or not
    
    while ( ! radio.available() ){                             // While nothing is received
      if (micros() - started_waiting_at > 1000000 ){            // If waited longer than 5000ms, indicate timeout and exit while loop
          timeout = true;
          break;
      }      
    }
        
    if ( timeout ){                                             // Describe the results
        Serial.println(F("Failed, response timed out."));
    }else{
        unsigned long got_time;                                 // Grab the response, compare, and send to debugging spew
        radio.read( &got_time, sizeof(unsigned long) );
        unsigned long end_time = micros();
        
        // Spew it
        Serial.print(F("Got response "));
        Serial.println(got_time);
    }
    digitalWrite(GREEN_LED_PIN, HIGH);   // turn the LED on (HIGH is the voltage level)
    delay(1000);                       // wait for a second
    digitalWrite(GREEN_LED_PIN, LOW);    // turn the LED off by making the voltage LOW  
    // Try again 5s later
    //delay(5000);
  }



/****************** Pong Back Role ***************************/

  if ( role == 0 )
  {
    unsigned long got_time;
    
    if( radio.available()){
                                                                    // Variable for the received timestamp
      while (radio.available()) {                                   // While there is data ready
        radio.read( &got_time, sizeof(unsigned long) );             // Get the payload
      }
     
      radio.stopListening();                                        // First, stop listening so we can talk   
      radio.write( &got_time, sizeof(unsigned long) );              // Send the final one back.      
      radio.startListening();                                       // Now, resume listening so we catch the next packets.     
      Serial.print(F("Sent response "));
      Serial.println(got_time);  
   }
 }




/****************** Change Roles via Serial Commands ***************************/

  if ( Serial.available() )
  {
    char c = toupper(Serial.read());
    if ( c == 'T' && role == 0 ){      
      Serial.println(F("*** CHANGING TO TRANSMIT ROLE -- PRESS 'R' TO SWITCH BACK"));
      role = 1;                  // Become the primary transmitter (ping out)
    
   }else
    if ( c == 'R' && role == 1 ){
      Serial.println(F("*** CHANGING TO RECEIVE ROLE -- PRESS 'T' TO SWITCH BACK"));      
       role = 0;                // Become the primary receiver (pong back)
       radio.startListening();
       
    }
  }


} // Loop

