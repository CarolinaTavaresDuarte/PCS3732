/*
  Semáforo Completo - ESP32-C3

  Sequência:
  Verde -> 3s
  Amarelo -> 1s
  Vermelho -> 4s
*/

#define PIN_GREEN   4
#define PIN_YELLOW  5
#define PIN_RED     6

void setup() {

  pinMode(PIN_GREEN, OUTPUT);
  pinMode(PIN_YELLOW, OUTPUT);
  pinMode(PIN_RED, OUTPUT);

}

void loop() {

  // ESTADO VERDE
  digitalWrite(PIN_GREEN, HIGH);
  digitalWrite(PIN_YELLOW, LOW);
  digitalWrite(PIN_RED, LOW);

  delay(3000);

  // ESTADO AMARELO
  digitalWrite(PIN_GREEN, LOW);
  digitalWrite(PIN_YELLOW, HIGH);
  digitalWrite(PIN_RED, LOW);

  delay(1000);

  // ESTADO VERMELHO
  digitalWrite(PIN_GREEN, LOW);
  digitalWrite(PIN_YELLOW, LOW);
  digitalWrite(PIN_RED, HIGH);

  delay(4000);
}