#include <Wire.h>
#include <RtcDS3231.h>                        // Include RTC library by Makuna: https://github.com/Makuna/Rtc
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <FastLED.h>
#include <FS.h>                               // Please read the instructions on http://arduino.esp8266.com/Arduino/versions/2.3.0/doc/filesystem.html#uploading-files-to-file-system
#define countof(a) (sizeof(a) / sizeof(a[0]))
#define NUM_LEDS 30                           // Total of 86 LED's     
#define DATA_PIN D7                           // Change this if you are using another type of ESP board than a WeMos D1 Mini
#define MILLI_AMPS 2000
#define COUNTDOWN_OUTPUT D7

#define WIFIMODE 0                            // 0 = Only Soft Access Point, 1 = Only connect to local WiFi network with UN/PW, 2 = Both

#if defined(WIFIMODE) && (WIFIMODE == 0 || WIFIMODE == 2)
const char* APssid = "CLOCK_AP";
const char* APpassword = "dupadupa";
#endif

#if defined(WIFIMODE) && (WIFIMODE == 1 || WIFIMODE == 2)
// #include "Credentials.h"                    // Create this file in the same directory as the .ino file and add your credentials (#define SID YOURSSID and on the second line #define PW YOURPASSWORD)
const char *ssid = "Bog Wybacza Arka Nigdy 2.4";
const char *password = "matkaboska1000";
#endif


CRGBPalette16 palette = RainbowColors_p;
uint8_t currentHue = 1;
uint8_t baseHue = 1;
byte currentM2 = -1; // current minutes (for leds update)

RtcDS3231<TwoWire> Rtc(Wire);
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdateServer;
CRGB LEDs[NUM_LEDS];

// Settings
unsigned long prevTime = 0;
byte r_val = 0;
byte g_val = 64;
byte b_val = 200;
bool dotsOn = true;
byte brightness = 225;
float temperatureCorrection = -3.0;
byte temperatureSymbol = 12;                  // 12=Celcius, 13=Fahrenheit check 'numbers'
byte clockMode = 0;                           // Clock modes: 0=Clock, 1=Countdown, 2=Temperature, 3=Scoreboard
unsigned long countdownMilliSeconds;
unsigned long endCountDownMillis;
byte hourFormat = 24;                         // Change this to 12 if you want default 12 hours format instead of 24
CRGB countdownColor = CRGB::Green;
byte scoreboardLeft = 0;
byte scoreboardRight = 0;
CRGB scoreboardColorLeft = CRGB::Green;
CRGB scoreboardColorRight = CRGB::Red;
CRGB alternateColor = CRGB::Black;
long numbers[] = {
  0b1110111, //0b000111111111111111111,  // [0] 0
  0b0010001, //0b000111000000000000111,  // [1] 1
  0b1101011, //0b111111111000111111000,  // [2] 2
  0b0111011, //0b111111111000000111111,  // [3] 3
  0b0011101, //0b111111000111000000111,  // [4] 4
  0b0111110, //0b111000111111000111111,  // [5] 5
  0b1111110,  //0b111000111111111111111,  // [6] 6
  0b0010011, //0b000111111000000000111,  // [7] 7
  0b1111111, //0b111111111111111111111,  // [8] 8
  0b0111111, //0b111111111111000111111,  // [9] 9
  0b0000000, //0b000000000000000000000,  // [10] off
  0b0001111,  //0b111111111111000000000,  // [11] degrees symbol
  0b1100110, //0b000000111111111111000,  // [12] C(elsius)
  0b0111001,  //0b111000111111111000000,  // [13] F(ahrenheit)
};


void setup() {
  pinMode(COUNTDOWN_OUTPUT, OUTPUT);
  Serial.begin(115200);
  delay(200);

  Serial.print("Setup.");

  // RTC DS3231 Setup
  Rtc.Begin();
  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);

  if (!Rtc.IsDateTimeValid()) {
    if (Rtc.LastError() != 0) {
      // we have a communications error see https://www.arduino.cc/en/Reference/WireEndTransmission for what the number means
      Serial.print("RTC communications error = ");
      Serial.println(Rtc.LastError());
    } else {
      // Common Causes:
      //    1) first time you ran and the device wasn't running yet
      //    2) the battery on the device is low or even missing
      Serial.println("RTC lost confidence in the DateTime!");
      // following line sets the RTC to the date & time this sketch was compiled
      // it will also reset the valid flag internally unless the Rtc device is
      // having an issue
      Rtc.SetDateTime(compiled);
    }
  }

  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  delay(200);
  //Serial.setDebugOutput(true);

  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(LEDs, NUM_LEDS);
  FastLED.setDither(false);
  FastLED.setCorrection(TypicalLEDStrip);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MILLI_AMPS);
  fill_solid(LEDs, NUM_LEDS, CRGB::Black);
  FastLED.show();

  // WiFi - AP Mode or both
