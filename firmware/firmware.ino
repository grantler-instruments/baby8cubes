// !!!! select usb type: serial + audio + midi

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_VL53L0X.h>
#include <Adafruit_MPU6050.h>
#include <CD74HC4067.h>  //https://github.com/waspinator/CD74HC4067
#include "Parameter.h"
#include "config.h"
#include "testVoice.h"

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
int _distance = 0;

int hallValues[NUMSTEPS * 4];


Voice _voices[NUMVOICES];
int _currentVoice = 0;
AudioMixer4 firstMixer;
AudioMixer4 secondMixer;
AudioMixer4 mainMixer;
AudioOutputI2S headphones;
AudioOutputAnalog dac;

AudioConnection c0(_voices[0]._playMem, 0, firstMixer, 0);
AudioConnection c1(_voices[1]._playMem, 0, firstMixer, 1);
AudioConnection c2(_voices[2]._playMem, 0, firstMixer, 2);
AudioConnection c3(_voices[3]._playMem, 0, firstMixer, 3);

AudioConnection c4(_voices[4]._playMem, 0, secondMixer, 0);
AudioConnection c5(_voices[5]._playMem, 0, secondMixer, 1);
AudioConnection c6(_voices[6]._playMem, 0, secondMixer, 2);
AudioConnection c7(_voices[7]._playMem, 0, secondMixer, 3);

AudioConnection c8(firstMixer, 0, mainMixer, 0);
AudioConnection c9(secondMixer, 0, mainMixer, 1);

AudioConnection c10(mainMixer, 0, headphones, 0);
AudioConnection c11(mainMixer, 0, headphones, 1);
AudioConnection c12(mainMixer, 0, dac, 0);

AudioControlSGTL5000 audioShield;


Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMSTEPS, NEOPIXEL_PIN, NEO_RGB + NEO_KHZ800);
Adafruit_VL53L0X lox = Adafruit_VL53L0X();
Adafruit_MPU6050 mpu;
unsigned long _timestamp = 0;


ParameterGroup _parameters;
Parameter<bool> _on;



#define BUFFER_SIZE 10  // Number of readings to store in history
int distanceBuffer[BUFFER_SIZE];
int bufferIndex = 0;
bool isJumpy = false;

void updateDistanceBuffer(int newValue) {
  distanceBuffer[bufferIndex] = newValue;
  bufferIndex = (bufferIndex + 1) % BUFFER_SIZE;  // Circular buffer
}

float calculateVariance() {
  float mean = 0;
  for (int i = 0; i < BUFFER_SIZE; i++) {
    mean += distanceBuffer[i];
  }
  mean /= BUFFER_SIZE;

  float variance = 0;
  for (int i = 0; i < BUFFER_SIZE; i++) {
    float diff = distanceBuffer[i] - mean;
    variance += diff * diff;
  }
  return variance / BUFFER_SIZE;
}

void checkJumpy() {
  float variance = calculateVariance();
  // Define threshold based on your setup's noise level
  isJumpy = variance > 30.0;  // Example threshold
}


void audioTest() {
  _voices[0].noteOn(60);
  delay(250);
}
void neoPixelTest() {
  Serial.print("started neopixel test");
  for (auto i = 0; i < NUMSTEPS; i++) {
    for (auto j = 0; j < 3; j++) {
      strip.setPixelColor(i, strip.Color(j == 0 ? 255 : 0, j == 1 ? 255 : 0, j == 2 ? 255 : 0));
      strip.show();
      delay(100);
    }
    delay(100);
  }
  strip.clear();
  strip.show();
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
  _ledMux.channel(index);
}
void hideStepLed() {
  digitalWrite(LED_SIG_PIN, LOW);
}
void renderStepAndIncrement() {
  showStepLed(_step);
  _step = (_step + 1) % NUMSTEPS;
  _timestamp = millis();
}
void readSensors() {
  auto potiValue = analogRead(POTI_PIN);
  bool buttonValue = digitalRead(BUTTON_PIN);
  _on = !buttonValue;

  _bpm = map(potiValue, 0, 1024, 600, 20);
  for (auto i = 0; i < 16; i++) {
    _hallAMux.channel(i);
    _hallBMux.channel(i);
    delay(2);
    hallValues[i] = analogRead(HALL_A_SIG_PIN);
    hallValues[i + 16] = analogRead(HALL_B_SIG_PIN);
  }

  // _hallAMux.channel(1);
  // delay(100);
  // Serial.println(analogRead(HALL_A_SIG_PIN));

  // for (auto i = 0; i < NUMSTEPS * 4; i++) {
  //   Serial.print(hallValues[i]);
  //   Serial.print(",");
  // }
  // Serial.println();
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

void onNoteOn(byte channel, byte note, byte velocity) {
  Serial.println("got note on");
  _currentVoice = (_currentVoice + 1) % NUMVOICES;
  _voices[_currentVoice].noteOn(note);
}
void onNoteOff(byte channel, byte note, byte velocity) {
}

void setup() {
  Serial.begin(115200);

  pinMode(LED_SIG_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  for (auto i = 0; i < NUMSTEPS * 4; i++) {
    hallValues[i] = 0;
  }

  _on.setup("on", false);

  // midi
  Serial.println("setup midi engine");
  usbMIDI.setHandleNoteOff(onNoteOff);
  usbMIDI.setHandleNoteOn(onNoteOn);

  // audio
  Serial.println("setup audio engine");

  AudioMemory(128);
  audioShield.enable();
  audioShield.volume(0.5);

  for (auto i = 0; i < 4; i++) {
    firstMixer.gain(i, 0.4);
    secondMixer.gain(i, 0.4);
  }


  mainMixer.gain(0, 0.5);
  mainMixer.gain(1, 0.5);
  // audioTest();


  // led
  // strip.begin();
  // strip.show();

  // set led high
  digitalWrite(LED_SIG_PIN, HIGH);

  Serial.println("setup distance sensor");
  if (!lox.begin()) {
    Serial.println(F("Failed to boot VL53L0X"));
  }

  delay(1000);
  Serial.println("setup motion sensor");
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);

  // neoPixelTest();
}

void loop() {
  auto timestamp = millis();
  usbMIDI.read();
  // Serial.println("loop");

  return;

  // distance for velocity modulation
  VL53L0X_RangingMeasurementData_t measure;
  lox.rangingTest(&measure, false);
  if (measure.RangeStatus != 4) {


    updateDistanceBuffer(measure.RangeMilliMeter);
    checkJumpy();

    if (isJumpy) {
      // Serial.println("Distance measurements are jumpy!");
    } else {
      if (measure.RangeMilliMeter > 100) {
        _velocity = 127;
      } else {
        _velocity = 127 - map(measure.RangeMilliMeter, 0, 100, 127, 0);
      }
      Serial.println(_velocity);
    }
  }


  // gyroscope for speed modulation
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  // Serial.print(g.orientation.x);
  // Serial.print(", ");
  // Serial.print(g.orientation.y);
  // Serial.print(", ");
  // Serial.println(g.orientation.z);

  // TODO: map gyro value to speed modulation



  readSensors();
  String message = "";
  message = String(hallValues[0]) + ", " + String(hallValues[1]) += ", " + String(hallValues[2]) + ", " + String(hallValues[3]);
  // Serial.println(hallValues[5]);

  // printHallValues();
  if (_on) {
    if (timestamp - _timestamp > (60.0 * 1000 / _bpm)) {
      renderStepAndIncrement();
    }
  }
}
