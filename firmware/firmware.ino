#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_VL53L0X.h>
#include <CD74HC4067.h>  //https://github.com/waspinator/CD74HC4067
#include "Parameter.h"
#include "config.h"

#include "AudioSampleSnare.h"   // http://www.freesound.org/people/KEVOY/sounds/82583/
#include "AudioSampleTomtom.h"  // http://www.freesound.org/people/zgump/sounds/86334/
#include "AudioSampleHihat.h"   // http://www.freesound.org/people/mhc/sounds/102790/
#include "AudioSampleKick.h"    // http://www.freesound.org/people/DWSD/sounds/171104/


CD74HC4067 _ledMux(LED_SELECT_0_PIN, LED_SELECT_1_PIN, LED_SELECT_2_PIN, LED_SELECT_3_PIN);
CD74HC4067 _hallAMux(HALL_A_SELECT_0_PIN, HALL_A_SELECT_1_PIN, HALL_A_SELECT_2_PIN, HALL_A_SELECT_3_PIN);
CD74HC4067 _hallBMux(HALL_B_SELECT_0_PIN, HALL_B_SELECT_1_PIN, HALL_B_SELECT_2_PIN, HALL_B_SELECT_3_PIN);


int _bpm = 120;
int _step = 0;
int _velocity = 127;

int hallValues[NUMSTEPS * 4];


AudioPlayMemory sounds[NUMSTEPS];
//AudioPlayMemory snare;
//AudioPlayMemory hihat;
//AudioPlayMemory tomtom;
AudioMixer4 firstMixer;
AudioMixer4 secondMixer;
AudioMixer4 mainMixer;

AudioOutputI2S headphones;
AudioOutputAnalog dac;

AudioConnection c0(sounds[0], 0, firstMixer, 0);
AudioConnection c1(sounds[1], 0, firstMixer, 1);
AudioConnection c2(sounds[2], 0, firstMixer, 2);
AudioConnection c3(sounds[3], 0, firstMixer, 3);

AudioConnection c4(sounds[4], 0, secondMixer, 0);
AudioConnection c5(sounds[5], 0, secondMixer, 1);
AudioConnection c6(sounds[6], 0, secondMixer, 2);
AudioConnection c7(sounds[7], 0, secondMixer, 3);

AudioConnection c8(firstMixer, 0, mainMixer, 0);
AudioConnection c9(secondMixer, 0, mainMixer, 1);

AudioConnection c10(mainMixer, 0, headphones, 0);
AudioConnection c11(mainMixer, 0, headphones, 1);
AudioConnection c12(mainMixer, 0, dac, 0);

AudioControlSGTL5000 audioShield;


Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMSTEPS, NEOPIXEL_PIN, NEO_RGB + NEO_KHZ800);
unsigned long _timestamp = 0;

Adafruit_VL53L0X lox = Adafruit_VL53L0X();


ParameterGroup _parameters;
Parameter<bool> _on;

void audioTest() {
  sounds[0].play(AudioSampleKick);
  delay(250);

  sounds[1].play(AudioSampleHihat);
  delay(250);

  sounds[2].play(AudioSampleSnare);
  delay(250);

  sounds[3].play(AudioSampleHihat);
  delay(250);

  sounds[4].play(AudioSampleKick);
  delay(250);

  sounds[5].play(AudioSampleHihat);
  delay(250);

  sounds[6].play(AudioSampleSnare);
  delay(250);

  sounds[7].play(AudioSampleHihat);
  delay(250);
}
void neoPixelTest() {
  Serial.print("started neopixel test");
  for (auto i = 0; i < NUMSTEPS; i++) {
    for (auto j = 0; j < 3; j++) {
      strip.setPixelColor(i, strip.Color(j == 0 ? 255 : 0, j == 1 ? 255 : 0, j == 2 ? 255 : 0));
      strip.show();
      delay(500);
    }
    delay(500);
  }
  strip.clear();
  delay(100);
  Serial.println(": done");
}

void ledTest() {
  Serial.print("started led test");
  for (auto i = 0; i < NUMSTEPS; i++) {
    showStepLed(i);
    delay(1000);
  }
  Serial.println(": done");
}

