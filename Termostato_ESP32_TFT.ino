/*
     ESP8266-12 - TFT 1.8" - D18B20  Webserver prova caldaia
*/

#include <TFT_eSPI.h>
#include <SPI.h>
#include <WiFi.h>
#include <Wire.h>
#include <Button2.h>
#include "bmp.h"

#include <WiFiClient.h>
#include <WebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>

#include "RTClib.h"
#include <WiFiUdp.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>    //  https://github.com/adafruit/Adafruit-GFX-Library

#ifndef ST7789_DRIVER
#define TFT_DRIVER 0x7789
#endif

#ifndef TFT_SLPIN
#define TFT_SLPIN   0x10
#endif

//#define TFT_MOSI            19
//#define TFT_SCLK            18
//#define TFT_CS              5
//#define TFT_DC              16
//#define TFT_RST             23

#define TFT_BL          4  // Display backlight control pin
#define ADC_EN          14
#define ADC_PIN         34
#define BUTTON_1        35
#define BUTTON_2        0

TFT_eSPI tft = TFT_eSPI(135, 240); // Invoke custom library
Button2 btn1(BUTTON_1);
Button2 btn2(BUTTON_2);

char buff[512];
int vref = 1100;
int btnCick = false;

const char* ssid     = "Michele";
const char* password = "bffmhl77b22b885a";

WebServer server(80);
RTC_Millis RTC;

const byte Rele = 16 ;
const byte puls_auto = 0 ;
#define ONE_WIRE_BUS 4  // DS18B20 pin su GPIO 4 ESP8266-12

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);
unsigned int localPort = 2390;      // local port to listen for UDP packets
IPAddress timeServerIP(217, 147, 223, 78); //europe.pool.ntp.org
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

WiFiUDP udp;
unsigned long tempo ;
byte isteresi  ;
byte automatico  ; // 1 = locale         0 = Web

byte flag_ora = 0 ;
int value_loc ;
char TextBox1[10] = "";
//int value_web = atoi(TextBox1);
int value_web ;

const byte eep_man_aut = 2 ; // locazione EEProm Locale/Web
const byte eep_isteresi = 1 ; // locazione EEProm valore isteresi
const byte eep_temp_web = 0 ; // Locazione EEprom per temperatura

byte statopulsante_AUT = 1;         // stato corrente del pulsante:
byte last_statopulsante_AUT = 1;    // precedente stato del pulsante:

float tempC;
String webString = "";   // String to display
unsigned long previousMillis = 0;
const long interval = 2000;

DeviceAddress insideThermometer;

void handle_root() {
  gettemperature();       // read sensor
  prepara_txtbox() ;
  webString = "<p><em>";
  webString += "<h2>Temperatura: " + String((float)tempC) + " C";
  webString += "<br>\r\n";
  String rele_stat;
  if ( digitalRead(Rele) == HIGH ) {
    rele_stat = "Accesa" ;
  }
  else {
    rele_stat = "Spenta" ;
  }
  webString += "La caldaia e' " + rele_stat  ;
  webString += "<br><A HREF='auto'>  Web/Locale On/Off </A> ";
  webString += "</h2>\n</p>";
  webString += "<p><em><div style='text-align: center;'>";
  webString += "<A HREF='cmd=on'><h1> Accende </A> &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp";
  webString += "<A HREF='cmd=off'>  Spegne </A> </h1> <br>";
  webString += "<A HREF='cmd=blank'><h1>Refresh Page</A> <br>";
  webString += "<a href='+inc'> <img src ='https://sites.google.com/site/qepcafe/_/rsrc/1341933313362/misc/media/clker-green-plus-md.png?height=100&width=100'></a>";
  webString += "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp";
  webString +=  TextBox1 ;
  webString += "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp";
  webString += "<a href='-dec'> <img src ='https://sites.google.com/site/qepcafe/_/rsrc/1341933313306/misc/media/clker-red-minus-md.png?height=100&width=100'></h1></a>";
  webString += "<br><br>";
  webString += "<h2>Isteresi</h2><br>";
  webString += "<h2><a href='piu_ist'> <img src ='https://sites.google.com/site/qepcafe/_/rsrc/1341933313362/misc/media/clker-green-plus-md.png?height=50&width=50'></a>";
  webString += "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp";
  webString +=  EEPROM.read(eep_isteresi);
  webString += "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp";
  webString += "<a href='meno_ist'> <img src ='https://sites.google.com/site/qepcafe/_/rsrc/1341933313306/misc/media/clker-red-minus-md.png?height=50&width=50'></h2></a>";
  webString += "</em></p></body></html>";
  server.send(200, "text/html", webString);
}

