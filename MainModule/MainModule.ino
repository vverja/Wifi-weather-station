 #include <ESP8266WiFi.h>
  WiFiClient client;

 #include <ArduinoOTA.h>
 #include <WiFiUdp.h>

 
 #include <ArduinoJson.h>
 const char *filename = "/config.txt";  // <- SD library uses 8.3 filenames

 /*инициализация сд карты*/
#include <SPI.h>
#include <SD.h>
#include <Wire.h> // must be included here so that Arduino library object file references work
#include <RtcDS1307.h>
RtcDS1307<TwoWire> Rtc(Wire);

//Библиотеки для работы с температурным датчиком
#include "DHT.h"
#define DHTPIN 0 //D2
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

//Библиотеки для работы с экраном
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(PCF8574A_ADDR_A21_A11_A01, 4, 5, 6, 16, 11, 12, 13, 14, POSITIVE);


struct iniStruct{
  char my_name[64];
  char ssid1[64];
  char ssid2[64];
  char password1[100];
  char password2[100];
  char host[64];
  char url[255]; 
  int port;
};
iniStruct config ;

struct dataStruct{
  char id[3];
  char time[22];
  float temp;
  float hum;
  char my_name[64];
  };
dataStruct dataFile;
bool noConnection = false;
RtcDateTime dateOfLostConnection;

void setup() {
  Serial.begin(115200);
  delay(10);
  
  
  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Loading....");
  //инициализируем время
   Wire.begin(4, 5);
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

    Rtc.SetSquareWavePin(DS1307SquareWaveOut_Low); 
  
  Serial.print("Initializing SD card...");

  if (!SD.begin(15)) {
    Serial.println("initialization failed!");
    printError(2);
    return;
  }
  Serial.println("initialization done.");


  //readIniFile(&ini);
  loadConfiguration(filename, config);
  
  DHT dht(DHTPIN, DHTTYPE);  

  // We start by connecting to a WiFi network
 ArduinoOTA.setHostname(config.my_name);
 ArduinoOTA.begin();
 
 WiFi.mode(WIFI_STA);
}

int value = 0;

void loop() {
  if (!Rtc.IsDateTimeValid()) 
  {
      Serial.println("RTC lost confidence in the DateTime! in loop procedure");
      printError(1);
      
  }
  RtcDateTime now = Rtc.GetDateTime();
  char currentDate[20]; 
  printDateTime(currentDate, now, false); 
  char currentDateTime[22]; 
  printDateTime(currentDateTime, now, true);
  
  if (WiFi.status()!= WL_CONNECTED){ 
    Serial.print("Connecting to ");
    Serial.println(config.ssid1);
    WiFi.begin(config.ssid1, config.password1);
  }
  int count = 0; 
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    if (count > 30){
      Serial.println("First wifi AP is unavailable");
      Serial.println("Connecting to second");
      WiFi.begin(config.ssid2, config.password2);  
      count = 0;
      delay(1000);
      if (WiFi.status()!= WL_CONNECTED)
      {
        printError(3);
        break;
      } 
    }
    count++;
  }
  
  if (WiFi.status() == WL_CONNECTED){
    Serial.println("WiFi connected");  
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    printError(4);
    return;
  }
  writeToSD(h, t, currentDate, currentDateTime);
  lcd.setCursor(0, 1); 
  lcd.print(String(config.my_name) + " " + String(int(t))+ "*C" + " " +String(int(h))+"%");
  Serial.print("connecting to ");
  Serial.println(config.host);
  if(!postPage(config.host,config.port,config.url,"obj=" + getJsonString(t, h, currentDateTime)))
  {
      if(!noConnection)
      {
        dateOfLostConnection = now;
        noConnection = true;
      }
      Serial.println("No connection with Server");   
  }
  else 
  {
     if (noConnection)
     {
        noConnection = false;  
        sendLostDataToServer(dateOfLostConnection,now);
     }
    Serial.print(F("Pass "));
  }
  for(int i=0; i < 1800; i++){
      now = Rtc.GetDateTime();
      printDateTime(currentDateTime, now, true);
      lcd.setCursor(0, 0); 
      lcd.print(currentDateTime);
      ArduinoOTA.handle();
      delay(1000);
    }
}

void loadConfiguration(const char *filename, iniStruct &config) {
  File file = SD.open(filename);
  StaticJsonBuffer<512> jsonBuffer;
  JsonObject &root = jsonBuffer.parseObject(file);
  if (!root.success())
    Serial.println(F("Failed to read file"));
  strlcpy(config.my_name, root["my_name"], sizeof(config.my_name));
  strlcpy(config.ssid1, root["ssid1"],sizeof(config.ssid1));
  strlcpy(config.ssid2, root["ssid2"],sizeof(config.ssid2));
  strlcpy(config.host, root["host"], sizeof(config.host));
  strlcpy(config.password1, root["password1"], sizeof(config.password1));
  strlcpy(config.password2, root["password2"], sizeof(config.password2));
  strlcpy(config.url, root["url"], sizeof(config.url));
  config.port = root["port"];
  file.close();
}

