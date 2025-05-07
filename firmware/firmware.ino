// !!!! select usb type: serial + audio + midi
#include <Arduino.h>
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_VL53L0X.h>
#include <Adafruit_MPU6050.h>
#include <CD74HC4067.h>      //https://github.com/waspinator/CD74HC4067
#include <Parameter.h>       //https://github.com/thomasgeissl/Parameter
#include <uClock.h>          //https://github.com/midilab/uClock
#include "RunningAverage.h"  //https://github.com/RobTillaart/RunningAverage/
#include "config.h"
#include "helpers.h"
#include "voice.h"
#include "NeoPixelState.h"


CD74HC4067 _ledMux(LED_SELECT_0_PIN, LED_SELECT_1_PIN, LED_SELECT_2_PIN, LED_SELECT_3_PIN);
CD74HC4067 _hallAMux(HALL_A_SELECT_0_PIN, HALL_A_SELECT_1_PIN, HALL_A_SELECT_2_PIN, HALL_A_SELECT_3_PIN);
CD74HC4067 _hallBMux(HALL_B_SELECT_0_PIN, HALL_B_SELECT_1_PIN, HALL_B_SELECT_2_PIN, HALL_B_SELECT_3_PIN);

int _bpmModulator = 0;
int _volume = 127;
int _distance = 0;
float _lastStableDistance = 100;  // Default to max distance
long _lastStableReadingTime = 0;


float _timeStep = 0.1;
bool _resetRequested = false;

NeoPixelState _neoPixelState;

bool _hallValues[NUMSTEPS * NUMCORNERS];
bool _lastHallValues[NUMSTEPS * NUMCORNERS];

Voice _voices[NUMVOICES];
int _currentVoice = 0;

RunningAverage _accelerationX(16);
RunningAverage _accelerationY(16);
// RunningAverage _distance(16);

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
AudioMixer4 _delayInMixer;


// connect voices to mixer
AudioConnection voice0ToMixer(_voices[0]._amp, 0, _firstMixer, 0);
AudioConnection voice1ToMixer(_voices[1]._amp, 0, _firstMixer, 1);
AudioConnection voice2ToMixer(_voices[2]._amp, 0, _firstMixer, 2);
AudioConnection voice3ToMixer(_voices[3]._amp, 0, _firstMixer, 3);
AudioConnection voice4ToMixer(_voices[4]._amp, 0, _secondMixer, 0);
AudioConnection voice5ToMixer(_voices[5]._amp, 0, _secondMixer, 1);
AudioConnection voice6ToMixer(_voices[6]._amp, 0, _secondMixer, 2);
AudioConnection voice7ToMixer(_voices[7]._amp, 0, _secondMixer, 3);
AudioConnection firstMixerToMainMixer(_firstMixer, 0, _mainMixer, 0);
AudioConnection secondMixerToMainMixer(_secondMixer, 0, _mainMixer, 1);

AudioConnection _mainMixerToDelayInMixer(_mainMixer, 0, _delayInMixer, 0);
// AudioConnection _filterToDelayInMixer0(_filter, 0, _delayInMixer, 0);
AudioConnection _delayInMixerToDelay(_delayInMixer, 0, _delay, 0);

AudioConnection delay0ToDelayMixer(_delay, 0, _delayMixer, 0);
AudioConnection delay1ToDelayMixer(_delay, 1, _delayMixer, 1);
AudioConnection delay2ToDelayMixer(_delay, 2, _delayMixer, 2);
AudioConnection delay3ToDelayMixer(_delay, 3, _delayMixer, 3);

AudioConnection _delayMixerToBitCrusher(_delayMixer, 0, _bitcrusher, 0);
AudioConnection _delayFeedbackToDelayInMixer(_delayMixer, 0, _delayInMixer, 1);
AudioConnection _bitcrusherToMainAmp(_bitcrusher, _mainAmp);

AudioConnection _mainAmpToHeadphones0(_mainAmp, 0, _headphones, 0);
AudioConnection _mainAmpToHeadphones1(_mainAmp, 0, _headphones, 1);
AudioConnection _mainAmpToDac0(_mainAmp, 0, _dac, 0);
AudioConnection _mainAmpToDac1(_mainAmp, 0, _dac, 1);

AudioConnection _mainAmpToUsbL(_mainAmp, 0, _usbOutput, 0);  // left channel
AudioConnection _mainAmpToUsbR(_mainAmp, 0, _usbOutput, 1);  // right channel