void setup()
{
  EEPROM.begin(10);
  automatico = EEPROM.read(eep_man_aut); // legge su EEprom se Locale o WEB
  value_web = EEPROM.read(eep_temp_web); // legge su EEpromtemperatura impostata da Web

  pinMode ( Rele, OUTPUT);
  digitalWrite ( Rele, LOW);
  pinMode(puls_auto, INPUT_PULLUP);

  //Serial.begin(115200);
  RTC.begin(DateTime()); // Avvia RTC software
  DS18B20.setResolution(insideThermometer, 12); // Risoluzione sensore

  tft.init();
  tft.setRotation(2); // Sceglie il verso di visualizzazione
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(0, 0);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(1);

  tft.setSwapBytes(true);
  //  tft.pushImage(0, 0,  240, 135, ttgo);
  delay(5000);

  // Connect to WiFi network
  WiFi.begin(ssid, password);
  // In attesa di connessione

  tft.println("   TERMOSTATO");
  tft.println();
  while (WiFi.status() != WL_CONNECTED) {
    delay(2000);
    // Serial.print(".");
    tft.print(".");
  }
  tft.fillScreen(TFT_WHITE);
  tft.setTextColor(TFT_RED);
  // tft.setRotation(2);
  tft.println("");
  tft.println("Connesso a ");
  tft.println(ssid);
  tft.println("IP address: ");
  tft.println(WiFi.localIP());

  server.on("/", handle_root);
  server.on("/cmd=blank", handle_root);

  server.on("/cmd=on", []() { // accende relè
    digitalWrite (Rele, HIGH);
    handle_root();
  });

  server.on("/cmd=off", []() { // spegne relè
    digitalWrite (Rele, LOW);
    handle_root();
  });

  server.on("/+inc", []() { // aumenta temperatura
    if ( automatico == 0 ) {
      value_web = EEPROM.read (eep_temp_web);
      value_web++ ;
      if (value_web > 30 ) {
        value_web = 30 ;
      }
      EEPROM.write(eep_temp_web, value_web );
      EEPROM.commit();
    }
    ctrl_ora() ;
    ctrl_on_off() ;
    handle_root();

  });

  server.on("/-dec", []() { // diminuisce temperatura
    if ( automatico == 0 ) {
      value_web = EEPROM.read (eep_temp_web);
      value_web-- ;
      if (value_web < 10 ) {
        value_web = 30 ;
      }
      EEPROM.write(eep_temp_web, value_web );
      EEPROM.commit();
    }
    ctrl_ora() ;
    ctrl_on_off() ;
    handle_root();

  });

  server.on("/piu_ist", []() { // aumenta isteresi
    isteresi = EEPROM.read(eep_isteresi);
    isteresi++ ;
    if (isteresi > 4 ) { // valore massimo isteresi
      isteresi = 4 ;
    }
    EEPROM.write(eep_isteresi, isteresi );
    EEPROM.commit();

    ctrl_ora() ;
    ctrl_on_off() ;
    handle_root();
  });

  server.on("/meno_ist", []() { // diminuisce isteresi
    isteresi = EEPROM.read(eep_isteresi);
    isteresi-- ;
    if (isteresi <= 1 ) { // valore minimo isteresi
      isteresi = 1 ;
    }
    EEPROM.write(eep_isteresi, isteresi );
    EEPROM.commit();
    ctrl_ora() ;
    ctrl_on_off() ;
    handle_root();
  });

  server.on("/auto", []() { // Automatico / Manuale
    automatico = EEPROM.read(eep_man_aut) ;
    automatico = !automatico ;
    EEPROM.write(eep_man_aut, automatico );
    EEPROM.commit();

    ctrl_ora() ;
    ctrl_on_off() ;
    handle_root();
  });

  udp.begin(localPort);
  //Serial.print("Local port: ");
  //Serial.println(udp.localPort());

  server.begin();
  //Serial.println("HTTP server started");

  legge_ntp();
  gettemperature() ;
}