void showStepLed(int index) {
  Serial.print("show led: ");
  Serial.println(index);
  _ledMux.channel(index);
}
void hideStepLed() {
  digitalWrite(LED_SIG_PIN, LOW);
}
void renderStepAndIncrement() {
  Serial.print("rendering step ");
  Serial.println(_step);
  showStepLed(_step);
  strip.clear();
  strip.setPixelColor(_step, strip.Color(0, 255, 255));
  strip.show();
  sounds[_step].play(AudioSampleKick);


  _step = (_step + 1) % NUMSTEPS;
  _timestamp = millis();
}
void readSensors() {
  auto potiValue = analogRead(POTI_PIN);
  bool buttonValue = digitalRead(BUTTON_PIN);
  _on = !buttonValue;

  //  Serial.print("poti: ");
  //  Serial.print(potiValue);
  //  Serial.print(", button: ");
  //  Serial.println(buttonValue);

  _bpm = map(potiValue, 0, 1024, 600, 20);
  for (auto i = 0; i < 16; i++) {
    _hallAMux.channel(i);
    _hallBMux.channel(i);
    delay(2);
    hallValues[i] = analogRead(HALL_A_SIG_PIN);
    hallValues[i + 16] = analogRead(HALL_B_SIG_PIN);
  }

  //  for (auto i = 0; i < NUMSTEPS * 4; i++) {
  //    Serial.print(hallValues[i]);
  //    Serial.print(",");
  //  }
  //  Serial.println();
}
void printHallValues() {
  for (auto i = 0; i < 32; i++) {
    Serial.print(hallValues[i]);
    if (i < 31) {
      Serial.print(", ");
    } else {
      Serial.println();
    }
  }
}
void setup() {
  Serial.begin(115200);

  pinMode(LED_SIG_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  for (auto i = 0; i < NUMSTEPS * 4; i++) {
    hallValues[i] = 0;
  }



  _on.setup("on", false);

  // audio
  AudioMemory(10);
  audioShield.enable();
  audioShield.volume(0.5);

  for (auto i = 0; i < 4; i++) {
    firstMixer.gain(i, 0.4);
    secondMixer.gain(i, 0.4);
  }


  mainMixer.gain(0, 0.5);
  mainMixer.gain(1, 0.5);

  // led
  strip.begin();
  strip.show();

  // sert led high
  digitalWrite(LED_SIG_PIN, HIGH);


  if (!lox.begin()) {
    Serial.println(F("Failed to boot VL53L0X"));
  }
}

void loop() {
  auto timestamp = millis();

  //  ledTest();
  //  neoPixelTest();
  //  audioTest();

  VL53L0X_RangingMeasurementData_t measure;
  lox.rangingTest(&measure, false);  // pass in 'true' to get debug data printout!
  // TODO: map measurement to velocity

  if (measure.RangeStatus != 4) {  // phase failures have incorrect data
    Serial.print("Distance (mm): ");
    Serial.println(measure.RangeMilliMeter);
  } else {
    Serial.println(" out of range ");
  }

  readSensors();
  printHallValues();
  if (_on) {
    if (timestamp - _timestamp > (60.0 * 1000 / _bpm)) {
      renderStepAndIncrement();
    }
  }



  //  auto input = analogRead(pins[step]);
  //  if (input < 128) { // cube not placed
  //    strip.setPixelColor(step, strip.Color(255, 255, 255));
  //  } else if (input < 256) { // cube rotated to kick
  //    strip.setPixelColor(step, strip.Color(255, 0, 0));
  //    sounds[step].play(AudioSampleKick);
  //  } else if (input < 512) { // cube rotated to snare
  //    strip.setPixelColor(step, strip.Color(0, 255, 0));
  //    sounds[step].play(AudioSampleSnare);
  //  } else if (input < 768) { // cube rotated to hihats
  //    strip.setPixelColor(step, strip.Color(0, 0, 255));
  //    sounds[step].play(AudioSampleHihat);
  //  } else {// cube rotated to clap
  //    strip.setPixelColor(step, strip.Color(255, 0, 255));
  //    sounds[step].play(AudioSampleTomtom);
  //  }
  //  strip.show();


  // move to the next step and wait for the next tick
  //  delay();
}
