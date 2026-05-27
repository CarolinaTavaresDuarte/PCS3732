/*
Semáforo com LED RGB do ESP32-C3
Sem bibliotecas externas
*/

#define RGB_BUILTIN 8   // GPIO do LED RGB integrado

void setup() {
}

void loop() {

  // VERDE
  rgbLedWrite(RGB_BUILTIN, 0, 255, 0);
  delay(3000);

  // AMARELO
  rgbLedWrite(RGB_BUILTIN, 255, 255, 0);
  delay(1000);

  // VERMELHO
  rgbLedWrite(RGB_BUILTIN, 255, 0, 0);
  delay(4000);
}