//*****************************************************
void loop()
{
  server.handleClient();


  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    gettemperature() ;

    tft.fillScreen(TFT_BLACK);
    scrive_data();

    tft.setCursor ( 1, 40 ); // scrive temperatura sul display
    tft.setTextColor(TFT_YELLOW);
    tft.setTextSize(3);

    tft.print(tempC);
    tft.print(" C");
    vis_modo_auto(); // scrive sul display la modalita'
    value_loc = map (analogRead(A0), 0, 1023, 15, 31 ); //lettura_adc
    scrive_temp_impostata() ; // scrive temperatura impostata
  }
  lettura_pulsanti() ;
  ctrl_ora() ;
  ctrl_on_off() ;
  delay(500);
} // fine loop **************************************

void gettemperature() {
  do {
    DS18B20.requestTemperatures();
    tempC = DS18B20.getTempCByIndex(0);
    Serial.print("Temperatura: ");
    Serial.println(tempC);
  } while (tempC == 85.0 || tempC == (-127.0));
}

void scrive_data()  {
  DateTime now = RTC.now();
  tft.setCursor(6, 2);
  tft.setTextColor(TFT_GREEN);
  tft.setTextSize(2);
  switch (now.dayOfTheWeek() ) {   //RTClib_ADA
    case 0:
      tft.print(F("DOM"));
      break;
    case 1:
      tft.print(F("LUN"));
      break;
    case 2:
      tft.print(F("MAR"));
      break;
    case 3:
      tft.print(F("MER"));
      break;
    case 4:
      tft.print(F("GIO"));
      break;
    case 5:
      tft.print(F("VEN"));
      break;
    case 6:
      tft.print(F("SAB"));
      break;
  }
  tft.print(" ");
  if (now.day() < 10)
    tft.print("0");
  tft.print(now.day());
  tft.print("/");
  if (now.month() < 10)
    tft.print("0");
  tft.print(now.month());
  tft.setCursor(30, 20);
  if (now.hour() < 10)
    tft.print(" ");
  tft.print(now.hour());
  tft.print(":");
  if (now.minute() < 10)
    tft.print("0");
  tft.print(now.minute());

}

void legge_ntp() {
  for (int i = 0; i <= 6 ; i++) {
    NTP() ;
    if ( tempo > 1111111111 ) {
      RTC.adjust(DateTime(tempo));
      break ;
    }
    delay (3000);
  }
}

void NTP()
{
  sendNTPpacket(timeServerIP); // send an NTP packet to a time server
  delay(1000);

  if (udp.parsePacket()) {
    udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    const unsigned long seventyYears = 2208988800UL;
    unsigned long epoch = secsSince1900 - seventyYears; // toglie 70 anni
    // print Unix time:
    Serial.println(epoch);
    //print the hour, minute and second:
    Serial.print("The UTC time is ");       // UTC is the time at Greenwich Meridian (GMT)
    Serial.print((epoch  % 86400L) / 3600); // print the hour (86400 equals secs per day)
    Serial.print(':');
    tempo = (epoch + 3600) ; // aggiunge 1 ora
    if ( epoch > 1459036800  && epoch < 1477785600 ) { // ora legale 27/03/2016 - 30/10/2016
      tempo = (tempo + 3600) ; // aggiunge 1 ora
    }
    if ( epoch > 1490486400  && epoch < 1509235200 ) { // ora legale 26/03/2017 - 29/10/2017
      tempo = (tempo + 3600) ; // aggiunge 1 ora
    }
    if ( epoch > 1521936000  && epoch < 1540684800 ) { // ora legale 25/03/2018 - 28/10/2018
      tempo = (tempo + 3600) ; // aggiunge 1 ora
    }
    if ( epoch > 1553990400  && epoch < 1572134400 ) { // ora legale 31/03/2019 - 27/10/2019
      tempo = (tempo + 3600) ; // aggiunge 1 ora
    }
    if ( epoch > 1585440001  && epoch < 1603584001 ) { // ora legale 29/03/2020 - 25/10/2020
      tempo = (tempo + 3600) ; // aggiunge 1 ora
    }

    if ( ((epoch % 3600) / 60) < 10 ) {
      //In the first 10 minutes of each hour, we'll want a leading '0'
      Serial.print('0');
    }
      Serial.print((epoch  % 3600) / 60); // print the minute (3600 equals secs per minute)
Serial.print(': ');
      if ( (epoch % 60) < 10 ) {
      //In the first 10 seconds of each minute, we'll want a leading '0'
      Serial.print('0');
    }
    Serial.println(epoch % 60); // print the second
  }
}

