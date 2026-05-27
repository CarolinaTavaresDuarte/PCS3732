// Bit 3 (Pino 7), Bit 2 (Pino 6), Bit 1 (Pino 5), Bit 0 (Pino 4)
const int ledPins[4] = {7, 6, 5, 4};

void setup() {
  for (int i = 0; i < 4; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }
}

void loop() {
  // Efeito sequencial usando Bit Shift para validar cada trilha
  for (int i = 0; i < 4; i++) {
    digitalWrite(ledPins[i], HIGH);
    delay(200);
    digitalWrite(ledPins[i], LOW);
  }
  delay(500);
}