#if defined(WIFIMODE) && (WIFIMODE == 0 || WIFIMODE == 2)
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(APssid, APpassword);    // IP is usually 192.168.4.1
  Serial.println();
  Serial.print("SoftAP IP: ");
  Serial.println(WiFi.softAPIP());
#endif

  // WiFi - Local network Mode or both
#if defined(WIFIMODE) && (WIFIMODE == 1 || WIFIMODE == 2)
  byte count = 0;
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    // Stop if cannot connect
    if (count >= 60) {
      Serial.println("Could not connect to local WiFi.");
      return;
    }

    delay(500);
    Serial.print(".");
    LEDs[count] = CRGB::Green;
    FastLED.show();
    count++;
  }
  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP());

  IPAddress ip = WiFi.localIP();
  Serial.println(ip[3]);
#endif

  httpUpdateServer.setup(&server);

  // Handlers
  server.on("/color", HTTP_POST, []() {
    r_val = server.arg("r").toInt();
    g_val = server.arg("g").toInt();
    b_val = server.arg("b").toInt();
    server.send(200, "text/json", "{\"result\":\"ok\"}");
  });

  server.on("/setdate", HTTP_POST, []() {
    // Sample input: date = "Dec 06 2009", time = "12:34:56"
    // Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec
    String datearg = server.arg("date");
    String timearg = server.arg("time");
    Serial.println(datearg);
    Serial.println(timearg);
    char d[12];
    char t[9];
    datearg.toCharArray(d, 12);
    timearg.toCharArray(t, 9);
    RtcDateTime compiled = RtcDateTime(d, t);
    Rtc.SetDateTime(compiled);
    clockMode = 0;
    server.send(200, "text/json", "{\"result\":\"ok\"}");
  });

  server.on("/brightness", HTTP_POST, []() {
    brightness = server.arg("brightness").toInt();
    server.send(200, "text/json", "{\"result\":\"ok\"}");
  });

  server.on("/countdown", HTTP_POST, []() {
    countdownMilliSeconds = server.arg("ms").toInt();
    byte cd_r_val = server.arg("r").toInt();
    byte cd_g_val = server.arg("g").toInt();
    byte cd_b_val = server.arg("b").toInt();
    digitalWrite(COUNTDOWN_OUTPUT, LOW);
    countdownColor = CRGB(cd_r_val, cd_g_val, cd_b_val);
    endCountDownMillis = millis() + countdownMilliSeconds;
    allBlank();
    clockMode = 1;
    server.send(200, "text/json", "{\"result\":\"ok\"}");
  });

  server.on("/temperature", HTTP_POST, []() {
    temperatureCorrection = server.arg("correction").toInt();
    temperatureSymbol = server.arg("symbol").toInt();
    clockMode = 2;
    server.send(200, "text/json", "{\"result\":\"ok\"}");
  });

  server.on("/scoreboard", HTTP_POST, []() {
    scoreboardLeft = server.arg("left").toInt();
    scoreboardRight = server.arg("right").toInt();
    scoreboardColorLeft = CRGB(server.arg("rl").toInt(), server.arg("gl").toInt(), server.arg("bl").toInt());
    scoreboardColorRight = CRGB(server.arg("rr").toInt(), server.arg("gr").toInt(), server.arg("br").toInt());
    clockMode = 3;
    server.send(200, "text/json", "{\"result\":\"ok\"}");
  });

  server.on("/hourformat", HTTP_POST, []() {
    hourFormat = server.arg("hourformat").toInt();
    clockMode = 0;
    server.send(200, "text/json", "{\"result\":\"ok\"}");
  });

  server.on("/clock", HTTP_POST, []() {
    clockMode = 0;
    server.send(200, "text/json", "{\"result\":\"ok\"}");
  });

  // Before uploading the files with the "ESP8266 Sketch Data Upload" tool, zip the files with the command "gzip -r ./data/" (on Windows I do this with a Git Bash)
  // *.gz files are automatically unpacked and served from your ESP (so you don't need to create a handler for each file).
  server.serveStatic("/", SPIFFS, "/", "max-age=86400");
  server.begin();

  SPIFFS.begin();
  Serial.println("SPIFFS contents:");
  Dir dir = SPIFFS.openDir("/");
  while (dir.next()) {
    String fileName = dir.fileName();
    size_t fileSize = dir.fileSize();
    Serial.printf("FS File: %s, size: %s\n", fileName.c_str(), String(fileSize).c_str());
  }
  Serial.println();

  digitalWrite(COUNTDOWN_OUTPUT, LOW);
}

