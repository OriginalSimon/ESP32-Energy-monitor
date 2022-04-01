// Библиотеки
#include <WiFi.h> // Подключение WiFi
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <DHT.h> 
#include <Ticker.h>
#include <Wire.h>
#include "time.h"
#include "EmonLib.h" // Расчёт потреблений          
#include <SPI.h>
#include <Adafruit_GFX.h> // Дисплей 1306
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>

#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64 
#define VOLT_CAL 510  
#define CURRENT_CAL 62.6
#define DHTPIN 14

#define DHTTYPE    DHT11
DHT dht(DHTPIN, DHTTYPE);

// Данные сети 
const char* WIFI_SSID = "---";
const char* WIFI_PASSWOERD = "simon007";
//------------------------------------------------------------
int calYear = 2022;                //По умолчанию
double kWhCost = 1.68;             //По умолчанию        
double kWh = 0;
double calCost = 0;
double Cost = 0;
unsigned long lastmillis;
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 7200;   //7200 секунд на синхронизацию с нашим временем
const int   daylightOffset_sec = 0; //Я проигнорировал это смещение
struct tm timeinfo;
int second, minute, hour, day, month, year, weekday;
//------------------------------------------------------------
float t;
float f;
float h;
float realPower;
float apparentPower;
float supplyVoltage;
float currentDraw;
unsigned long lastTime = 0;  
unsigned long timerDelay = 5000;  // таймер отправки показаний
// Активаторы
EnergyMonitor emon1;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void SensorData ();
Ticker timer;

AsyncWebServer server(80); 
WebSocketsServer websockets(81);

//------------------------------------------------------------------------
void printLocalTime(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
   }
}
//-------------------------------------------------------------------------
void notFound(AsyncWebServerRequest *request)
{
  request->send(404, "text/plain", "Страница не найдена");
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

  switch (type) 
  {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] ¡Отключено!\n", num);
      break;
    case WStype_CONNECTED: {
        IPAddress ip = websockets.remoteIP(num);
        Serial.printf("[%u] Подключенный %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
      }
      break;
    case WStype_TEXT:
      Serial.printf("[%u] Texto: %s\n", num, payload);
      String mensaje = String((char*)( payload));
      Serial.println(mensaje);
      
      DynamicJsonDocument doc(200); // документ (емкость)
      DeserializationError error = deserializeJson(doc, mensaje);
      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return;
        }
      }
}

void setup(void)
{
  
  Serial.begin(115200);
  dht.begin();
  // ----------------------------------------------------------------------------------------
  emon1.voltage(35, VOLT_CAL, 1.7);  //Напряжение: входной контакт, калибровка, фазовый сдвиг
  emon1.current(34, CURRENT_CAL);    // Ток: входной контакт, калибровка.
  // ----------------------------------------------------------------------------------------
  WiFi.begin(WIFI_SSID, WIFI_PASSWOERD);
  Serial.print("Подключен к сети");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Полученый IP: ");
  Serial.println(WiFi.localIP());
  
  if(!SPIFFS.begin(true)){
    Serial.println("Произошла ошибка при чтения SPIFFS");
    return;
  }

   server.on("/", HTTP_GET, [](AsyncWebServerRequest * request)
  { 
   request->send(SPIFFS, "/index.html", "text/html");
  });
  
  // Инициализация дисплея ------------------------------------------------------------------
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  delay(2000);
  display.clearDisplay();
  display.setTextColor(WHITE);
  // ----------------------------------------------------------------------------------------
  
  server.onNotFound(notFound);
  server.begin();
  
  websockets.begin();
  websockets.onEvent(webSocketEvent);

  timer.attach(2,SensorData); // Тикерный таймер (вызывает функции с заданным интервалом)

  //инициализируем и получаем время-----------------------------------------------------------
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();
  //------------------------------------------------------------------------------------------
}

