/*
  Semáforo amarelo piscante
  ESP32-C3
*/

#define LED_RGB 8

void setup() {
}

void loop() {

  // Amarelo ligado
  neopixelWrite(LED_RGB, 255, 180, 0);
  delay(500);

  // Apagado
  neopixelWrite(LED_RGB, 0, 0, 0);
  delay(500);
}