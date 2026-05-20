/*
  Etapa - Estado de Atenção
  Semáforo em modo amarelo piscante usando o LED built-in da ESP32-C3.
*/

#define PIN_YELLOW LED_BUILTIN

void setup() {
  pinMode(PIN_YELLOW, OUTPUT);
}

void loop() {
  digitalWrite(PIN_YELLOW, HIGH);
  delay(500);

  digitalWrite(PIN_YELLOW, LOW);
  delay(500);
}