#include "arduino_secrets.h"
// Adafruit seesaw Library - Version: 1.3.1
#define SERIAL_SPEED      230400
#include <NTPClient.h>
#include "Wifi.h"
#include "FirebaseStats.h"
#include "Adafruit_seesaw.h"
#include <RTCZero.h>

int dry_count = 0;

Adafruit_seesaw soilSensorBuss;
uint8_t sensorA = 0x36;
boolean is_connected_to_wifi=false;

char datetime[64];

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
RTCZero rtc;

void setup() {
  
  Serial.begin(SERIAL_SPEED);
  delay(10000); //wit to connect
  statusMessage("Initializing...");
  statusMessage("Searching moisture sensor...");

  //LOW POWER MODE
  //Read https://www.element14.com/community/community/project14/iot-in-the-cloud/blog/2019/05/27/the-windchillator-reducing-the-sleep-current-of-the-arduino-mkr-wifi-1010-to-800-ua
  // Set digital pins to input to reduce powe consumption
  for (int i=1; i < 15; i++) {
    pinMode(i, INPUT);
  }
  WiFi.lowPowerMode();
  
  if (!soilSensorBuss.begin(sensorA)) {
    while(1){
      statusMessage("Error moisture sensor not found!");
      delay(1000);
    }
  }else{
     char message[64];
     snprintf(message, sizeof(message), "Soil Sensor found. version: %x", soilSensorBuss.getVersion());
     statusMessage(message);
  }
 statusMessage("Moisture sensor found!");
 
 pinMode(0, OUTPUT);
 
 statusMessage("Read config...");
 init_config();
 
 statusMessage("Initiating Wifi...");
  if(connect_to_wifi() !=0){
    statusMessage("Unable to connect to Wifi");
    is_connected_to_wifi=false;
    statusMessage("Starting Config mode...");
    wifi_ap();
    return;
  }else{
    is_connected_to_wifi=true;

    timeClient.begin();
    timeClient.setTimeOffset(7200);
    timeClient.forceUpdate();

    //set timer
    rtc.begin();
    rtc.setEpoch(timeClient.getEpochTime());
    rtc.attachInterrupt(wakeup);

    CycleStats update = { rtc.getEpoch(),getDateTime(),getLiPoVoltage() , 0, 0, "Booting"};
    pushUpdate(update);
  }

  statusMessage("Initializing duty cycle...");
  statusMessage("ready!");
}

void loop() {

  digitalWrite(LED_BUILTIN, HIGH);

  if(!is_connected_to_wifi){
      wifi_server();
      delay(1000);
      return;
  }

  connect_to_wifi();
  
  if(settings.standby){
    statusMessage("Standby!");
    CycleStats update = { rtc.getEpoch(),getDateTime(),getLiPoVoltage() , 0, 0, "StandBy"};
    pushUpdate(update);
  }else{
    dropCycle();
  }
  //settings are updated at the end of the loop to ensure the first cycle
  //is always a stand by one.
  readSettings();
  digitalWrite(LED_BUILTIN, LOW); 
  sleep(settings.cyle_rest);
}

void sleep(int minutes) {
    

  unsigned int epoc = rtc.getEpoch();
  epoc += minutes*60;
  rtc.setAlarmEpoch(epoc);

  int h = rtc.getAlarmHours();

  if(h > settings.work_end || h < settings.work_start){ //Sleep until work_start time
    rtc.setAlarmTime(settings.work_start, 0, 0);
    
    //Re-sync RTC time with NTP client time
    //NOTE: this sync seems to introduce some bugs and wrong hours
    //timeClient.update();
    //rtc.setEpoch(timeClient.getEpochTime());
    
    CycleStats update = {rtc.getEpoch(),getDateTime(),getLiPoVoltage(), 0, 0, "Outside working hours - long sleep"};
    pushUpdate(update);
  }
  
  rtc.enableAlarm(rtc.MATCH_HHMMSS);
 Serial.print ( "[FILSDROP] Wakeup at:" ); Serial.print ( rtc.getAlarmHours() ); Serial.print ( ":" ); Serial.print ( rtc.getAlarmMinutes()); Serial.print ( ":" ); Serial.println ( rtc.getAlarmSeconds());
  
  //Power saving
  WiFi.disconnect();
  WiFi.end();
  rtc.standbyMode();
}


void wakeup() {
  //Wake-up
  rtc.disableAlarm();
}



void dropCycle(){
  
  statusMessage("Start Drop Cycle...");
  //Check LiPo Voltage
  float vBat = getLiPoVoltage();
  String action = "none";
  char s[64];
  snprintf(s, sizeof(s), "LiPo vBat: %f", vBat);
  statusMessage(s);

  int moisture = getMoisture(sensorA);
  Serial.print ( "[FILSDROP] Moisture level:" ); Serial.print ( moisture ); Serial.println ( " units" );

  CycleStats update = { rtc.getEpoch(),getDateTime(),getLiPoVoltage() , moisture, dry_count, action};

  if(vBat < 3.5){
    statusMessage("Low vBat, cycle aborted!");
    action = "abort";
    update.action = action;
    pushUpdate(update);
    
    return;
  }
  
  if(moisture < settings.moisture_threshold){
    dry_count--;
    //if(timeClient.getHours() >= 6 && timeClient.getHours() <= 18 ){

      if(dry_count <=0){
         statusMessage("Start watering...");
         action = "pump";
         startPumpCicles();
       }
      
    /*}else{
      if(dry_count <= settings.dry_cicles*-1  ){
        statusMessage("Start watering...");
        action = "override working hours";
        startPumpCicles();
      }else{
        Serial.print ( "[FILSDROP] Outside working hours:" ); Serial.println ( timeClient.getHours() );
        action = "outside working hours";
      }
    }*/

  }else{
    dry_count = settings.dry_cicles;
  }

  update.action = action;
  update.dry_count = dry_count;
  pushUpdate(update);
  
}

int getMoisture(uint8_t addr){
  //soilSensorBuss.setI2CAddr(addr);
  return soilSensorBuss.touchRead(0);
}

float getTemperature(uint8_t addr){
  //soilSensorBuss.setI2CAddr(addr);
  return soilSensorBuss.getTemp();
}

char * getDateTime(){
  
  snprintf(datetime, sizeof(datetime), "%d-%d-%d %d:%d:%d", rtc.getYear(),rtc.getMonth(),rtc.getDay(),rtc.getHours(),rtc.getMinutes(),rtc.getSeconds());
  return datetime;
}


float getLiPoVoltage(){
  
   // read the input on analog pin 0:
  int sensorValue = analogRead(ADC_BATTERY);
  // Convert the analog reading (which goes from 0 - 1023) to a voltage (0 - 4.3V):
  float voltage = sensorValue * (4.3 / 1023.0);
  
  return voltage;
}

void startPumpCicles(){

  statusMessage("Start pumping");
  for(int i=settings.pump_pushes;i>0;i--){
    digitalWrite(0, HIGH);
    delay(settings.pump_time*1000);
    digitalWrite(0, LOW);
    delay(settings.pump_time*1000);
  }
  statusMessage("Stop pumping");
}

void statusMessage(char const * message){
  char s[128];
  snprintf(s, sizeof(s), "[FILSDROP] %s", message);
  Serial.println(s);
}
