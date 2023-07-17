#include <FastLED.h>
#include <DS3231.h>
#include <ErriezDS1302.h>
#include <Wire.h>

#define NUM_LEDS            30
#define DATA_PIN            4

#define DS1302_CLK_PIN      7
#define DS1302_IO_PIN       8
#define DS1302_CE_PIN       9
#define BUZZER              5
CRGB leds[NUM_LEDS];
CRGBPalette16 palette = RainbowColors_p; 
//CRGBPalette16 palette = PartyColors_p;
//CRGBPalette16 palette = ForestColors_p;
//CRGBPalette16 palette = CloudColors_p;
//CRGBPalette16 palette = HeatColors_p;
ErriezDS1302 rtc = ErriezDS1302(DS1302_CLK_PIN, DS1302_IO_PIN, DS1302_CE_PIN);

const int TOTAL_SEGMENTS = 4; // Total amount of segments
const int LEDS_PER_SEGMENT = 7; // Amount of LEDs per segment
const int DISPLAY_SEGMENT[] = {0, 7, 7 * 2 + 2, 7 * 3 + 2}; // LED starting position of each segment
const int DISPLAY_NUMBER[][7] = { // True: Lit, False:  Not lit
  {true, false, true, true, true, true, true},      // 0
  {false, false, false, false, true, true, false},  // 1
  {true, true, false, true, true, false, true},     // 2
  {false, true, false, true, true, true, true},     // 3
  {false, true, true, false, true, true, false},    // 4
  {false, true, true, true, false, true, true},     // 5
  {true, true, true, true, false, true, true},      // 6
  {false, false, false, true, true, true, false},   // 7
  {true, true, true, true, true, true, true},       // 8
  {false, true, true, true, true, true, true},      // 9
};

int red = 255;
int green = 8;
int blue = 8;
uint8_t brightness = 100;
char currentFade = 'r';
uint8_t minuteSecondDigitSet = 10;
boolean  even = false;
uint8_t gHue = 1; // rotating "base color" used by many of the patterns
uint8_t currentHue = 1;
uint8_t secHue = 1;
void setup()
{
  pinMode(BUZZER, INPUT);
  // Initialize serial port
  delay(500);
  Serial.begin(115200);

  Serial.println(F("\nErriez DS1302 + fastpixel 7 segment clock"));

  // Initialize I2C
  Wire.begin();
  Wire.setClock(100000);

  // Initialize RTC
  while (!rtc.begin()) {
    Serial.println(F("RTC not found"));
    delay(3000);
  }

  // Set new time 12:11:00
    if (!rtc.setTime(14, 38, 00)) {
      Serial.println(F("Set time failed"));
   }
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  FastLED.setBrightness(brightness); // Lower brightness

}

void loop()
{

  uint8_t hour;
  uint8_t min;
  uint8_t sec;
  //first time will always refresh

  // Get time from RTC
  if (!rtc.getTime(&hour, &min, &sec)) {
    Serial.println(F("Get time failed"));
  } else {

    //        // Print time
    //        Serial.print(hour);
    //        Serial.print(F(":"));
    //        if (min < 10) {
    //          Serial.print(F("0"));
    //        }
    //        Serial.print(min);
    //        Serial.print(F(":"));
    //        if (sec < 10) {
    //          Serial.print(F("0"));
    //        }
    //        Serial.println(sec);
  }




  uint8_t hourFirstDigit = hour / 10; // Take the first digit
  uint8_t hourSecondDigit = hour % 10; // Take the second digit

  uint8_t minuteFirstDigit = min / 10; // Take the first digit1
  uint8_t minuteSecondDigit = min % 10; // Take the second digit
  //  Serial.print(hourFirstDigit);
  //  Serial.print(hourSecondDigit);
  //  Serial.print(minuteFirstDigit);
  //  Serial.print(minuteSecondDigit);
  //  Serial.print("-");
  //  Serial.println(minuteSecondDigitSet);
  //  Serial.println(even);
  if (even) {
    leds[14].setRGB(secHue * 4, secHue * 3, secHue * 2);
    leds[15] = ColorFromPalette(palette, gHue + 20, 255);
    FastLED.show();
    even = !even;

  } else {
    leds[15].setRGB(secHue * 4, secHue * 3, secHue * 2);
    leds[14] = ColorFromPalette(palette, gHue + 20, 90);
    FastLED.show();
    even = !even;
  }
  secHue++;

  if (minuteSecondDigitSet != minuteSecondDigit) {
    if (gHue < 255) gHue++; else gHue = 2;
    currentHue = gHue;
    Serial.println("+");
    FastLED.clear(); // Clear the LEDs
    displayNumber(3, hourFirstDigit);
    displayNumber(2, hourSecondDigit);
    displayNumber(1, minuteFirstDigit);
    displayNumber(0, minuteSecondDigit);
    leds[14].setRGB(red, green, blue); // Light the dots
    leds[14 + 1].setRGB(red, green, blue);
    FastLED.show(); // Show the current LEDs
    minuteSecondDigitSet = minuteSecondDigit;
    secHue = 1;
    if (min == 0) {
      pinMode(BUZZER, OUTPUT);
      tone(BUZZER, 2500);
      delay(200);
      noTone(BUZZER);
      delay(75);
      tone(BUZZER, 2500);
      delay(200);
      noTone(BUZZER);
      pinMode(BUZZER, INPUT);
      // Wait a second
      delay(990);
    }


  }

  //  tone(BUZZER, 500);
  //  delay(15);
  //  pinMode(BUZZER, OUTPUT);
  //  tone(BUZZER, 10000);
  //  delay(10);
  //  noTone(BUZZER);
  //  pinMode(BUZZER, INPUT);
  //  // Wait a second
  delay(990);
}

void displayNumber(int segment, int number) {

  Serial.print("number ");  Serial.print(number); Serial.print(" on segment: "); Serial.print(segment);
  Serial.println();
  for (int j = 0; j < LEDS_PER_SEGMENT; j++) { // Loop over each LED of said segment
    if (DISPLAY_NUMBER[number][j]) { // If this LED should be on
      //      leds[DISPLAY_SEGMENT[segment] + j].setRGB(red, green, blue); // Turn it on
      leds[DISPLAY_SEGMENT[segment] + j] = ColorFromPalette(palette, currentHue, brightness);
      Serial.print(DISPLAY_SEGMENT[segment] + j); Serial.print(",");
      Serial.print("currHue: ");
      Serial.print(currentHue);
      Serial.println();
      currentHue = currentHue + 3;

    }

  }
  Serial.println();
  Serial.println("hue: ");
  Serial.println(gHue);
}

void bpm()
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for ( int i = 0; i < NUM_LEDS; i++) { //9948
    leds[i] = ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 10));
  }
}
