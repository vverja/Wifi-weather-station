/*
 *  This sketch sends data via HTTP GET requests to data.sparkfun.com service.
 *
 *  You need to get streamId and privateKey at data.sparkfun.com and paste them
 *  below. Or just customize this script to talk to other HTTP servers.
 *
 */
 
 #include <ESP8266WiFi.h>

   
  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  
 #include <ArduinoJson.h>
 const char *filename = "/config.txt";  // <- SD library uses 8.3 filenames

 
 /*инициализация сд карты*/
#include <SPI.h>
#include <SD.h>

//Библиотеки для работы со временем
// pins are D1=5 and D2=4.
#include <Wire.h> // must be included here so that Arduino library object file references work
#include <RtcDS1307.h>
RtcDS1307<TwoWire> Rtc(Wire);


//Библиотеки для работы с температурным датчиком
#include "DHT.h"
#define DHTPIN 4 //D2
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

//Библиотеки для работы с экраном
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(PCF8574_ADDR_A21_A11_A01, 4, 5, 6, 16, 11, 12, 13, 14, POSITIVE);


File DataFile;
//File IniFile;

struct iniStruct{
  char ssid[64];
  char password[100];
  char host[64];
  char url[255]; 
  int port;
};
iniStruct config ;

void setup() {
  Serial.begin(115200);
  delay(10);

  lcd.begin(16, 2);
  lcd.clear();

  
  //инициализируем время
   Wire.begin(0, 2);
   Rtc.Begin();

    RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);


    if (!Rtc.IsDateTimeValid()) 
    {
      //если не чип инициализируется после потери питания
        Serial.print("I lost the DateTime!");
        Rtc.SetDateTime(compiled);
    }

    if (!Rtc.GetIsRunning())
    {
        Serial.println("Time is starting now");
        Rtc.SetIsRunning(true);
    }

    RtcDateTime now = Rtc.GetDateTime();
    if (now < compiled) 
    {
        Serial.println("RTC is older than compile time!  (Updating DateTime)");
        Rtc.SetDateTime(compiled);
    }
    

    // never assume the Rtc was last configured by you, so
    // just clear them to your needed state
    Rtc.SetSquareWavePin(DS1307SquareWaveOut_Low); 
  
  Serial.print("Initializing SD card...");

  if (!SD.begin(4)) {
    Serial.println("initialization failed!");
    return;
  }
  Serial.println("initialization done.");


  //readIniFile(&ini);
  loadConfiguration(filename, config);
  
  DHT dht(DHTPIN, DHTTYPE);  

  // We start by connecting to a WiFi network

  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(config.ssid);
  
  /* Explicitly set the ESP8266 to be a WiFi-client, otherwise, it by default,
     would try to act as both a client and an access-point and could cause
     network-issues with your other WiFi-devices on your WiFi-network. */
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.ssid, config.password);
  int count = 0; 
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    if (count > 15){
      WiFi.begin("finance", "kvadrat1");
      Serial.print("\nConnecting to ");
      Serial.println("finance");
      while (WiFi.status() != WL_CONNECTED) {
          delay(1000);
          Serial.print(".");    
          if (count > 15){
            ESP.reset();  
          }
        }    
    }
    count++;
  }

  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

int value = 0;

void loop() {


  if (!Rtc.IsDateTimeValid()) 
  {
      // Common Cuases:
      //    1) the battery on the device is low or even missing and the power line was disconnected
      Serial.println("RTC lost confidence in the DateTime!");
  }

  RtcDateTime now = Rtc.GetDateTime();
  char currentDate[20]; 
  printDateTime(currentDate, now, false);
  Serial.println(currentDate);
  
  char currentDateTime[20]; 
  printDateTime(currentDateTime, now, true);
  Serial.println(currentDateTime);
  
  Serial.print("connecting to ");
  Serial.println(config.host);

   
  float h = dht.readHumidity();
  
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  writeToSD(h, t, currentDate, currentDateTime);
  
  if(!postPage(config.host,config.port,"/deviceController/setDataFromWifiDevices",getJsonString(t, h))) Serial.print(F("Fail "));
    else Serial.print(F("Pass "));
  delay(3600000);
}


