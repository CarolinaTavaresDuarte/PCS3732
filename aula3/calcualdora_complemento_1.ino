const int LED_BIT0 = 2;
const int LED_BIT1 = 3;
const int LED_BIT2 = 4;
const int LED_BIT3 = 5;

// -------- Aritmetica em complemento de 1 --------

// Soma com end-around carry, sobre 4 bits.
int addC1(int a, int b) {
  a &= 0x0F;
  b &= 0x0F;
  int sum = a + b;
  int carry = (sum >> 4) & 1;
  sum &= 0x0F;
  if (carry) {
    sum = (sum + 1) & 0x0F;
  }
  return sum;
}

// Subtracao: a - b = a + (~b), tudo mascarado em 4 bits.
int subC1(int a, int b) {
  int negB = (~b) & 0x0F;
  return addC1(a, negB);
}

// Converte 4 bits c1 para inteiro com sinal.
int c1ToInt(int v) {
  v &= 0x0F;
  if (v & 0x08) return -((~v) & 0x0F);
  return v;
}

// -------- Utilitarios --------

int parseBin4(const String &s) {
  return (int)strtol(s.c_str(), NULL, 2) & 0x0F;
}

String toBin4(int v) {
  v &= 0x0F;
  String s = "";
  for (int i = 3; i >= 0; i--) s += ((v >> i) & 1) ? '1' : '0';
  return s;
}

void atualizarLEDs(int v) {
  digitalWrite(LED_BIT0, (v >> 0) & 1);
  digitalWrite(LED_BIT1, (v >> 1) & 1);
  digitalWrite(LED_BIT2, (v >> 2) & 1);
  digitalWrite(LED_BIT3, (v >> 3) & 1);
}

void imprimeAjuda() {
  Serial.println();
  Serial.println("=== Calculadora 4 bits - Complemento de 1 (Serial) ===");
  Serial.println("Formato: AAAA<op>BBBB");
  Serial.println("  AAAA, BBBB = 4 bits binarios (0/1)");
  Serial.println("  <op> = '+' (soma) ou '-' (subtracao)");
  Serial.println("Exemplo: 0011+0010");
  Serial.println("Intervalo: -7 a +7. Note que 0000 e 1111 sao ambos zero.");
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(200);
  pinMode(LED_BIT0, OUTPUT);
  pinMode(LED_BIT1, OUTPUT);
  pinMode(LED_BIT2, OUTPUT);
  pinMode(LED_BIT3, OUTPUT);
  atualizarLEDs(0);
  imprimeAjuda();
  Serial.print("> ");
}

void loop() {
  if (!Serial.available()) return;

  String linha = Serial.readStringUntil('\n');
  linha.trim();
  if (linha.length() == 0) {
    Serial.print("> ");
    return;
  }

  // localiza o operador (primeiro '+' ou '-' que nao esteja no inicio)
  int posOp = -1;
  char op = 0;
  for (int i = 1; i < (int)linha.length(); i++) {
    char c = linha[i];
    if (c == '+' || c == '-') { posOp = i; op = c; break; }
  }
  if (posOp < 0) {
    Serial.println("Erro de formato. Use AAAA+BBBB ou AAAA-BBBB.");
    Serial.print("> ");
    return;
  }

  String sA = linha.substring(0, posOp);
  String sB = linha.substring(posOp + 1);

  int A = parseBin4(sA);
  int B = parseBin4(sB);
  int res = (op == '+') ? addC1(A, B) : subC1(A, B);

  // Deteccao de overflow em complemento de 1
  int effB = (op == '+') ? B : ((~B) & 0x0F);
  int signA = (A    >> 3) & 1;
  int signB = (effB >> 3) & 1;
  int signR = (res  >> 3) & 1;
  bool overflow = (signA == signB) && (signR != signA);

  atualizarLEDs(res);

  Serial.println();
  Serial.printf("A         = %s  (%d)\n", toBin4(A).c_str(),   c1ToInt(A));
  Serial.printf("B         = %s  (%d)\n", toBin4(B).c_str(),   c1ToInt(B));
  Serial.printf("Operacao  = %c\n", op);
  Serial.printf("Resultado = %s  (%d)\n", toBin4(res).c_str(), c1ToInt(res));
  if (overflow) {
    Serial.println("*** OVERFLOW ***  (resultado fora do intervalo [-7,+7])");
  }
  Serial.println("LEDs atualizados (b3 b2 b1 b0).");
  Serial.println();
  Serial.print("> ");
}