Adafruit_NeoPixel _strip = Adafruit_NeoPixel(NUMSTEPS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_VL53L0X lox = Adafruit_VL53L0X();
Adafruit_MPU6050 _mpu;
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

// four dots (blue), one dots (red), two dots (green), trhee dot (purple)
uint32_t _colors[NUMCORNERS] = { _strip.Color(0, 255, 0), _strip.Color(255, 255, 0), _strip.Color(0, 0, 255), _strip.Color(255, 0, 255) };

#define BUFFER_SIZE 10  // Number of readings to store in history
int distanceBuffer[BUFFER_SIZE];
int bufferIndex = 0;
bool isJumpy = false;

void updateDistanceBuffer(int newValue) {
  // Treat "no reading" as maximum distance (200mm)
  distanceBuffer[bufferIndex] = (_measure.RangeStatus == 4) ? 200 : newValue;
  bufferIndex = (bufferIndex + 1) % BUFFER_SIZE;
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
  isJumpy = variance > JUMPY_THRESHOLD;

  // Debug output
  // Serial.print("Variance: ");
  // Serial.print(variance);
  // Serial.print(" | Jumpy: ");
  // Serial.println(isJumpy ? "YES" : "NO");
}
void audioTest() {
  _voices[0].noteOn(60);
  delay(250);
}
void neoPixelTest() {
  Serial.print("started neopixel test");
  for (auto i = 0; i < NUMSTEPS; i++) {
    _strip.clear();
    _strip.setPixelColor(i, _strip.Color(255, 0, 0));
    _strip.show();
    delay(100);
    _strip.clear();
    _strip.setPixelColor(i, _strip.Color(0, 255, 0));
    _strip.show();
    delay(100);
    _strip.clear();
    _strip.setPixelColor(i, _strip.Color(0, 0, 255));
    _strip.show();
    delay(100);
    _strip.clear();
  }
  // _strip.setPixelColor(0, _strip.Color(255, 0, 0));
  _strip.show();
  delay(100);
  Serial.println(": done");
}

void updatePlayheadIndicator() {
  _ledMux.channel(_position);
}

void hideStepLed() {
  digitalWrite(LED_SIG_PIN, LOW);
}
void updateNeoPixels() {
  if ((_lastChangeIndex != -1) && (_lastChangeTimestamp + HALL_CHANGE_FEEDBACK_TIME > millis())) {
    _neoPixelState.setColor(_lastChangeIndex / NUMCORNERS, _colors[_lastChangeIndex % NUMCORNERS]);
  } else {
    _lastChangeIndex = -1;
  }

  if (_neoPixelState._hasChanged) {
    _strip.clear();
    for (auto i = 0; i < NUMSTEPS; i++) {
      auto color = _neoPixelState._pixels[i];
      _strip.setPixelColor(i, color);
    }
    _strip.show();
    _neoPixelState._hasChanged = false;
  }
}

void updateControls() {
  Serial.println(_bpmModulator);
  uClock.setTempo(_bpm + _bpmModulator);
  if (_measure.RangeStatus != 4) {
    updateDistanceBuffer(_measure.RangeMilliMeter);
    checkJumpy();
    if (!isJumpy) {
      _lastStableDistance = _measure.RangeMilliMeter;
      _lastStableReadingTime = millis();
      _volume = (_lastStableDistance > 100) ? 127 : 127 - map(_lastStableDistance, 30, 100, 127, 0);
    } else if (millis() - _lastStableReadingTime > STABLE_TIMEOUT) {
      _volume = 127;  // Default to max volume
    }
  }

  // Serial.print(_a.acceleration.x);
  // Serial.print(", ");
  // Serial.print(_a.acceleration.y);
  // Serial.print(" ");
  // Serial.println(_volume);

  auto threshold = 1.3;


  if (_accelerationX.getAverage() < threshold - 0.1) {
    _delayMixer.gain(0, .4);
    _delayMixer.gain(1, 0.4);
    _delayMixer.gain(2, 0.4);
    _delayMixer.gain(3, 0.4);
    _delayMixer.gain(0, map(_a.acceleration.x, threshold, -5, 0, 1));
    _delayInMixer.gain(1, map(_a.acceleration.x, threshold, -5, 0, 1));  //feedback
  } else {
    _delayMixer.gain(0, 0);
    _delayMixer.gain(0, .4);
    _delayMixer.gain(1, 0);
    _delayMixer.gain(2, 0);
    _delayMixer.gain(3, 0);

    _delayInMixer.gain(1, 0);  //feedback
  }

  if (_accelerationX.getAverage() > threshold + 0.1) {
    _bitcrusher.bits(16 - (int)(map(_accelerationX.getAverage(), 0.5, 8, 0, 16)));
    _bitcrusher.sampleRate(44100 - (int)(map(_accelerationX.getAverage(), 0.5, 8, 0, 44100)));
  } else {
    _bitcrusher.bits(16);
    _bitcrusher.sampleRate(44100);
  }


  if (_accelerationY.getAverage() < -ACCELERATION_Y_THRESHOLD) {
    _bpmModulator = -map(_accelerationY.getAverage(), -ACCELERATION_Y_THRESHOLD, -3, 0, 120);
  } else if (_accelerationY.getAverage() > ACCELERATION_Y_THRESHOLD) {
    _bpmModulator = map(_accelerationY.getAverage(), ACCELERATION_Y_THRESHOLD, 3, 0, 120);
  }
}
void tick() {
  if (_resetRequested) {
    _position = 0;
    _resetRequested = false;
  }

  auto note = -1;
  auto lastNote = -1;

  auto color = _strip.Color(0, 0, 0);
  if (_hallValues[_position * NUMCORNERS]) {
    note = 60;
    color = _colors[0];
  } else if (_hallValues[_position * NUMCORNERS + 1]) {
    note = 61;
    color = _colors[1];
  } else if (_hallValues[_position * NUMCORNERS + 2]) {
    note = 62;
    color = _colors[2];
  } else if (_hallValues[_position * NUMCORNERS + 3]) {
    note = 63;
    color = _colors[3];
  }

  if (_lastHallValues[_position * NUMCORNERS]) {
    lastNote = 60;
  } else if (_lastHallValues[_position * NUMCORNERS + 1]) {
    lastNote = 61;
  } else if (_lastHallValues[_position * NUMCORNERS + 2]) {
    lastNote = 62;
  } else if (_lastHallValues[_position * NUMCORNERS + 3]) {
    lastNote = 63;
  }


  _neoPixelState.clear();
  _neoPixelState.setColor(_position, color);

  //send out midi notes via usb and the audio renderer
  if (lastNote != -1) {
    onNoteOff(1, lastNote, 127);
    usbMIDI.sendNoteOff(lastNote, 0, 1);
  }
  if (note != -1) {
    onNoteOn(1, note, 127);
    usbMIDI.sendNoteOn(note, _volume, 1);
  }

  updatePlayheadIndicator();

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
  if (_on == buttonValue) {
    _on = !buttonValue;
  }
  _bpm = map(potiValue, 0, 1024, 600, 20);
  _mpu.getEvent(&_a, &_g, &_temp);
  _accelerationX.addValue(_a.acceleration.x);
  _accelerationY.addValue(_a.acceleration.y);

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

void onPPQNCallback(uint32_t tick) {
}
void onSync24Callback(uint32_t uTick) {
  usbMIDI.sendRealTime(usbMIDI.Clock);
}

void onStepCallback(uint32_t uTick) {
  if (_on) {
    tick();
  }
}


void onClockStart() {
  usbMIDI.sendRealTime(usbMIDI.Start);
}

void onClockStop() {
  usbMIDI.sendRealTime(usbMIDI.Stop);
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
  _on.addListener([](Parameter<bool>& parameter) {
    Serial.println(parameter.getName() + " changed, new value: " + String(parameter.get()));
    if (parameter) {
      uClock.start();
    } else {
      uClock.pause();
    }
  });
  _seasawMode.setup("seasaw", false);
  _bpm.setup("bpm", 120, 0, 400);

  _position.setup("position", 0, 0, NUMSTEPS);

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

  _delayInMixer.gain(0, 0.5);
  _delayInMixer.gain(1, 0);

  _delay.delay(0, 50);
  _delay.delay(1, 78);
  _delay.delay(2, 103);
  _delay.delay(3, 174);
  for (auto i = 4; i < 8; i++) {
    _delay.disable(i);
  }
  _delayMixer.gain(0, 0.4);
  _delayMixer.gain(1, 0.4);
  _delayMixer.gain(2, 0.4);
  _delayMixer.gain(3, 0.4);

  _mainMixer.gain(0, 0.5);
  _mainMixer.gain(1, 0.5);
  _mainAmp.gain(1);
  // audioTest();
  Serial.println("done");

  // led
  Serial.print("setup leds ... ");
  _strip.begin();
  _strip.setBrightness(255);  // Set BRIGHTNESS to about 1/5 (max = 255)
  _strip.show();
  Serial.println("done");

  // set playhead led high
  digitalWrite(LED_SIG_PIN, HIGH);

  Serial.print("setup distance sensor ... ");
  if (!lox.begin()) {
    Serial.println(F("Failed to boot VL53L0X"));
  }
  Serial.println("done");

  delay(1000);
  Serial.print("setup motion sensor ... ");
  if (!_mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
  }
  _mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  Serial.println("done");

  // setup midi clock
  uClock.init();
  uClock.setOnPPQN(onPPQNCallback);
  uClock.setOnSync24(onSync24Callback);
  uClock.setOnClockStart(onClockStart);
  uClock.setOnClockStop(onClockStop);
  uClock.setTempo(120);
  uClock.setOnStep(onStepCallback);
  uClock.stop();

  neoPixelTest();
}

void loop() {
  usbMIDI.read();
  readSensors();
  updateControls();
  updateNeoPixels();
}