void loadConfiguration(const char *filename, iniStruct &config) {
  // Open file for reading
  File file = SD.open(filename);

  // Allocate the memory pool on the stack.
  // Don't forget to change the capacity to match your JSON document.
  // Use arduinojson.org/assistant to compute the capacity.
  StaticJsonBuffer<512> jsonBuffer;

  // Parse the root object
  JsonObject &root = jsonBuffer.parseObject(file);

  if (!root.success())
    Serial.println(F("Failed to read file"));
    
  // Copy values from the JsonObject to the Config
  strlcpy(config.ssid, root["ssid"],sizeof(config.ssid));
  strlcpy(config.host, root["host"], sizeof(config.host));
  strlcpy(config.password, root["password"], sizeof(config.password));
  strlcpy(config.url, root["url"], sizeof(config.url));
  config.port = root["port"];
  // Close the file (File's destructor doesn't close the file)
  file.close();
}

void writeToSD(float h, float t, char currentDate[], char currentDateTime[]){
  String fileName = String(currentDate);
  fileName.trim();
  fileName = fileName + ".txt";
  Serial.println("Trying to write into the file " + fileName);
  File myFile = SD.open(fileName, FILE_WRITE);
  
  // if the file opened okay, write to it:
  if (myFile) {
    myFile.print(">>> ");
    myFile.print(currentDateTime);
    myFile.print("H: ");
    myFile.print(h);
    myFile.print(" %\t");
    myFile.print("T: ");
    myFile.print(t);
    myFile.print(" *C ");
    myFile.print("\n");
    myFile.close();

  } else {
    // if the file didn't open, print an error:
    Serial.println("error opening " + fileName);
  }  
}

#define countof(a) (sizeof(a) / sizeof(a[0]))

void printDateTime(char datestring[], const RtcDateTime& dt, bool type)
{
  if(type)
    snprintf_P(datestring, 
            20,
            PSTR("%02u/%02u/%04u %02u:%02u"),
            dt.Day(),
            dt.Month(),
            dt.Year(),
            dt.Hour(),
            dt.Minute());
  else{
        snprintf_P(datestring, 
            20,
            PSTR("%02u%02u%04u"),
            dt.Day(),
            dt.Month(),
            dt.Year());    
    } 
}

byte postPage(char* domainBuffer,int thisPort,char* page,String thisData)
{
  int inChar;
  char outBuf[64];
  Serial.println(thisData);
  Serial.print(F("connecting..."));

  if(client.connect(domainBuffer,thisPort) == 1)
  {
    Serial.println(F("connected"));

    // send the header
    sprintf(outBuf,"POST %s HTTP/1.1",page);
    client.println(outBuf);
    sprintf(outBuf,"Host: %s",domainBuffer);
    client.println(outBuf);
    client.println(F("Connection: close\r\nContent-Type: application/x-www-form-urlencoded"));
    sprintf(outBuf,"Content-Length: %u\r\n",(thisData.length()));
    client.println(outBuf);

    // send the body (variables)
    client.print(thisData);
  } 
  else
  {
    Serial.println(F("failed"));
    return 0;
  }

  int connectLoop = 0;

  while(client.connected())
  {
    while(client.available())
    {
      inChar = client.read();
      Serial.write(inChar);
      connectLoop = 0;
    }

    delay(1);
    connectLoop++;
    if(connectLoop > 10000)
    {
      Serial.println();
      Serial.println(F("Timeout"));
      client.stop();
    }
  }

  Serial.println();
  Serial.println(F("disconnecting."));
  client.stop();
  return 1;
}
String getJsonString(float t, float h){
    //sprintf(outBuf,"{\'obj\':\'{\'id\':\'1\', \'type\':\'W\', \'temp\':\'%u\', \'hum\':\'%u\'}\'}",t,h);
    
    return "obj={\"id\":1, \"type\":\"W\", \"temp\":" + String(t) + ", \"hum\":" + String(h) + ", \"name\":\"Device1\"}";
    
  }

