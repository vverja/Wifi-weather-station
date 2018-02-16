/*
 *  This sketch sends data via HTTP GET requests to data.sparkfun.com service.
 *
 *  You need to get streamId and privateKey at data.sparkfun.com and paste them
 *  below. Or just customize this script to talk to other HTTP servers.
 *
 */
 #include <ESP8266WiFi.h>
 
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
#define DHTPIN 2
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

//Библиотеки для работы с экраном
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27,20,4);


File DataFile;
File IniFile;

struct iniStruct{
  char* ssid;
  char* password;
  char* host; 
  int port;
};
struct iniStruct ini ;

void setup() {
  Serial.begin(115200);
  delay(10);

  
  lcd.begin(0,2);
  lcd.backlight();

  
  //инициализируем время
   Rtc.Begin();

    RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);


    if (!Rtc.IsDateTimeValid()) 
    {
      //если не чип инициализируется после потери питания
        lcd.print("I lost the DateTime!");
        Rtc.SetDateTime(compiled);
    }

    if (!Rtc.GetIsRunning())
    {
        lcd.println("Time is starting now");
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


  readIniFile(&ini);
  
  DHT dht(DHTPIN, DHTTYPE);  

  // We start by connecting to a WiFi network

  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ini.ssid);
  
  /* Explicitly set the ESP8266 to be a WiFi-client, otherwise, it by default,
     would try to act as both a client and an access-point and could cause
     network-issues with your other WiFi-devices on your WiFi-network. */
  WiFi.mode(WIFI_STA);
  WiFi.begin(ini.ssid, ini.password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

int value = 0;

void loop() {
  delay(5000);
  ++value;

  if (!Rtc.IsDateTimeValid()) 
  {
      // Common Cuases:
      //    1) the battery on the device is low or even missing and the power line was disconnected
      Serial.println("RTC lost confidence in the DateTime!");
  }

  RtcDateTime now = Rtc.GetDateTime();

  char* currentDate = printDateTime(now, false);
 
  Serial.print("connecting to ");
  Serial.println(ini.host);
  
  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  //const int httpPort = 80;
  
  if (!client.connect(ini.host, ini.port)) {
    Serial.println("connection failed");
    return;
  }
  
  
  float h = dht.readHumidity();
  
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  writeToSD(h, t, currentDate);
  
//  Serial.print("Requesting URL: ");
//  Serial.println(url);
  
  // This will send the request to the server
//  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
//               "Host: " + host + "\r\n" + 
//               "Connection: close\r\n\r\n");
//  unsigned long timeout = millis();
//  while (client.available() == 0) {
//    if (millis() - timeout > 5000) {
//      Serial.println(">>> Client Timeout !");
//      client.stop();
//      return;
//    }
//  }
  
  // Read all the lines of the reply from server and print them to Serial
//  while(client.available()){
//    String line = client.readStringUntil('\r');
//    Serial.print(line);
//  }
  
  Serial.println();
  Serial.println("closing connection");
}

void readIniFile(struct iniStruct *ini){
  String property, value, temp;
  
  
  IniFile = SD.open("sensor.ini");
  if (IniFile)
    while (IniFile.available()) {
    //while (fscanf (IniFile, "%s%s", property,value) != EOF) {
     char thisChar = IniFile.read();
     temp.concat(thisChar);
     if(thisChar == ':'){
        property = temp;
        temp = "";   
      }
      if(thisChar == '\n'){
        value = temp;
        temp = "";  
      }
      if(property == "host:")
        value.toCharArray(ini->host,value.length());
      else if(property == "ssid:")
        value.toCharArray(ini->ssid,value.length());
      else if(property == "password:")
        value.toCharArray(ini->password,value.length());
      else if(property  == "port:")
        ini->port = value.toInt();
      property= "";
      value = "";
    }
     
}

void writeToSD(float h, float t, char* currentDate){
  String fileName = String(currentDate)+".txt";
  File myFile = SD.open(fileName, FILE_WRITE);

  // if the file opened okay, write to it:
  if (myFile) {
    myFile.print("Humidity: ");
    myFile.print(h);
    myFile.print(" %\t");
    myFile.print("Temperature: ");
    myFile.print(t);
    myFile.print(" *C ");
    
    myFile.close();

  } else {
    // if the file didn't open, print an error:
    Serial.println("error opening test.txt");
  }  
}

#define countof(a) (sizeof(a) / sizeof(a[0]))

char* printDateTime(const RtcDateTime& dt, bool type)
{
    char datestring[20];
  if(type)
    snprintf_P(datestring, 
            countof(datestring),
            PSTR("%02u/%02u/%04u %02u:%02u"),
            dt.Month(),
            dt.Day(),
            dt.Year(),
            dt.Hour(),
            dt.Minute());
  else{
        snprintf_P(datestring, 
            countof(datestring),
            PSTR("%02u_%02u_%04u"),
            dt.Month(),
            dt.Day(),
            dt.Year());    
    } 
    return datestring;
}