void loop(void) {
  websockets.loop();

  //---------------------------------------------------------------------------------------------------
  printLocalTime();
  second = timeinfo.tm_sec;
  minute = timeinfo.tm_min;
  hour   = timeinfo.tm_hour;
  day    = timeinfo.tm_mday;
  month  = timeinfo.tm_mon + 1;
  year   = timeinfo.tm_year + 1900;
  //weekday = timeinfo.tm_wday + 1;
  //---------------------------------------------------------------------------------------------------
  
  // --------------------------------------------------------------------------------------------------
  emon1.calcVI(20,2000);         // //Рассчитать все.  Количество полудлин волн (пересечений), тайм-аут  

  realPower       = emon1.realPower;        //извлекаем реальную мощность в переменную 
  apparentPower   = emon1.apparentPower;    //извлекаем кажущуюся мощность в переменную 
  supplyVoltage   = emon1.Vrms;             //извлекаем Vrms в переменную 
  currentDraw     = emon1.Irms;             //извлекаем Irms в переменную 
  // --------------------------------------------------------------------------------------------------

  //---------------------------------------------------------------------------------------------------
    //Расчетная формула для показаний кВтч 
  kWh += (((millis() - lastmillis) * realPower) / 3600000000);
  lastmillis = millis();
  Cost = kWh * kWhCost;
  
  //Все следующие операторы if для сброса показаний стоимости и кВтч 
  if (day == 30 && month == 4 && year == calYear && hour == 23 && minute == 59) {
    calCost += Cost; kWh = 0;//April
  }
  if (day == 31 && month == 5 && year == calYear && hour == 23 && minute == 59) {
    calCost += Cost; kWh = 0;//May
  }
  if (day == 30 && month == 6 && year == calYear && hour == 23 && minute == 59) {
    calCost += Cost; kWh = 0;//June
  }
  if (day == 31 && month == 7 && year == calYear && hour == 23 && minute == 59) {
    calCost += Cost; kWh = 0;//July
  }
  if (day == 31 && month == 8 && year == calYear && hour == 23 && minute == 59) {
    calCost += Cost; kWh = 0;//August
  }
  if (day == 30 && month == 9 && year == calYear && hour == 23 && minute == 59) {
    calCost += Cost; kWh = 0;//September
  }
  if (day == 31 && month == 10 && year == calYear && hour == 23 && minute == 59) {
    calCost += Cost; kWh = 0;//October
  }
  if (day == 30 && month == 11 && year == calYear && hour == 23 && minute == 59) {
    calCost += Cost; kWh = 0;//November
  }
  if (day == 31 && month == 12 && year == calYear && hour == 23 && minute == 59) {
    calCost += Cost; kWh = 0; calCost = 0;//December
  }
  if (day == 31 && month == 1 && year == calYear && hour == 23 && minute == 59) {
    calCost += Cost; kWh = 0;//January
  }
  if (day == 28 && month == 2 && year == calYear && hour == 23 && minute == 59) {
    calCost += Cost; kWh = 0;//February
  }
  if (day == 31 && month == 3 && year == calYear && hour == 23 && minute == 59) {
    calCost += Cost; kWh = 0;//March
  }
  if ((millis() - lastTime) > timerDelay) {
    
    SensorData();
    Serial.print("IP= ");
    Serial.println(WiFi.localIP());
    Serial.printf("Temperature = %.2f ºC \n", t);
    Serial.printf("Temperature = %.2f ºF \n", f);
    Serial.printf("Humidity= %f %\n", h);
    Serial.print("Voltage= ");
    Serial.println(supplyVoltage);
    Serial.print("Current= ");
    Serial.println(currentDraw);
    Serial.print("Watts= ");
    Serial.println(currentDraw * supplyVoltage);
    Serial.print("kWh= ");
    Serial.println(kWh);
    Serial.print("Cost= ");
    Serial.println(Cost);
    Serial.print("realPower= ");
    Serial.println(realPower);
    Serial.println("\n\n");
  // --------------------------------------------------------------------------------------------------

  //---------------------------------------------------------------------------------------------------
    // Дисплей вывод данных
  display.clearDisplay();
  //  WiFi.localIP()
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("IP: ");
  display.setTextSize(1);;
  display.print(WiFi.localIP());
  //  температура
  display.setTextSize(1);
  display.setCursor(0,10);
  display.print("Temperature: ");
  display.setTextSize(1);
  display.print(t);
  display.print(" ");
  display.setTextSize(1);
  display.cp437(true);
  display.write(167);
  display.setTextSize(1);
  display.print("C");
  // play влажность
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.print("Humidity: ");
  display.setTextSize(1);
  display.print(h);
  display.print(" %");  
   //  напряжение
  display.setTextSize(1);
  display.setCursor(0, 30);
  display.print("Voltage: ");
  display.setTextSize(1);;
  display.print(supplyVoltage);
  display.print(" V "); 
     //  сила тока
  display.setTextSize(1);
  display.setCursor(0, 40);
  display.print("Current: ");
  display.setTextSize(1);
  display.print(currentDraw);
  display.print(" A "); 
  //  мощность
  display.setTextSize(1);
  display.setCursor(0, 50);
  display.print("Watts: ");
  display.setTextSize(1);
  display.print(realPower);
  display.print(" W "); 
  display.display();
  //  мощность
  display.setTextSize(1);
  display.setCursor(0, 50);
  display.print("Cost: ");
  display.setTextSize(1);
  display.print(Cost);
  display.print(" UAH "); 
  display.display();
  //---------------------------------------------------------------------------------------------------
  }
}

void SensorData() {
  
   h = dht.readHumidity();
   t = dht.readTemperature();
   f = dht.readTemperature(true);
     
   if (isnan(h) || isnan(t)) {
    Serial.println(F("Ошибка чтения в датчике DHT11!"));
    return;
    }
   
   String JSON_Data = "{\"temp\":";
          JSON_Data += t;
          JSON_Data += ",\"hum\":";
          JSON_Data += h;
          JSON_Data += ",\"farn\":";
          JSON_Data += f;
          JSON_Data += ",\"rPower\":";
          JSON_Data += realPower;
          JSON_Data += ",\"aPower\":";
          JSON_Data += apparentPower;
          JSON_Data += ",\"Voltage\":";
          JSON_Data += supplyVoltage;
          JSON_Data += ",\"Current\":";
          JSON_Data += currentDraw;
          JSON_Data += ",\"kWh_V\":";
          JSON_Data += kWh;
          JSON_Data += ",\"Cost_V\":";
          JSON_Data += Cost;
          JSON_Data += "}";
          
   Serial.println(JSON_Data);
   websockets.broadcastTXT(JSON_Data);  // отправляем данные всем подключенным клиентам
}
