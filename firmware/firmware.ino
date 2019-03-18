// teensy 3.6, audio shield

#define NUMBEROFSTEPS 8
#define NUMBEROFOPTIONSPERSTEPS 4
#include <Adafruit_NeoPixel.h>

int stepPins[NUMBEROFSTEPS];
int NEOPIXELPIN = 6;
int BPMPIN = 6;
int bpm = 120;
uint32_t colors[NUMBEROFSTEPS];


Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMBEROFSTEPS, NEOPIXELPIN, NEO_GRB + NEO_KHZ800);

void setup() {
  //TODO: check pins for audio shield
  stepPins[0] = 23;
  stepPins[1] = 22;
  stepPins[2] = 21;
  stepPins[3] = 20;
  stepPins[4] = 19;
  stepPins[5] = 18;
  stepPins[6] = 17;
  stepPins[7] = 16;

  colors[0] = strip.Color(255, 0, 0);
  colors[1] = strip.Color(0, 255, 0);
  colors[2] = strip.Color(0, 0, 255);
  colors[3] = strip.Color(255, 0, 255);


  strip.begin();
  strip.show();
}

void loop() {
  auto bpmInput = analogRead(BPMPIN);
  bpm = map(bpmInput, 0, 1023, 20, 200);
  auto delayTime = 60.0 * 1000 / bpm;
  for (auto i = 0; i < NUMBEROFSTEPS; i++) {
    auto value = analogRead(stepPins[i]);
    //TODO: map value to note
    int note = 0;
    strip.setPixelColor(i, colors[note]);
    // TODO: play sound
    // TODO: output midi
    delay(delayTime);
    strip.setPixelColor(i, 0, 0, 0);
  }
}
