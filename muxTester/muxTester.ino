#define NUMSTEPS 8

#define HALL_A_SELECT_0_PIN 28
#define HALL_A_SELECT_1_PIN 29
#define HALL_A_SELECT_2_PIN 30
#define HALL_A_SELECT_3_PIN 31

#define HALL_B_SELECT_0_PIN 32
#define HALL_B_SELECT_1_PIN 33
#define HALL_B_SELECT_2_PIN 34
#define HALL_B_SELECT_3_PIN 35

#define HALL_A_SIG_PIN 3  //A9
#define HALL_B_SIG_PIN 2  //A8
#define NEOPIXEL_PIN 39


#include <Adafruit_NeoPixel.h>


#include <CD74HC4067.h>  //https://github.com/waspinator/CD74HC4067
CD74HC4067 _hallAMux(HALL_A_SELECT_0_PIN, HALL_A_SELECT_1_PIN, HALL_A_SELECT_2_PIN, HALL_A_SELECT_3_PIN);
CD74HC4067 _hallBMux(HALL_B_SELECT_0_PIN, HALL_B_SELECT_1_PIN, HALL_B_SELECT_2_PIN, HALL_B_SELECT_3_PIN);

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMSTEPS, NEOPIXEL_PIN, NEO_RGB + NEO_KHZ800);

void neoPixelTest() {
  Serial.print("started neopixel test");
  for (auto i = 0; i < NUMSTEPS; i++) {
    strip.clear();
    strip.setPixelColor(i, strip.Color(255, 0, 0));
    strip.show();
    delay(500);
    strip.clear();
    strip.setPixelColor(i, strip.Color(0, 255, 0));
    strip.show();
    delay(500);
    strip.clear();
    strip.setPixelColor(i, strip.Color(0, 0, 255));
    strip.show();
    delay(500);
    strip.clear();
  }
  // strip.setPixelColor(0, strip.Color(255, 0, 0));
  strip.show();
  delay(100);
  Serial.println(": done");
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  strip.begin();
  strip.setBrightness(255);  // Set BRIGHTNESS to about 1/5 (max = 255)
  strip.clear();
  strip.show();

  neoPixelTest();
}

void loop() {
  // put your main code here, to run repeatedly:
  String message = "";
  bool values[NUMSTEPS * 4];
  for (auto i = 0; i < 4 * 4; i++) {
    _hallAMux.channel(i);
    _hallBMux.channel(i);
    values[i] = analogRead(HALL_A_SIG_PIN) < 64 ? 1 : 0;
    values[16 + i] = analogRead(HALL_B_SIG_PIN) < 64 ? 1 : 0;
  }

  String firstRow = "";
  for (auto i = 0; i < NUMSTEPS; i++) {
    firstRow += " ";
    firstRow += String(values[i * 4]);
    firstRow += " ";
    firstRow += " ";
    firstRow += " ";
    firstRow += " ";
    firstRow += " ";
    firstRow += " ";
  }

  String secondRow = "";
  for (auto i = 0; i < 8; i++) {
    secondRow += String(values[i * 4 + 3]);
    secondRow += " ";
    secondRow += String(values[i * 4 + 1]);
    secondRow += " ";
    secondRow += " ";
    secondRow += " ";
    secondRow += " ";
    secondRow += " ";
  }

  String thirdRow = "";
  for (auto i = 0; i < 8; i++) {
    thirdRow += " ";
    thirdRow += String(values[i * 4 + 2]);
    thirdRow += " ";
    thirdRow += " ";
    thirdRow += " ";
    thirdRow += " ";
    thirdRow += " ";
    thirdRow += " ";
  }


  Serial.println(firstRow);
  Serial.println(secondRow);
  Serial.println(thirdRow);
  Serial.println();

  strip.clear();
  for (auto i = 0; i < 1; i++) {
    if (values[i * NUMSTEPS]) {
      strip.setPixelColor(i, strip.Color(255, 0, 0));
    } else if (values[i * NUMSTEPS + 1]) {
      strip.setPixelColor(i, strip.Color(0, 255, 0));
    } else if (values[i * NUMSTEPS + 2]) {
      strip.setPixelColor(i, strip.Color(0, 0, 255));
    } else if (values[i * NUMSTEPS + 3]) {
      strip.setPixelColor(i, strip.Color(0, 255, 255));
    } else {
      strip.setPixelColor(i, strip.Color(0, 0, 0));
    }
  }
  strip.show();
}