unsigned long sendNTPpacket(IPAddress& address)
{
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

void vis_modo_auto() {
  tft.setTextSize(2);
  tft.setCursor(0, 140 );
  if ( digitalRead(Rele) == HIGH ) {
    tft.setTextColor(TFT_YELLOW);
    tft.print("Acceso ");
  }
  else {
    tft.setTextColor(TFT_RED);
    tft.print("Spento ");
  }
  tft.setCursor(80, 140);
  if (  automatico == 1 ) {
    tft.print("Loc");
  }
  else {
    tft.print("Web");
  }
  tft.print ( EEPROM.read(eep_isteresi) );
}

//********************** se si vuole impostare degli orari
void ctrl_ora() {
  DateTime now = RTC.now();
  if (( now.hour() * 60) +  now.minute()  < 360 ) { // tieni spento fino alle 6
    flag_ora = 0 ;
  }
  else {
    flag_ora = 1 ;
  }
  if (( now.hour() * 60) + now.minute()  > 1350 )  { // ore 22.30
    flag_ora = 0 ;
  }
}

void ctrl_on_off() { // Accende-Spegne Relè
  if ( automatico == 1 ) {
    if ( flag_ora == 1 ) {
      if ( tempC < value_loc - EEPROM.read(eep_isteresi) )  {
        digitalWrite(Rele, HIGH);   // Acceso
      }
      if ( tempC > value_loc )  {
        digitalWrite(Rele, LOW);   // Spento
      }
    }
    else {
      digitalWrite(Rele, LOW);   // orario notturno
    }
  }
  if ( automatico == 0 ) {
    if ( tempC < value_web - EEPROM.read(eep_isteresi) )  {
      digitalWrite(Rele, HIGH);   // Acceso
    }
    if ( tempC > value_web )  {
      digitalWrite(Rele, LOW);   // Spento
    }
  }
}


void prepara_txtbox() {
  // itoa (value_web, TextBox1, 10 );
  automatico =    EEPROM.read(eep_man_aut);
  if ( automatico == 1) {
    itoa (value_loc, TextBox1, 10 );
    strcat(TextBox1, " Loc");
  }
  else {
    itoa (value_web, TextBox1, 10 );
    strcat(TextBox1, " Web");
  }
}

void scrive_temp_impostata() {
  tft.setTextSize(2);
  if ( automatico == 1 ) {
    tft.setTextColor(TFT_YELLOW);
  }
  else {
    tft.setTextColor(TFT_RED);
  }

  tft.setCursor(4, 78);
  tft.print(F("T.Loc "));
  tft.setTextSize(3);
  tft.setCursor(82, 75);
  tft.print(value_loc);

  if ( automatico == 1 ) {
    tft.setTextColor(TFT_RED);
  }
  else {
    tft.setTextColor(TFT_YELLOW);
  }
  tft.setTextSize(2);
  tft.setCursor(4, 109);
  tft.print(F("T.Web "));
  tft.setTextSize(3);
  tft.setCursor(82, 106);
  tft.print(value_web);

}

void lettura_pulsanti() {
  statopulsante_AUT = digitalRead(puls_auto);              // Leggi il Pin del pulsante:
  if (statopulsante_AUT != last_statopulsante_AUT ) {

    if (statopulsante_AUT == LOW) {
      automatico =  !automatico ;
      EEPROM.write(eep_man_aut, automatico);
      delay (50 ) ;
      EEPROM.commit();
      prepara_txtbox();
      vis_modo_auto(); // scrive sul display la modalita'
    }
  }
  last_statopulsante_AUT = statopulsante_AUT ;
}