void writeToSD(float h, float t, char currentDate[], char currentDateTime[]){
  String fileName = String(currentDate);
  fileName.trim();
  fileName = fileName + ".txt";
  Serial.println("Trying to write into the file " + fileName);
  File myFile = SD.open(fileName, FILE_WRITE);
  if (myFile) {
    myFile.println(getJsonString(t, h, currentDateTime));
    myFile.close();
  } else {
    Serial.println("error opening " + fileName);
    printError(5);
  }  
}

#define countof(a) (sizeof(a) / sizeof(a[0]))
void printDateTime(char datestring[], const RtcDateTime& dt, bool type)
{
  if(type)
    snprintf_P(datestring, 
            20,
            PSTR("%04u-%02u-%02u %02u:%02u:%02u"),
            dt.Year(),
            dt.Month(),
            dt.Day(),
            dt.Hour(),
            dt.Minute(),
            dt.Second());
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
String getJsonString(float t, float h, char* currentDateTime){ 
    return "{\"id\":1, \"time\":\"" + String(currentDateTime) + "\", \"temp\":" + String(t) + ", \"hum\":" + String(h) + ", \"name\":\""+config.my_name+ "\"}";
  }
//This module sends the data which never been send from last onnection lost
void sendLostDataToServer(const RtcDateTime& dateOfLostConnection,  const RtcDateTime& currentDate){
  char lostDateString[20]; 
  char nowDateString[20]; 
  char xxx[20];
  RtcDateTime tempTime = dateOfLostConnection;
  printDateTime(lostDateString, tempTime, false);
  printDateTime(nowDateString, currentDate, false);
    
  DynamicJsonBuffer jsonBuffer; 
  JsonArray& array = jsonBuffer.createArray();
  
  String temp;

 do
  {
    printDateTime(lostDateString, tempTime, false);
    String fileName =  String(lostDateString) + ".txt";
    File file = SD.open(fileName);
    if(file)
    {
          while (file.available()) 
          {
            char a = file.read();
            if (a!='\n')
            {
                temp += a;
            }
            else
            {
                const char* jsonDuplicate = jsonBuffer.strdup(temp); // <- make a copy in the JsonBuffer
                array.add(jsonBuffer.parseObject(jsonDuplicate));
                temp="";
            }
          }
    }
    tempTime += 86400;
  } while(strcheck(lostDateString, nowDateString)==0);
  
  for(int i=0; i<array.size(); i++)
  { 
    float t = array[i]["temp"];
    float h = array[i]["hum"];
    const char* timer = array[i]["time"];
    char data[12];
    char time[8];
    convertTime(timer, data, time);
    RtcDateTime tempActionTime(data, time);
    printDateTime(xxx, tempActionTime, true);
    if (tempActionTime > dateOfLostConnection)
      postPage(config.host,config.port,config.url,"obj=" + getJsonString(t, h, xxx));
  }
}

 void printError(int e){
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Error " + String(e));
  }


int strcheck(char *str1, char *str2){
  int i = 0;
  while(str1[i] != '\0' && str2[i] != '\0'){
    if(str1[i] != str2[i])
      return 0;
    i++;
  }
  //Если мы вышли из цикла значит одну из строк мы перебрали до конца
  if(str1[i] == '\0' && str2[i] == '\0')
    return 1;
  return 0;
}

void convertTime(const char* timer, char* newTime, char* time)
{
  char year[5];
  char month[4];
  char day[3];   
  newTime[0] = 0;
  strlcpy(year, timer, 5);
  strlcpy(month, timer+5, 4);
  strlcpy(day, timer+8, 3);
  switch(month[1]){
      case '1':
        if(month[0]=='1')
        {
          month[0] = 'N';
          month[1] = 'o';
          month[2] = 'v';  
        }
        else
        {
          month[0] = 'J';
          month[1] = 'a';
          month[2] = 'n';
        }
        break;
      case '2':
        if(month[0]=='1')
        {
          month[0] = 'D';
          month[1] = 'e';
          month[2] = 'c';          
        }
        else
        {
          month[0] = 'F';
          month[1] = 'e';          
          month[2] = 'b';          
        }
        break;  
      case '3':
        month[0] = 'M';
        month[1] = 'a';
        month[2] = 'r';
        break;  
      case '4':
        month[0] = 'A';
        month[1] = 'p';
        month[2] = 'r';      
        break;  
      case '5':
        month[0] = 'M';
        month[1] = 'a';
        month[2] = 'y';
        break;
      case '6':
        month[0] = 'J';
        month[1] = 'u';
        month[2] = 'n';
        break;  
      case '7':
        month[0] = 'J';
        month[1] = 'u';
        month[2] = 'l';
        break; 
      case '8':
        month[0] = 'A';
        month[1] = 'u';
        month[2] = 'g';
        break;   
      case '9':
        month[0] = 'S';
        month[1] = 'e';
        month[2] = 'p';
        break;
      case '0':
        month[0] = 'O';
        month[1] = 'c';
        month[2] = 't';
        break;    
  }

  if(day[0]=='0')
    day[0]=' ';
    
  strcat(newTime, month);
  strcat(newTime, " ");
  strcat(newTime, day);
  strcat(newTime, " ");
  strcat(newTime, year);
  
  for(int i=11; i<20; i++)
  {
    time[i-11] = timer[i];
  }
  
}


