#include <ESP8266WiFi.h> 
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFiMulti.h>

#include <Wire.h>
#include <SFE_MicroOLED.h>
#include <Adafruit_BMP085.h>

#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
void MQTT_connect();

/************************* SETUP *********************************/
const bool LOGGING = false;

const bool useImperial = false;

const char* wifiSSID = "";
const char* wifiPassword = "";
const int connectionRetrys = 30;

const char* mqttServer = "io.adafruit.com";
const int   mqttPort = 1883;                   //8883 for SSL
const String mqttUser = "";
const char* mqttPassword = "";

const String temperatureFeed = mqttUser + "/feeds/temperature";
const String pressureFeed = mqttUser + "/feeds/pressure";
const String altitudeFeed = mqttUser + "/feeds/altitude";

const int oledJumper = 0;  // I2C 0=0x3C, 1=0x3D
const int logDelay = 10000; //how many millis between log entry (adafruit limits to 1 datapoint/s so 3000 should be minimum)
/************************* Other settings ****************************/
ESP8266WiFiMulti WiFiMulti;
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, mqttServer, mqttPort, mqttUser.c_str(), mqttPassword);
Adafruit_MQTT_Publish temperature = Adafruit_MQTT_Publish(&mqtt, temperatureFeed.c_str());
Adafruit_MQTT_Publish pressure = Adafruit_MQTT_Publish(&mqtt, pressureFeed.c_str());
Adafruit_MQTT_Publish altitude = Adafruit_MQTT_Publish(&mqtt, altitudeFeed.c_str());

MicroOLED oled(255, oledJumper); //255 = no reset pin
Adafruit_BMP085 bmp;

int SCREEN_WIDTH = oled.getLCDWidth();
int SCREEN_HEIGHT = oled.getLCDHeight();

bool wifiTimeOut = true;

float metersToFeet(float meters)
{
  return meters * 3.28084;
}

float celsiusToFahrenheit(float celsius)
{
  return celsius * 1.8 + 32;
}

void setup()
{
  Serial.begin(115200);
  if(LOGGING)
    WiFiMulti.addAP(wifiSSID, wifiPassword);
  bmp.begin();
  oled.begin();
  oled.clear(PAGE);
  oled.println("Connecting");
  oled.display();
  if(!LOGGING)
    return;
  wifiTimeOut = false;
  for(int i = 0; i < connectionRetrys; i++)
  {
    if(WiFiMulti.run() == WL_CONNECTED) return;
    oled.print(".");
    oled.display();
    delay(1000);
  }
  wifiTimeOut = true;
}

void loop()
{
  float t = bmp.readTemperature();
  if(useImperial)
    t = celsiusToFahrenheit(t);
  float p = bmp.readPressure();
  float a = bmp.readAltitude();
  if(useImperial) 
    a = metersToFeet(bmp.readAltitude());
  showValues(t, p, a);
  if(!wifiTimeOut)
    mqttLog(t, p, a);
}

int accumulatedValues = 0;
double tSum = 0;
double pSum = 0;
double aSum = 0;
unsigned long proviousTime = 0;

void mqttLog(float t, float p, float a)
{
  tSum += t;
  pSum += p;
  aSum += a;
  accumulatedValues++;
  unsigned long currentTime = millis();
  if(proviousTime > currentTime || proviousTime + logDelay <= currentTime)
  {
    MQTT_connect();
    temperature.publish(tSum / accumulatedValues);
    pressure.publish(pSum / accumulatedValues);
    altitude.publish(aSum / accumulatedValues);
    tSum = pSum = aSum = 0;
    accumulatedValues = 0;
    proviousTime = currentTime;
  }
}

void showValues(float t, float p, float a)
{
  oled.clear(PAGE);
  oled.setCursor(0, 0);
  oled.print(t);
  if(useImperial)
    oled.println("F");
  else
    oled.println("C");
  oled.print(p * 0.01f);
  oled.println("hPa");
  oled.setCursor(0, 16);
  oled.print(a);
  if(useImperial)
    oled.println("ft");
  else
    oled.println("m");
  oled.display();
}

void MQTT_connect() {
  int8_t ret;
  if (mqtt.connected())
    return;
  oled.clear(PAGE);
  oled.setCursor(0, 0);
  oled.println("Connecting MQTT");
  oled.display();
  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       oled.println(mqtt.connectErrorString(ret));
       oled.println("Retrying...");
       oled.display();
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds
       retries--;
       if (retries == 0) {
         // basically die and wait for WDT to reset me
         while (1);
       }
       oled.clear(PAGE);
       oled.setCursor(0, 0);
  }
  oled.println("MQTT Connected!");
}
