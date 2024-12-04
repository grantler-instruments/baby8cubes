// !!!! select usb type: serial + audio + midi


// TODO: seesaw mode

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
#include "voice.h"

// http://www.freesound.org/people/KEVOY/sounds/82583/
#include "AudioSampleSnare.h"
// http://www.freesound.org/people/zgump/sounds/86334/
#include "AudioSampleTomtom.h"
// http://www.freesound.org/people/mhc/sounds/102790/
#include "AudioSampleHihat.h"
// http://www.freesound.org/people/DWSD/sounds/171104/
#include "AudioSampleKick.h"


CD74HC4067 _ledMux(LED_SELECT_0_PIN, LED_SELECT_1_PIN, LED_SELECT_2_PIN, LED_SELECT_3_PIN);
CD74HC4067 _hallAMux(HALL_A_SELECT_0_PIN, HALL_A_SELECT_1_PIN, HALL_A_SELECT_2_PIN, HALL_A_SELECT_3_PIN);
CD74HC4067 _hallBMux(HALL_B_SELECT_0_PIN, HALL_B_SELECT_1_PIN, HALL_B_SELECT_2_PIN, HALL_B_SELECT_3_PIN);


int _bpm = 120;
int _bpmModulator = 0;
int _step = 0;
int _velocity = 127;
int _distance = 0;

int hallValues[NUMSTEPS * 4];


Voice _voices[NUMVOICES];
int _currentVoice = 0;

AudioControlSGTL5000 _audioShield;
AudioOutputUSB _usbOutput;

AudioMixer4 _firstMixer;
AudioMixer4 _secondMixer;
AudioMixer4 _mainMixer;
AudioAmplifier _mainAmp;
AudioOutputI2S _headphones;
AudioOutputAnalog _dac;
AudioEffectBitcrusher _bitcrusher;
AudioFilterBiquad _filter;
AudioEffectFreeverb _freeverb;
AudioEffectReverb _reverb;


AudioConnection c0(_voices[0]._amp, 0, _firstMixer, 0);
AudioConnection c1(_voices[1]._amp, 0, _firstMixer, 1);
AudioConnection c2(_voices[2]._amp, 0, _firstMixer, 2);
AudioConnection c3(_voices[3]._amp, 0, _firstMixer, 3);

AudioConnection c4(_voices[4]._amp, 0, _secondMixer, 0);
AudioConnection c5(_voices[5]._amp, 0, _secondMixer, 1);
AudioConnection c6(_voices[6]._amp, 0, _secondMixer, 2);
AudioConnection c7(_voices[7]._amp, 0, _secondMixer, 3);

AudioConnection c8(_firstMixer, 0, _mainMixer, 0);
AudioConnection c9(_secondMixer, 0, _mainMixer, 1);

AudioConnection c10(_mainMixer, 0, _filter, 0);
AudioConnection c11(_filter, _bitcrusher);
AudioConnection c12(_bitcrusher, _mainAmp);

AudioConnection c13(_mainAmp, 0, _headphones, 0);
AudioConnection c14(_mainAmp, 0, _headphones, 1);
AudioConnection c15(_mainAmp, 0, _dac, 0);

AudioConnection _mainAmpToUsbL(_mainAmp, 0, _usbOutput, 0);  // left channel
AudioConnection _mainAmpToUsbR(_mainAmp, 0, _usbOutput, 1);  // right channel


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
void showStepColor(int index, int red, int green, int blue) {
  strip.clear();
  strip.setPixelColor(index, strip.Color(red, green, blue));
  strip.show();
}
void hideStepLed() {
  digitalWrite(LED_SIG_PIN, LOW);
}
void renderStepAndIncrement() {
  showStepLed(_step);
  showStepColor(_step, 255, 0, 0);
  onNoteOn(1, 60, 127);
  _mainAmp.gain(((float)(_velocity)) / 127);
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
  _currentVoice = (_currentVoice + 1) % NUMVOICES;
  _voices[_currentVoice].noteOn(note);
}
void onNoteOff(byte channel, byte note, byte velocity) {
}
void onControlChange(byte channel, byte controller, byte value) {
  switch (controller) {
    case 1:
      {
        _filter.setLowpass(0, map(value, 0, 127, 100, 20000), 0.5);
        break;
      }
    case 2:
      {
        _bitcrusher.bits(map(value, 0, 127, 0, 16));
        break;
      }
  }
}

