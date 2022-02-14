//Библиотеки
#include <Wire.h>
#include "DHT.h"
#include "time.h"
#include <Wire.h>
#include <WiFi.h>// Подключение WiFi
#include "ESPAsyncWebServer.h"
#include "EmonLib.h" // Расчёт потреблений          
#include <SPI.h>
#include <Adafruit_GFX.h> // Дисплей 1306
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <DHT.h> // Датчик температуры 
// Переменые 
#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64 
#define VOLT_CAL 148.7  
#define CURRENT_CAL 62.6
#define DHTTYPE DHT11 // DHT 11
//
int calYear = 2022;                                                             //Default
double kWhCost = 1.68;                                                             //Default                                                             //Default
double kWh = 0;
double calCost = 0;
double Cost = 0;
unsigned long lastmillis;
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 7200;                                                        //7200 seconds to sync with our time
const int   daylightOffset_sec = 0;                                                      //I ignored this offset
struct tm timeinfo;
int second, minute, hour, day, month, year, weekday;
//
uint8_t DHTPin = 14;
float temperature_Celsius;
float temperature_Fahrenheit;
float Humidity;
float currentDraw;
float supplyVoltage;
unsigned long lastTime = 0;  
unsigned long timerDelay = 5000;  // таймер отправки показаний
// Активаторы
EnergyMonitor emon1;
DHT dht(DHTPin, DHTTYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
// Данные сети
const char* ssid = "---";
const char* password = "simon007";
AsyncWebServer server(80);
AsyncEventSource events("/events");

void printLocalTime(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
   }
}

void getDHTReadings(){
 
  Humidity = dht.readHumidity();
  // Read temperature as Celsius (the default)
  temperature_Celsius = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  temperature_Fahrenheit= dht.readTemperature(true);

}

