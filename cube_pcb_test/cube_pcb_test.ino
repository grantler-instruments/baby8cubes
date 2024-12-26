#include <AceButton.h>
#include <Adafruit_NeoPixel.h>

using namespace ace_button;

const uint8_t NUMBER_OF_SENSORS = 4;
#define PIN_NEOPIXEL 6

const int buttonPin = 2;  // the number of the pushbutton pin
const int ledPin = 13;    // the number of the LED pin

// variables will change:

AceButton _buttons[NUMBER_OF_SENSORS];
Adafruit_NeoPixel strip(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// Forward reference to prevent Arduino compiler becoming confused.
void handleEvent(AceButton*, uint8_t, uint8_t);

void setup() {
  Serial.begin(115200);


  for (uint8_t i = 0; i < NUMBER_OF_SENSORS; i++) {
    // initialize built-in LED as an output
    pinMode(2 + i, INPUT_PULLUP);
    _buttons[i].init(2 + i, HIGH, i);
  }
  ButtonConfig* buttonConfig = ButtonConfig::getSystemButtonConfig();
  buttonConfig->setEventHandler(handleEvent);
  buttonConfig->setFeature(ButtonConfig::kFeatureClick);
  // buttonConfig->setFeature(ButtonConfig::kFeatureDoubleClick);
  // buttonConfig->setFeature(ButtonConfig::kFeatureLongPress);
  // buttonConfig->setFeature(ButtonConfig::kFeatureRepeatPress);

  strip.begin();  // INITIALIZE NeoPixel strip object (REQUIRED)
  strip.show();   // Turn OFF all pixels ASAP
  strip.setBrightness(50);
}

void loop() {
  for (uint8_t i = 0; i < NUMBER_OF_SENSORS; i++) {
    _buttons[i].check();
  }
}

void handleEvent(AceButton* button, uint8_t eventType, uint8_t buttonState) {
  // Get the LED pin
  uint8_t id = button->getId();

  // Control the LED only for the Pressed and Released events.
  // Notice that if the MCU is rebooted while the button is pressed down, no
  // event is triggered and the LED remains off.
  uint32_t colors[] = {
    strip.Color(255, 0, 0),
    strip.Color(0, 255, 0),
    strip.Color(0, 0, 255),
    strip.Color(255, 0, 255)
  };

  switch (eventType) {
    case AceButton::kEventPressed:
      Serial.print("pressed: ");
      Serial.println(id);
      strip.setPixelColor(0, colors[id]);  //  Set pixel's color (in RAM)
      strip.show();
      break;
    case AceButton::kEventReleased:
      Serial.print("released: ");
      Serial.println(id);
      strip.clear();
      strip.show();
      break;
  }
}