void setup() {
  Serial.begin(115200);
  Wire.setClock(400000);  // Set I2C to 400 kHz

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
  usbMIDI.setHandleControlChange(onControlChange);

  // audio
  Serial.println("setup audio engine");

  AudioMemory(128);
  _audioShield.enable();
  _audioShield.volume(1);

  for (auto i = 0; i < 4; i++) {
    _firstMixer.gain(i, 0.4);
    _secondMixer.gain(i, 0.4);
  }


  _filter.setLowpass(0, 20000, 0.5);
  _bitcrusher.bits(12);
  _bitcrusher.sampleRate(44100);
  _mainMixer.gain(0, 0.5);
  _mainMixer.gain(1, 0.5);
  _mainAmp.gain(1);
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

  neoPixelTest();
}

void loop() {
  auto timestamp = millis();
  usbMIDI.read();
  // Serial.println("loop");

  // distance for velocity modulation
  VL53L0X_RangingMeasurementData_t measure;
  lox.rangingTest(&measure, false);
  if (measure.RangeStatus != 4) {
    updateDistanceBuffer(measure.RangeMilliMeter);
    if (measure.RangeMilliMeter > 100) {
      _velocity = 127;
    } else {
      _velocity = 127 - map(measure.RangeMilliMeter, 30, 100, 127, 0);
    }
    Serial.println(_velocity);
    //   if (isJumpy) {
    //     // Serial.println("Distance measurements are jumpy!");
    //   } else {

    //     Serial.println(_velocity);
    //   }
  }


  // gyroscope for speed modulation
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  // Serial.print(g.orientation.x);
  // Serial.print(", ");
  // Serial.print(g.orientation.y);
  // Serial.print(", ");
  // Serial.println(g.orientation.z);

  // Serial.print(a.acceleration.x);
  // Serial.print(", ");
  // Serial.print(a.acceleration.y);
  // Serial.print(", ");
  // Serial.println(a.acceleration.z);

  if (a.acceleration.x < 0.5) {
    _filter.setLowpass(0, 22000 - map(a.acceleration.x, 0.5, -5, 0, 21900));
    _freeverb.roomsize(map(a.acceleration.x, 0.5, -5, 0, 1));
    _freeverb.damping(map(a.acceleration.x, 0.5, -5, 0, 1));
    _reverb.reverbTime(map(a.acceleration.x, 0.5, -5, 0, 5));
  } else {
    _filter.setLowpass(0, 22000, 0.5);
    _freeverb.roomsize(0);
    _freeverb.damping(0);
    _reverb.reverbTime(0);
  }

  if (a.acceleration.x > 0.5) {
    _bitcrusher.bits(16 - (int)(map(a.acceleration.x, 0.5, 5, 0, 16)));
    _bitcrusher.sampleRate(44100 - (int)(map(a.acceleration.x, 0.5, 5, 0, 44100)));
  } else {
    _bitcrusher.bits(16);
    _bitcrusher.sampleRate(44100);
  }

  if (a.acceleration.x < 0.5) {
    _filter.setLowpass(0, 5000 - map(a.acceleration.x, 0.5, -5, 0, 5000));
  } else {
    _filter.setLowpass(0, 22000, 0.5);
  }

  if (a.acceleration.y < -0.7) {
    _bpmModulator = -map(a.acceleration.y, -0.7, -3, 0, 80);
  } else {
    _bpmModulator = map(a.acceleration.y, 0.7, 3, 0, 80);
  }

  readSensors();
  String message = "";
  message = String(hallValues[0]) + ", " + String(hallValues[1]) += ", " + String(hallValues[2]) + ", " + String(hallValues[3]);
  // Serial.println(hallValues[5]);

  // printHallValues();
  if (_on) {
    if (timestamp - _timestamp > (60.0 * 1000 / (_bpm + _bpmModulator))) {
      renderStepAndIncrement();
    }
  }
}
