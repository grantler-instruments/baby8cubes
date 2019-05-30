#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <Adafruit_NeoPixel.h>

#define NUMSTEPS 8
#define LEDPIN 24

#include "AudioSampleSnare.h"        // http://www.freesound.org/people/KEVOY/sounds/82583/
#include "AudioSampleTomtom.h"       // http://www.freesound.org/people/zgump/sounds/86334/
#include "AudioSampleHihat.h"        // http://www.freesound.org/people/mhc/sounds/102790/
#include "AudioSampleKick.h"         // http://www.freesound.org/people/DWSD/sounds/171104/

int bpm = 120;
int step = 0;

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

int pins[NUMSTEPS] = {
  A13,
  A20,
  A19,
  A18,

  A17,
  A16,
  A15,
  A12

};

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMSTEPS, LEDPIN, NEO_RGB + NEO_KHZ800);

void test() {
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
void setup() {
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
}

void loop() {
  strip.clear();

  auto input = analogRead(pins[step]);
  if (input < 128) { // cube not placed
    strip.setPixelColor(step, strip.Color(255, 255, 255));
  } else if (input < 256) { // cube rotated to kick
    strip.setPixelColor(step, strip.Color(255, 0, 0));
    sounds[step].play(AudioSampleKick);
  } else if (input < 512) { // cube rotated to snare
    strip.setPixelColor(step, strip.Color(0, 255, 0));
    sounds[step].play(AudioSampleSnare);
  } else if (input < 768) { // cube rotated to hihats
    strip.setPixelColor(step, strip.Color(0, 0, 255));
    sounds[step].play(AudioSampleHihat);
  } else {// cube rotated to clap
    strip.setPixelColor(step, strip.Color(255, 0, 255));
    sounds[step].play(AudioSampleTomtom);
  }
  strip.show();

  // move to the next step and wait for the next tick
  step = (step + 1) % NUMSTEPS;
  delay(60.0 * 1000 / bpm);

    test();
}
