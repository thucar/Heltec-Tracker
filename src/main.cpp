#include <LoRaWan_APP.h>
#include <Arduino.h>
#include <GPS_Air530.h>
#include <cubecell_SSD1306Wire.h>
#include <OneButton.h>
#include "auth.h"

extern SSD1306Wire  display; 

//when gps waked, if in GPS_UPDATE_TIMEOUT, gps not fixed then into low power mode
#define GPS_UPDATE_TIMEOUT 120000

//once fixed, GPS_CONTINUE_TIME later into low power mode
#define GPS_CONTINUE_TIME 1000
/*
   set LoraWan_RGB to Active,the RGB active in loraWan
   RGB red means sending;
   RGB purple means joined done;
   RGB blue means RxWindow1;
   RGB yellow means RxWindow2;
   RGB green means received done;
*/

/* OTAA para*/
uint8_t devEui[] = DEV_EUI;
uint8_t appEui[] = APP_EUI;
uint8_t appKey[] = APP_KEY;

/* ABP para*/
uint8_t nwkSKey[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
uint8_t appSKey[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
uint32_t devAddr =  ( uint32_t )0x00000000;

/*LoraWan channelsmask, default channels 0-7*/ 
uint16_t userChannelsMask[6]={ 0x00FF,0x0000,0x0000,0x0000,0x0000,0x0000 };

/*LoraWan region, select in arduino IDE tools*/
LoRaMacRegion_t loraWanRegion = ACTIVE_REGION;

/*LoraWan Class, Class A and Class C are supported*/
DeviceClass_t  loraWanClass = LORAWAN_CLASS;

/*the application data transmission duty cycle.  value in [ms].*/
uint32_t appTxDutyCycle = 1000*30;

/*OTAA or ABP*/
bool overTheAirActivation = LORAWAN_NETMODE;

/*ADR enable*/
bool loraWanAdr = LORAWAN_ADR;

/* set LORAWAN_Net_Reserve ON, the node could save the network info to flash, when node reset not need to join again */
bool keepNet = LORAWAN_NET_RESERVE;

/* Indicates if the node is sending confirmed or unconfirmed messages */
bool isTxConfirmed = LORAWAN_UPLINKMODE;

// The interrupt pin is attached to USER_KEY
OneButton btn = OneButton(
  USER_KEY,  // Input pin for the button
  true,        // Button is active LOW
  false         // Enable internal pull-up resistor
);

bool oledStatus = true;


/* Application port */
uint8_t appPort = 10;
/*!
* Number of trials to transmit the frame, if the LoRaMAC layer did not
* receive an acknowledgment. The MAC performs a datarate adaptation,
* according to the LoRaWAN Specification V1.0.2, chapter 18.4, according
* to the following table:
*
* Transmission nb | Data Rate
* ----------------|-----------
* 1 (first)       | DR
* 2               | DR
* 3               | max(DR-1,0)
* 4               | max(DR-1,0)
* 5               | max(DR-2,0)
* 6               | max(DR-2,0)
* 7               | max(DR-3,0)
* 8               | max(DR-3,0)
*
* Note, that if NbTrials is set to 1 or 2, the MAC will not decrease
* the datarate, in case the LoRaMAC layer did not receive an acknowledgment
*/
uint8_t confirmedNbTrials = 4;

int32_t fracPart(double val, int n)
{
  return (int32_t)((val - (int32_t)(val))*pow(10,n));
}

void VextON(void)
{
  pinMode(Vext,OUTPUT);
  digitalWrite(Vext, LOW);
}

void VextOFF(void) //Vext default OFF
{
  pinMode(Vext,OUTPUT);
  digitalWrite(Vext, HIGH);
}

void displayGPSInfo()
{
  char str[30];
  display.clear();
  display.setFont(ArialMT_Plain_10);
  int index = sprintf(str,"%02d-%02d-%02d",Air530.date.year(),Air530.date.day(),Air530.date.month());
  str[index] = 0;
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 0, str);
  
  index = sprintf(str,"%02d:%02d:%02d",Air530.time.hour()+2,Air530.time.minute(),Air530.time.second(),Air530.time.centisecond());
  str[index] = 0;
  display.drawString(60, 0, str);

  if( Air530.location.age() < 1000 )
  {
    display.drawString(120, 0, "A");
  }
  else
  {
    display.drawString(120, 0, "V");
  }
  
  index = sprintf(str,"alt: %d.%d",(int)Air530.altitude.meters(),fracPart(Air530.altitude.meters(),2));
  str[index] = 0;
  display.drawString(0, 16, str);
   
  index = sprintf(str,"hdop: %d.%d",(int)Air530.hdop.hdop(),fracPart(Air530.hdop.hdop(),2));
  str[index] = 0;
  display.drawString(0, 32, str); 
 
  index = sprintf(str,"lat :  %d.%d",(int)Air530.location.lat(),fracPart(Air530.location.lat(),4));
  str[index] = 0;
  display.drawString(60, 16, str);   
  
  index = sprintf(str,"lon:%d.%d",(int)Air530.location.lng(),fracPart(Air530.location.lng(),4));
  str[index] = 0;
  display.drawString(60, 32, str);

  index = sprintf(str,"speed: %d.%d km/h",(int)Air530.speed.kmph(),fracPart(Air530.speed.kmph(),3));
  str[index] = 0;
  display.drawString(0, 48, str);
  display.display();

  index = sprintf(str,"sats:%d",(int)Air530.satellites.value());
  str[index] = 0;
  display.drawString(87, 48, str);
  display.display();
}

void printGPSInfo()
{
  Serial.print("Date/Time: ");
  if (Air530.date.isValid())
  {
    Serial.printf("%d/%02d/%02d",Air530.date.year(),Air530.date.day(),Air530.date.month());
  }
  else
  {
    Serial.print("INVALID");
  }

  if (Air530.time.isValid())
  {
    Serial.printf(" %02d:%02d:%02d.%02d",Air530.time.hour(),Air530.time.minute(),Air530.time.second(),Air530.time.centisecond());
  }
  else
  {
    Serial.print(" INVALID");
  }
  Serial.println();
  
  Serial.print("LAT: ");
  Serial.print(Air530.location.lat(),6);
  Serial.print(", LON: ");
  Serial.print(Air530.location.lng(),6);
  Serial.print(", ALT: ");
  Serial.print(Air530.altitude.meters());

  Serial.println(); 
  
  Serial.print("SATS: ");
  Serial.print(Air530.satellites.value());
  Serial.print(", HDOP: ");
  Serial.print(Air530.hdop.hdop());
  Serial.print(", AGE: ");
  Serial.print(Air530.location.age());
  Serial.print(", COURSE: ");
  Serial.print(Air530.course.deg());
  Serial.print(", SPEED: ");
  Serial.println(Air530.speed.kmph());
  Serial.println();
}

static void prepareTxFrame( uint8_t port )
{
  /*appData size is LORAWAN_APP_DATA_MAX_SIZE which is defined in "commissioning.h".
    appDataSize max value is LORAWAN_APP_DATA_MAX_SIZE.
    if enabled AT, don't modify LORAWAN_APP_DATA_MAX_SIZE, it may cause system hanging or failure.
    if disabled AT, LORAWAN_APP_DATA_MAX_SIZE can be modified, the max value is reference to lorawan region and SF.
    for example, if use REGION_CN470,
    the max value for different DR can be found in MaxPayloadOfDatarateCN470 refer to DataratesCN470 and BandwidthsCN470 in "RegionCN470.h".
  */

  float lat, lon, alt, course, speed, hdop, sats;
  
  Serial.println("Waiting for GPS FIX ...");

  VextON();// oled power on;
  delay(10); 
  
  Air530.begin();
  Air530.sendcmd("$PGKC115,1,0,0,1*2B\r\n"); // mode: gps, glonass, beidou, galileo [0: off, 1: on]
  if( Air530.location.age() > 5000 )
  {
    if (oledStatus)
    {
      display.init();
      display.clear();  
      display.setTextAlignment(TEXT_ALIGN_CENTER);
      display.setFont(ArialMT_Plain_16);
      display.drawString(64, 32-16/2, "Looking for sats");
      display.display();
    }
    
    Serial.print("Age:");
    Serial.println(Air530.location.age());
    Serial.println("Looking for satellites");
  }
  
  uint32_t start = millis();
  while( (millis()-start) < GPS_UPDATE_TIMEOUT )
  { 
    btn.tick();
    while (Air530.available() > 0)
    {
      btn.tick();
      Air530.encode(Air530.read());
    }
   // gps fixed in a second
    if( Air530.location.age() < 1000 )
    {
      break;
    }
  }

  //if gps fixed,  GPS_CONTINUE_TIME later stop GPS into low power mode, and every 1 second update gps, print and display gps info
  if(Air530.location.age() < 1000)
  {
    start = millis();
    uint32_t printinfo = 0;
    while( (millis()-start) < GPS_CONTINUE_TIME )
    {
      btn.tick();
      while (Air530.available() > 0)
      {
        btn.tick();
        Air530.encode(Air530.read());
      }

      if( (millis()-start) > printinfo )
      {
        printinfo += 1000;
        printGPSInfo();
        if (oledStatus)
        {
          displayGPSInfo();
        }
      }
    }
  }
  else
  {
    if (oledStatus)
    {
      display.init();
      display.clear();  
      display.setTextAlignment(TEXT_ALIGN_CENTER);
      display.setFont(ArialMT_Plain_16);
      display.drawString(64, 32-16/2, "No GPS signal");
      display.display();
    }
    Serial.println("No GPS signal");
    delay(2000);
  }
  Air530.end(); 
  //display.clear();
  //display.display();
  //display.stop();
  //VextOFF(); //oled power off
  
  lat = Air530.location.lat();
  lon = Air530.location.lng();
  alt = Air530.altitude.meters();
  course = Air530.course.deg();
  speed = Air530.speed.kmph();
  sats = Air530.satellites.value();
  hdop = Air530.hdop.hdop();

  uint16_t batteryVoltage = getBatteryVoltage();
  uint32_t LatitudeBinary = ((lat + 90) / 180.0) * 16777215;
  uint32_t LongitudeBinary = ((lon + 180) / 360.0) * 16777215;
  uint16_t Altitude = alt;
  uint8_t hdopGps = hdop;
  uint8_t satellites = sats;
  unsigned char *puc;

  appDataSize = 0;
  appData[appDataSize++] = (LatitudeBinary >> 16) & 0xFF;
  appData[appDataSize++] = (LatitudeBinary >> 8) & 0xFF;
  appData[appDataSize++] = LatitudeBinary & 0xFF;
  appData[appDataSize++] = (LongitudeBinary >> 16) & 0xFF;
  appData[appDataSize++] = (LongitudeBinary >> 8) & 0xFF;
  appData[appDataSize++] = LongitudeBinary & 0xFF;
  appData[appDataSize++] = (Altitude >> 8) & 0xFF;
  appData[appDataSize++] = Altitude & 0xFF;
  appData[appDataSize++] = hdopGps & 0xFF;
  appData[appDataSize++] = satellites & 0xFF;
  puc = (unsigned char *)(&course);
  appData[appDataSize++] = puc[0];
  appData[appDataSize++] = puc[1];
  appData[appDataSize++] = puc[2];
  appData[appDataSize++] = puc[3];
  puc = (unsigned char *)(&speed);
  appData[appDataSize++] = puc[0];
  appData[appDataSize++] = puc[1];
  appData[appDataSize++] = puc[2];
  appData[appDataSize++] = puc[3];
  appData[appDataSize++] = (uint8_t)(batteryVoltage >> 8);
  appData[appDataSize++] = (uint8_t)batteryVoltage;
  appData[appDataSize++] = (uint8_t)(Air530.location.age() < 1000);

  Serial.print("SATS: ");
  Serial.print(Air530.satellites.value());
  Serial.print(", HDOP: ");
  Serial.print(Air530.hdop.hdop());
  Serial.print(", LAT: ");
  Serial.print(Air530.location.lat());
  Serial.print(", LON: ");
  Serial.print(Air530.location.lng());
  Serial.print(", AGE: ");
  Serial.print(Air530.location.age());
  Serial.print(", ALT: ");
  Serial.print(Air530.altitude.meters());
  Serial.print(", COURSE: ");
  Serial.print(Air530.course.deg());
  Serial.print(", SPEED: ");
  Serial.println(Air530.speed.kmph());
  Serial.print(" BatteryVoltage:");
  Serial.println(batteryVoltage);
}

// Handler function for a single click:
static void handleClick() {
  Serial.println("Single Clicked!");
  if (oledStatus){
    display.displayOff();
    oledStatus = false;
  } else {
    display.displayOn();
    oledStatus = true;
  }
}

static void handleDoubleClick() {
  Serial.println("Double Clicked!");
}

static void handleHeld(){
  Serial.println("Held!");
}

static void handleReleased(){
  Serial.println("Released!");
}

void btnInterrupt(void) {
  // keep watching the push button:
  btn.tick(); // just call tick() to check the state.
}

void setup()
{
  boardInitMcu();
  Serial.begin(115200);

#if(AT_SUPPORT)
  enableAt();
#endif
  LoRaWAN.displayMcuInit();
  deviceState = DEVICE_STATE_INIT;
  LoRaWAN.ifskipjoin();

  // Single Click event attachment
  btn.attachClick(handleClick);

  // Double Click event attachment with lambda
  //btn.attachDoubleClick(handleDoubleClick);

  btn.attachLongPressStart(handleHeld);

  btn.attachLongPressStop(handleReleased);

  btn.setPressTicks(2000);

  btn.tick();

  attachInterrupt(USER_KEY,btnInterrupt, CHANGE);
}

void loop()
{
  btn.tick();
  switch( deviceState )
  {
    case DEVICE_STATE_INIT:
    {
#if(AT_SUPPORT)
      getDevParam();
#endif
      printDevParam();
      LoRaWAN.init(loraWanClass,loraWanRegion);
      deviceState = DEVICE_STATE_JOIN;
      break;
    }
    case DEVICE_STATE_JOIN:
    {
      LoRaWAN.displayJoining();
      LoRaWAN.join();
      break;
    }
    case DEVICE_STATE_SEND:
    {
      prepareTxFrame( appPort );
      //LoRaWAN.displaySending();
      LoRaWAN.send();
      deviceState = DEVICE_STATE_CYCLE;
      break;
    }
    case DEVICE_STATE_CYCLE:
    {
      // Schedule next packet transmission
      txDutyCycleTime = appTxDutyCycle + randr( 0, APP_TX_DUTYCYCLE_RND );
      LoRaWAN.cycle(txDutyCycleTime);
      deviceState = DEVICE_STATE_SLEEP;
      break;
    }
    case DEVICE_STATE_SLEEP:
    {
      //LoRaWAN.displayAck();
      LoRaWAN.sleep();
      break;
    }
    default:
    {
      deviceState = DEVICE_STATE_INIT;
      break;
    }
  }
}