void loop() {

  server.handleClient();

  unsigned long currentMillis = millis();
  if (currentMillis - prevTime >= 1000) {
    prevTime = currentMillis;

    if (clockMode == 0) {
      updateClock();
    } else if (clockMode == 1) {
      updateCountdown();
    } else if (clockMode == 2) {
      updateTemperature();
    } else if (clockMode == 3) {
      updateScoreboard();
    }

    FastLED.setBrightness(brightness);
    FastLED.show();
  }
}

void displayNumber(byte number, byte segment, CRGB color) {
  /*

      __ __ __        __ __ __          __ __ __        12 13 14
    __        __    __        __      __        __    11        15
    __        __    __        __      __        __    10        16
    __        __    __        __  42  __        __    _9        17
      __ __ __        __ __ __          __ __ __        20 19 18
    __        65    __        44  43  __        21    _8        _0
    __        __    __        __      __        __    _7        _1
    __        __    __        __      __        __    _6        _2
      __ __ __       __ __ __           __ __ __       _5 _4 _3

  */

  // segment from left to right: 3, 2, 1, 0
  byte startindex = 0;
  switch (segment) {
    case 0:
      startindex = 0;
      break;
    case 1:
      startindex = 7;
      break;
    case 2:
      startindex = 16;
      break;
    case 3:
      startindex = 23;
      break;
  }

  for (byte i = 0; i < 7; i++) {
  //  yield();
    LEDs[i + startindex] = ((numbers[number] & 1 << i) == 1 << i) ? color : alternateColor;
    //LEDs[i + startindex] = ((numbers[number] & 1 << i) == 1 << i) ? ColorFromPalette(palette, currentHue, brightness) : alternateColor;
    currentHue = currentHue + 1;
  }
  //Serial.print("(dN)currentHue: ");
  //Serial.println(currentHue);
  currentHue = currentHue - 1;


}

void displayNumberPallette(byte number, byte segment) {
  /*

      __ __ __        __ __ __          __ __ __        12 13 14
    __        __    __        __      __        __    11        15
    __        __    __        __      __        __    10        16
    __        __    __        __  42  __        __    _9        17
      __ __ __        __ __ __          __ __ __        20 19 18
    __        65    __        44  43  __        21    _8        _0
    __        __    __        __      __        __    _7        _1
    __        __    __        __      __        __    _6        _2
      __ __ __       __ __ __           __ __ __       _5 _4 _3

  */

  // segment from left to right: 3, 2, 1, 0
  byte startindex = 0;
  switch (segment) {
    case 0:
      startindex = 0;
      break;
    case 1:
      startindex = 7;
      break;
    case 2:
      startindex = 16;
      break;
    case 3:
      startindex = 23;
      break;
  }
CRGB color;
  for (byte i = 0; i < 7; i++) {
    //yield();

    if ((numbers[number] & 1 << i) == 1 << i) {
      color = ColorFromPalette(palette, currentHue, brightness) ;
      currentHue = currentHue + 1;
    } else {
      color = alternateColor;
    }
    // LEDs[i + startindex] = ((numbers[number] & 1 << i) == 1 << i) ? color : alternateColor;
    LEDs[i + startindex] = color;

  }

 // Serial.print("(dNP)currentHue: ");
 // Serial.println(currentHue);


}



void allBlank() {
  for (int i = 0; i < NUM_LEDS; i++) {
    LEDs[i] = CRGB::Black;
  }
  FastLED.show();
}

void updateClock() {
  RtcDateTime now = Rtc.GetDateTime();
  // printDateTime(now);
  Serial.println(now);

  int hour = now.Hour();
  int mins = now.Minute();
  int secs = now.Second();


  if (hourFormat == 12 && hour > 12)
    hour = hour - 12;

  byte h1 = hour / 10;
  byte h2 = hour % 10;
  byte m1 = mins / 10;
  byte m2 = mins % 10;
  byte s1 = secs / 10;
  byte s2 = secs % 10;

//  CRGB color = CRGB(r_val, g_val, b_val);

  if (h1 > 0)
    displayNumberPallette(h1, 3); //, color);
  else
    displayNumberPallette(10, 3); //, color); // Blank

  displayNumberPallette(h2, 2); //, color);
  displayNumberPallette(m1, 1); //, color);
  displayNumberPallette(m2, 0); //, color);
  if (currentM2 != m2) {
    baseHue = baseHue < 254 ? baseHue + 3 : 1;
    currentM2 = m2;
  }
  //Serial.print("(uC)currentHue: ");
  //Serial.println(currentHue);
   displayDots( ColorFromPalette(palette, currentHue, brightness) );
  currentHue = baseHue;
  //Serial.println(currentHue);

 
}

