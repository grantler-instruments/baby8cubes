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
#include <Parameter.h>
#include "config.h"
#include "helpers.h"
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


int _bpmModulator = 0;
int _volume = 127;
int _distance = 0;


float _timeStep = 0.1;



bool _hallValues[NUMSTEPS * NUMCORNERS];
bool _lastHallValues[NUMSTEPS * NUMCORNERS];


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
AudioEffectDelay _delay;
AudioMixer4 _delayMixer;


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
AudioConnection c16(_mainAmp, 0, _dac, 1);

AudioConnection _mainAmpToUsbL(_mainAmp, 0, _usbOutput, 0);  // left channel
AudioConnection _mainAmpToUsbR(_mainAmp, 0, _usbOutput, 1);  // right channel

// AudioConnection bitcrusherToDelay(_bitcrusher, _delay);
// AudioConnection delay0ToMixer(_delay, 0, _delayMixer, 0);
// AudioConnection delay1ToMixer(_delay, 1, _delayMixer, 1);
// AudioConnection delay2ToMixer(_delay, 2, _delayMixer, 2);
// AudioConnection delay3ToMixer(_delay, 3, _delayMixer, 3);
// TODO: feedback some signal to the delay line, add a controllable amp for that


Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMSTEPS, NEOPIXEL_PIN, NEO_RGB + NEO_KHZ800);
Adafruit_VL53L0X lox = Adafruit_VL53L0X();
Adafruit_MPU6050 mpu;
unsigned long _timestamp = 0;
unsigned long _lastChangeTimestamp = 0;
int _lastChangeIndex = -1;

ParameterGroup _parameters;
Parameter<bool> _on;
Parameter<bool> _seasawMode;
Parameter<int> _bpm;

Parameter<int> _position;
Parameter<float> _force;
Parameter<float> _velocity;
Parameter<float> _dampeningFactor;



VL53L0X_RangingMeasurementData_t _measure;
sensors_event_t _a, _g, _temp;


uint32_t _colors[NUMCORNERS] = { strip.Color(255, 0, 0), strip.Color(0, 255, 0), strip.Color(0, 0, 255), strip.Color(255, 0, 255) };






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
    strip.clear();
    strip.setPixelColor(i, strip.Color(255, 0, 0));
    strip.show();
    delay(100);
    strip.clear();
    strip.setPixelColor(i, strip.Color(0, 255, 0));
    strip.show();
    delay(100);
    strip.clear();
    strip.setPixelColor(i, strip.Color(0, 0, 255));
    strip.show();
    delay(100);
    strip.clear();
  }
  // strip.setPixelColor(0, strip.Color(255, 0, 0));
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
void showStepColor(int index, uint32_t color) {
  strip.clear();
  strip.setPixelColor(index, color);
  strip.show();
}
void hideStepLed() {
  digitalWrite(LED_SIG_PIN, LOW);
}
void renderNeoPixels() {
  strip.clear();
  auto color = strip.Color(0, 0, 0);
  if (_hallValues[_position * NUMCORNERS]) {
    color = _colors[0];
  } else if (_hallValues[_position * NUMCORNERS + 1]) {
    color = _colors[1];
  } else if (_hallValues[_position * NUMCORNERS + 2]) {
    color = _colors[2];
  } else if (_hallValues[_position * NUMCORNERS + 3]) {
    color = _colors[3];
  }
  strip.setPixelColor(_position, color);

  if ((_lastChangeIndex != -1) && _lastChangeTimestamp + HALL_CHANGE_FEEDBACK_TIME < millis()) {
    strip.setPixelColor(_lastChangeIndex / NUMCORNERS, _colors[_lastChangeIndex % NUMCORNERS]);
  }
  strip.show();
}
void tick() {
  // TODO: only turn off note of note of current step
  // usbMIDI.sendNoteOff(60, 0, 1);

  auto note = -1;
  auto color = strip.Color(0, 0, 0);
  if (_hallValues[_position * NUMCORNERS]) {
    note = 60;
  } else if (_hallValues[_position * NUMCORNERS + 1]) {
    note = 61;
  } else if (_hallValues[_position * NUMCORNERS + 2]) {
    note = 62;
  } else if (_hallValues[_position * NUMCORNERS + 3]) {
    note = 63;
  }
  if (note != -1) {
    onNoteOn(1, note, 127);
  }


  renderNeoPixels();
  showStepLed(_position);

  usbMIDI.sendNoteOn(note, _volume, 1);

  _mainAmp.gain(((float)(_volume)) / 127);
  _position = (_position + 1) % NUMSTEPS;
  _timestamp = millis();
}
void seasawTick() {
  _velocity = _velocity + _force * _timeStep;
  _position = _position + _velocity * _timeStep;


  if (_position < 0) {
    _position = 0;
    _velocity *= -1;
    _velocity *= _dampeningFactor;
  }
  if (_position > 7) {
    _position = 7;
    _velocity *= -1;
    _velocity *= _dampeningFactor;
  }
}
void readSensors() {
  auto potiValue = analogRead(POTI_PIN);
  bool buttonValue = digitalRead(BUTTON_PIN);
  _seasawMode = !buttonValue;
  _bpm = map(potiValue, 0, 1024, 600, 20);
  mpu.getEvent(&_a, &_g, &_temp);
  lox.rangingTest(&_measure, false);


  // TODO: detect if a step has changed, if so, show new color for 1s

  for (auto i = 0; i < 16; i++) {
    _hallAMux.channel(i);
    _hallBMux.channel(i);
    // save last hall values
    _lastHallValues[i] = _hallValues[i];
    _lastHallValues[i + 16] = _hallValues[i + 16];
    // read new hall values
    _hallValues[i] = (analogRead(HALL_A_SIG_PIN) < 64) ? 1 : 0;
    _hallValues[i + 16] = (analogRead(HALL_B_SIG_PIN) < 64) ? 1 : 0;

    if (_hallValues[i] != _lastHallValues[i]) {
      _lastChangeIndex = i;
      _lastChangeTimestamp = millis();
    } else if (_hallValues[i + 16] != _lastHallValues[i + 16]) {
      _lastChangeIndex = i + 16;
      _lastChangeTimestamp = millis();
    }
  }
}


