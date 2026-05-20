/*
  Semáforo Completo - ESP32-C3 RGB

  Sequência:
  Verde 3s
  Amarelo 1s
  Vermelho 4s
*/

#include <Adafruit_NeoPixel.h>

#define LED_PIN 8
#define LED_COUNT 1

Adafruit_NeoPixel led(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  led.begin();
}

void loop() {

  // VERDE
  led.setPixelColor(0, led.Color(0, 255, 0));
  led.show();

  delay(3000);

  // AMARELO
  led.setPixelColor(0, led.Color(255, 180, 0));
  led.show();

  delay(1000);

  // VERMELHO
  led.setPixelColor(0, led.Color(255, 0, 0));
  led.show();

  delay(4000);
}