void updateCountdown() {

  if (countdownMilliSeconds == 0 && endCountDownMillis == 0)
    return;

  unsigned long restMillis = endCountDownMillis - millis();
  unsigned long hours   = ((restMillis / 1000) / 60) / 60;
  unsigned long minutes = (restMillis / 1000) / 60;
  unsigned long seconds = restMillis / 1000;
  int remSeconds = seconds - (minutes * 60);
  int remMinutes = minutes - (hours * 60);

  Serial.print(restMillis);
  Serial.print(" ");
  Serial.print(hours);
  Serial.print(" ");
  Serial.print(minutes);
  Serial.print(" ");
  Serial.print(seconds);
  Serial.print(" | ");
  Serial.print(remMinutes);
  Serial.print(" ");
  Serial.println(remSeconds);

  byte h1 = hours / 10;
  byte h2 = hours % 10;
  byte m1 = remMinutes / 10;
  byte m2 = remMinutes % 10;
  byte s1 = remSeconds / 10;
  byte s2 = remSeconds % 10;

  CRGB color = countdownColor;
  if (restMillis <= 60000) {
    color = CRGB::Red;
  }

  if (hours > 0) {
    // hh:mm
    displayNumber(h1, 3, color);
    displayNumber(h2, 2, color);
    displayNumber(m1, 1, color);
    displayNumber(m2, 0, color);
  } else {
    // mm:ss
    displayNumber(m1, 3, color);
    displayNumber(m2, 2, color);
    displayNumber(s1, 1, color);
    displayNumber(s2, 0, color);
  }
  currentHue = currentHue < 254 ? baseHue + 3 : 1;
  displayDots(color);

  if (hours <= 0 && remMinutes <= 0 && remSeconds <= 0) {
    Serial.println("Countdown timer ended.");
    //endCountdown();
    countdownMilliSeconds = 0;
    endCountDownMillis = 0;
    digitalWrite(COUNTDOWN_OUTPUT, HIGH);
    return;
  }
}

void endCountdown() {
  allBlank();
  for (int i = 0; i < NUM_LEDS; i++) {
    if (i > 0)
      LEDs[i - 1] = CRGB::Black;

    LEDs[i] = CRGB::Red;
    FastLED.show();
    delay(25);
  }
}

void displayDots(CRGB color) {
  if (dotsOn) {
    //LEDs[42] = ColorFromPalette(palette, currentHue, brightness); // color;
    //LEDs[43] = ColorFromPalette(palette, currentHue, brightness); // color;
      LEDs[14] =  color;
    LEDs[15] = color;
  } else {
    LEDs[14] = CRGB::Black;
    LEDs[15] = CRGB::Black;
  }

  dotsOn = !dotsOn;
}

void hideDots() {
  LEDs[14] = CRGB::Black;
  LEDs[15] = CRGB::Black;
}

void updateTemperature() {
  
  RtcTemperature temp = Rtc.GetTemperature();
  float ftemp = temp.AsFloatDegC();
  float ctemp = ftemp + temperatureCorrection;
  Serial.print("Sensor temp: ");
  Serial.print(ftemp);
  Serial.print(" Corrected: ");
  Serial.println(ctemp);

  if (temperatureSymbol == 13)
    ctemp = (ctemp * 1.8000) + 32;

  byte t1 = int(ctemp) / 10;
  byte t2 = int(ctemp) % 10;
  CRGB color =  ColorFromPalette(palette, currentHue, brightness); //CRGB(r_val, g_val, b_val);
 //   Serial.print("color: ");
//  Serial.println(color);
allBlank();
  delay(200);
  displayNumber(t1, 3, color);
  displayNumber(t2, 2, color);
  displayNumberPallette(11, 1); // , color);
  displayNumberPallette(temperatureSymbol, 0); // , color);
  currentHue = baseHue;
  //hideDots();
}

void updateScoreboard() {
  byte sl1 = scoreboardLeft / 10;
  byte sl2 = scoreboardLeft % 10;
  byte sr1 = scoreboardRight / 10;
  byte sr2 = scoreboardRight % 10;

  displayNumber(sl1, 3, scoreboardColorLeft);
  displayNumber(sl2, 2, scoreboardColorLeft);
  displayNumber(sr1, 1, scoreboardColorRight);
  displayNumber(sr2, 0, scoreboardColorRight);
  hideDots();
}

void printDateTime(const RtcDateTime& dt)
{
  char datestring[20];

  snprintf_P(datestring,
             countof(datestring),
             PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
             dt.Month(),
             dt.Day(),
             dt.Year(),
             dt.Hour(),
             dt.Minute(),
             dt.Second() );
  Serial.println(datestring);
}