void onNoteOn(byte channel, byte note, byte velocity) {
  _currentVoice = (_currentVoice + 1) % NUMVOICES;
  _voices[_currentVoice].noteOn(note);
}
void onNoteOff(byte channel, byte note, byte velocity) {}
void onControlChange(byte channel, byte controller, byte value) {
  switch (controller) {
    case 1:
      {
        _filter.setLowpass(0, map(value, 0, 127, 100, 20000), 0.8);
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
    _hallValues[i] = 0;
  }

  // setup parameters
  _on.setup("on", true);
  _seasawMode.setup("seasaw", false);
  _bpm.setup("bpm", 120, 0, 400);

  _position("position", 0, 0, NUMSTEPS);

  // seasaw parameters
  _velocity.setup("velocity", 0, -1, 1);
  _force.setup("force", 0, -1, 1);
  _dampeningFactor.setup("dampening", 0.8, 0, 1);



  // midi
  Serial.print("setup midi engine ... ");
  usbMIDI.setHandleNoteOff(onNoteOff);
  usbMIDI.setHandleNoteOn(onNoteOn);
  usbMIDI.setHandleControlChange(onControlChange);
  Serial.println("done");

  // audio
  Serial.print("setup audio engine ... ");

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
  Serial.println("done");



  // led
  Serial.print("setup leds ... ");
  strip.begin();
  strip.setBrightness(255);  // Set BRIGHTNESS to about 1/5 (max = 255)
  strip.show();
  Serial.println("done");


  // set led high
  digitalWrite(LED_SIG_PIN, HIGH);

  Serial.print("setup distance sensor ... ");
  if (!lox.begin()) {
    Serial.println(F("Failed to boot VL53L0X"));
  }
  Serial.println("done");


  delay(1000);
  Serial.print("setup motion sensor ... ");
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  Serial.println("done");


  neoPixelTest();
}

void loop() {
  auto timestamp = millis();
  usbMIDI.read();
  readSensors();
  printHallValues(_hallValues);

  if (_measure.RangeStatus != 4) {
    updateDistanceBuffer(_measure.RangeMilliMeter);
    if (_measure.RangeMilliMeter > 100) {
      _volume = 127;
    } else {
      _volume = 127 - map(_measure.RangeMilliMeter, 30, 100, 127, 0);
    }
  }

  if (_a.acceleration.x < 0.5) {
    _filter.setLowpass(0, 22000 - map(_a.acceleration.x, 0.5, -5, 0, 21900), 0.8);
    _freeverb.roomsize(map(_a.acceleration.x, 0.5, -5, 0, 1));
    _freeverb.damping(map(_a.acceleration.x, 0.5, -5, 0, 1));
    _reverb.reverbTime(map(_a.acceleration.x, 0.5, -5, 0, 5));
  } else {
    _filter.setLowpass(0, 22000, 0.5);
    _freeverb.roomsize(0);
    _freeverb.damping(0);
    _reverb.reverbTime(0);
  }

  if (_a.acceleration.x > 0.5) {
    _bitcrusher.bits(16 - (int)(map(_a.acceleration.x, 0.5, 5, 0, 16)));
    _bitcrusher.sampleRate(44100 - (int)(map(_a.acceleration.x, 0.5, 5, 0, 44100)));
  } else {
    _bitcrusher.bits(16);
    _bitcrusher.sampleRate(44100);
  }

  if (_a.acceleration.x < 0.5) {
    _filter.setLowpass(0, 5000 - map(_a.acceleration.x, 0.5, -5, 0, 5000));
  } else {
    _filter.setLowpass(0, 22000, 0.5);
  }

  if (_a.acceleration.y < -0.7) {
    _bpmModulator = -map(_a.acceleration.y, -0.7, -3, 0, 80);
  } else {
    _bpmModulator = map(_a.acceleration.y, 0.7, 3, 0, 80);
  }



  if (_on) {
    if (_seasawMode) {
      seasawTick();
    } else {
      if (timestamp - _timestamp > (60.0 * 1000 / (_bpm + _bpmModulator))) {
        tick();
      }
    }
  }
}