String processor(const String& var){
  getDHTReadings();
  //Serial.println(var);
  if(var == "TEMPERATURE_C"){
    return String(temperature_Celsius);
  }
  else if(var == "TEMPERATURE_F"){
    return String(temperature_Fahrenheit);
  }
   else if(var == "HUMIDITY"){
    return String(Humidity);
  }
  else if(var == "supplyVoltage"){
    return String(supplyVoltage);
  }
  else if(var == "currentDraw"){
    return String(currentDraw);
  }
  else if(var == "POWER"){
    return String(currentDraw * supplyVoltage);
  }
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <script src="https://kit.fontawesome.com/027df4bff5.js" crossorigin="anonymous"></script>
  <title>Web Server ESP</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <script src="https://code.highcharts.com/highcharts.js"></script>
  <link rel="shortcut icon" href="data:," >
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    p {  font-size: 1.2rem;}
    body {  margin: 0;}
    .topnav { overflow: hidden; background-color: #9370DB; color: white; font-size: 1.7rem; }
    .content { padding: 20px; }
    .card { background-color: white; box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5); }
    .cards { max-width: 700px; margin: 0 auto; display: grid; grid-gap: 2rem; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); }
    .reading { font-size: 2.8rem; }
    .card.temperature { color: #0e7c7b; }
    .card.humidity { color: #17bebb; }
    .card.voltage { color: #FFA500; }
    .card.current { color: #FF4500; }
    .card.watt { color: #00FA9A; }
    .card.Cost { color: #571B7E ; }
    .card.kWh { color: #6CC417 ; }
    .card.realPower { color: #52595D ; }
    .card.grafic {
      min-width: 310px;
      max-width: 700px;
      height: 300px;
      margin: 30px auto;
    }
  </style>
</head>
<body>
  <div class="topnav">
    <h3>WEB SERVER ESP</h3>
  </div>
  <div class="content">
    <div class="cards">
      <div class="card temperature">
        <h4><i class="fas fa-thermometer-half"></i> TEMPERATURE</h4><p><span class="reading"><span id="temp_celcius">%TEMPERATURE_C%</span> &deg;C</span></p>
      </div>
      <div class="card temperature">
        <h4><i class="fas fa-thermometer-half"></i> TEMPERATURE</h4><p><span class="reading"><span id="temp_fahrenheit">%TEMPERATURE_F%</span> &deg;F</span></p>
      </div>
      <div class="card humidity">
        <h4><i class="fas fa-tint"></i> HUMIDITY</h4><p><span class="reading"><span id="hum">%HUMIDITY%</span> &percnt;</span></p>
      </div>
      <div class="card voltage">
        <h4><i class="fas fa-bolt"></i> VOLTAGE</h4><p><span class="reading"><span id="supplyVoltage">%supplyVoltage%</span> V</span></p>
      </div>
      <div class="card current">
        <h4><i class="fas fa-car-battery"></i> CURRENT </h4><p><span class="reading"><span id="currentDraw">%currentDraw%</span> A </span></p>
      </div>
      <div class="card watt">
        <h4><i class="fab fa-accessible-icon"></i> POWER </h4><p><span class="reading"><span id="apparentPower">%apparentPower%</span> W </span></p>
      </div>
      <div class="card kWh">
        <h4><i class="fa-solid fa-arrow-trend-up"></i> kWh </h4><p><span class="reading"><span id="kWh">%kWh%</span> kWh </span></p>
      </div>
      <div class="card Cost">
        <h4><i class="fa-solid fa-coins"></i> Cost </h4><p><span class="reading"><span id="Cost">%Cost%</span> <i class="fa-solid fa-wallet"></i> </span></p>
      </div>
      <div class="card realPower">
        <h4><i class="fa-solid fa-plug"></i> Real Power </h4><p><span class="reading"><span id="realPower">%realPower%</span> W </span></p>
      </div>
    </div>
    <div class="card grafic" id="chart-currentDraw" class="container"></div>
  </div>
<script>
if (!!window.EventSource) {
 var source = new EventSource('/events');
 
 source.addEventListener('open', function(e) {
  console.log("Events Connected");
 }, false);
 source.addEventListener('error', function(e) {
  if (e.target.readyState != EventSource.OPEN) {
    console.log("Events Disconnected");
  }
 }, false);
 
 source.addEventListener('message', function(e) {
  console.log("message", e.data);
 }, false);
 
 source.addEventListener('temperature_Celsius', function(e) {
  console.log("temperature", e.data);
  document.getElementById("temp_celcius").innerHTML = e.data;
 }, false);
 
 source.addEventListener('temperature_Fahrenheit', function(e) {
  console.log("temperature", e.data);
  document.getElementById("temp_fahrenheit").innerHTML = e.data;
 }, false);
 source.addEventListener('humidity', function(e) {
  console.log("humidity", e.data);
  document.getElementById("hum").innerHTML = e.data;
 }, false);
 source.addEventListener('supplyVoltage', function(e) {
  console.log("supplyVoltage", e.data);
  document.getElementById("supplyVoltage").innerHTML = e.data;
 }, false);
 source.addEventListener('currentDraw', function(e) {
  console.log("currentDraw", e.data);
  document.getElementById("currentDraw").innerHTML = e.data;
 }, false);
 source.addEventListener('apparentPower', function(e) {
  console.log("apparentPower", e.data);
  document.getElementById("apparentPower").innerHTML = e.data;
 }, false); 
  source.addEventListener('kWh', function(e) {
  console.log("kWh", e.data);
  document.getElementById("kWh").innerHTML = e.data;
 }, false); 
  source.addEventListener('Cost', function(e) {
  console.log("Cost", e.data);
  document.getElementById("Cost").innerHTML = e.data;
 }, false);
  source.addEventListener('realPower', function(e) {
  console.log("realPower", e.data);
  document.getElementById("realPower").innerHTML = e.data;
 }, false);
}
//grafic
var chartW = new Highcharts.Chart({
  chart:{ renderTo:'chart-currentDraw' },
  title: { text: 'Current' },
  series: [{
    showInLegend: false,
    data: []
  }],
  plotOptions: {
    line: { animation: true,
      dataLabels: { enabled: true }
    }
  },
  xAxis: {
    type: 'datetime',
    dateTimeLabelFormats: { second: '%H:%M:%S' }
  },
  yAxis: {
    title: { text: 'Current (A)' }
  },
  credits: { enabled: false }
});
setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      var x = (new Date()).getTime(),
          y = parseFloat(this.responseText);
      //console.log(this.responseText);
      if(chartW.series[0].data.length > 100) {
        chartW.series[0].addPoint([x, y], true, true, true);
      } else {
        chartW.series[0].addPoint([x, y], true, false, true);
      }
    }
  };
  xhttp.open("GET", "/currentDraw", true);
  xhttp.send();
}, 5000 ) ;
//grafic
</script>
</body>
</html>)rawliteral";

void setup() {
  Serial.begin(115200);
  // Датчик DHT11
  pinMode(DHTPin, INPUT);
  dht.begin();
  
  // Основная библиотека
  emon1.voltage(35, VOLT_CAL, 1.7);  //Напряжение: входной контакт, калибровка, фазовый сдвиг
  emon1.current(34, CURRENT_CAL);    // Ток: входной контакт, калибровка.
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Setting as a Wi-Fi Station..");
  }
  Serial.print("Station IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.println();
  
  // Инициализация дисплея
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  delay(2000);
  display.clearDisplay();
  display.setTextColor(WHITE);
  
  // Handle Web Server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });
  // Handle Web Server Events
  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    client->send("hello!", NULL, millis(), 5000);
  });
  server.addHandler(&events);
  server.begin();
  
  //инициализируем и получаем время 
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();
}

void loop() {
  
  printLocalTime();
  second = timeinfo.tm_sec;
  minute = timeinfo.tm_min;
  hour   = timeinfo.tm_hour;
  day    = timeinfo.tm_mday;
  month  = timeinfo.tm_mon + 1;
  year   = timeinfo.tm_year + 1900;
  //weekday = timeinfo.tm_wday + 1;
    
  
  
  emon1.calcVI(20,2000);         // //Рассчитать все.  Количество полудлин волн (пересечений), тайм-аут  

  float realPower       = emon1.realPower;        //извлекаем реальную мощность в переменную 
  float apparentPower   = emon1.apparentPower;    //извлекаем кажущуюся мощность в переменную 
  float supplyVoltage   = emon1.Vrms;             //извлекаем Vrms в переменную 
  float currentDraw     = emon1.Irms;             //извлекаем Irms в переменную 
  
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
    
    getDHTReadings();
    Serial.print("IP= ");
    Serial.println(WiFi.localIP());
    Serial.printf("Temperature = %.2f ºC \n", temperature_Celsius);
    Serial.printf("Temperature = %.2f ºF \n", temperature_Fahrenheit);
    Serial.printf("Humidity= %f %\n", Humidity);
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

    // Отправка событий на веб-сервер с показаниями датчика
    events.send("ping",NULL,millis());
    events.send(String(temperature_Celsius).c_str(),"temperature_Celsius",millis());
    events.send(String(temperature_Fahrenheit).c_str(),"temperature_Fahrenheit",millis());
    events.send(String(Humidity).c_str(),"humidity",millis());
    events.send(String(supplyVoltage).c_str(),"supplyVoltage",millis());
    events.send(String(currentDraw).c_str(),"currentDraw",millis());
    events.send(String(apparentPower).c_str(),"apparentPower",millis());
    events.send(String(kWh).c_str(),"kWh",millis());
    events.send(String(Cost).c_str(),"Cost",millis());
    events.send(String(realPower).c_str(),"realPower",millis());
    lastTime = millis();
      }
      
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
  display.print(temperature_Celsius);
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
  display.print(Humidity);
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
  
}
