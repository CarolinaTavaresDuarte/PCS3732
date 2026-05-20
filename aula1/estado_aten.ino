/*
  Etapa - Estado de Atenção
  Semáforo em modo amarelo piscante usando o LED built-in RGB da ESP32-C3.
*/

#include <Adafruit_NeoPixel.h>

#define PIN_YELLOW 8
#define LED_COUNT 1

Adafruit_NeoPixel led(LED_COUNT, PIN_YELLOW, NEO_GRB + NEO_KHZ800);

void setup() {
  led.begin();
}

void loop() {

  // Amarelo ligado
  led.setPixelColor(0, led.Color(255, 180, 0));
  led.show();

  delay(500);

  // LED apagado
  led.clear();
  led.show();

  delay(500);
}
