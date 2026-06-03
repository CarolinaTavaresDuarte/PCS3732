/*
 * PCS3732 — Laboratório 04 — DESAFIO ADICIONAL
 * Calculadora binária com soma, sub, mult, fatorial e DIVISÃO em c2.
 *
 * Mesma semântica do sketch principal:
 *   - Operandos em complemento de 2 com sinal (-32768 a +32767).
 *   - Fatorial só com A >= 0 (trava no front-end).
 *   - Multiplicação corrigida para qualquer combinação de sinais.
 *   - DIVISÃO INTEIRA por subtrações sucessivas (didático, simétrico à
 *     multiplicação por adições sucessivas).
 *   - Tratamento explícito de divisão por zero.
 *   - LEDs físicos sempre exibem os 4 LSB; display web expande conforme
 *     necessário para representar o resultado em c2 completo.
 *
 * Pinagem ESP32-C3: GPIOs 2, 3, 4, 5 (LSB -> MSB).
 */

#include <WiFi.h>
#include <WebServer.h>

const int LED_BIT0 = 2;
const int LED_BIT1 = 3;
const int LED_BIT2 = 4;
const int LED_BIT3 = 5;

const char* ssid     = "Calculadora_grupo_lindo";
const char* password = "12345678";

WebServer server(80);

const int FAT_MAX = 12;

// Operações aritméticas

// Multiplicação por adições sucessivas, robusta para qualquer sinal.
// O 'volatile' impede o compilador de otimizar o laço para uma instrução
// 'mul' nativa em hardware — preserva o comportamento didático O(|B|).
int multiply(int a, int b) {
  volatile int result = 0;
  int abs_b = (b < 0) ? -b : b;
  for (int i = 0; i < abs_b; i++) {
    result += a;
  }
  return (b < 0) ? -result : result;
}

// Fatorial iterativo. Front trava A < 0; valida aqui também.
int factorial(int n) {
  if (n < 0) return 0;
  if (n <= 1) return 1;
  int result = 1;
  for (int i = 2; i <= n; i++) result *= i;
  return result;
}

// Divisão inteira por SUBTRAÇÕES SUCESSIVAS (algoritmo didático, simétrico
// à multiplicação por adições). Trata todos os sinais e devolve quociente
// e resto (convenção: resto tem sinal do dividendo, como o operador % do C).
// Retorna 0 se b == 0 (erro tratado no handler).
// O 'volatile' em abs_a impede o compilador de otimizar o laço para uma
// instrução 'div' nativa em hardware — preserva o comportamento didático
// O(|A|/|B|).
int divide(int a, int b, int *remainder) {
  if (b == 0) { *remainder = 0; return 0; }

  int sign_a = (a < 0) ? -1 : 1;
  int sign_b = (b < 0) ? -1 : 1;
  volatile int abs_a = (a < 0) ? -a : a;
  int abs_b = (b < 0) ? -b : b;

  int q = 0;
  // subtrai abs_b de abs_a quantas vezes for possível
  while (abs_a >= abs_b) {
    abs_a -= abs_b;
    q++;
  }

  *remainder = sign_a * abs_a;          // resto com sinal do dividendo
  return sign_a * sign_b * q;           // quociente com sinal combinado
}

// Utilitários de representação em c2

int bitsNeeded(int n) {
  if (n >= -8 && n <= 7) return 4;
  int bits = 4;
  long min_val = -8;
  long max_val = 7;
  while (n < min_val || n > max_val) {
    bits++;
    min_val *= 2;
    max_val = max_val * 2 + 1;
    if (bits >= 32) break;
  }
  return bits;
}

String toC2(int n, int bits) {
  if (bits < 1) bits = 1;
  if (bits > 32) bits = 32;
  unsigned int mask = (bits == 32) ? 0xFFFFFFFFU : ((1U << bits) - 1U);
  unsigned int v = ((unsigned int)n) & mask;
  String s = "";
  for (int i = bits - 1; i >= 0; i--) {
    s += ((v >> i) & 1) ? '1' : '0';
  }
  return s;
}

void atualizarLEDs(int v) {
  digitalWrite(LED_BIT0, (v >> 0) & 1);
  digitalWrite(LED_BIT1, (v >> 1) & 1);
  digitalWrite(LED_BIT2, (v >> 2) & 1);
  digitalWrite(LED_BIT3, (v >> 3) & 1);
}

// HTML/JS embarcado
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Calculadora — Lab 04 + Divisão</title>
  <style>
    body { font-family: Arial, sans-serif; max-width: 600px; margin: 18px auto;
           padding: 12px; background: #f4f4f8; color: #222; }
    h1 { font-size: 1.3em; margin-bottom: 4px; }
    .sub { color: #555; font-size: 0.9em; margin-bottom: 14px; }
    .row { margin: 10px 0; display: flex; align-items: center; gap: 10px; }
    label { display: inline-block; width: 110px; font-weight: bold; }
    input[type="number"] { font-family: monospace; font-size: 1.15em; padding: 6px;
            width: 100px; text-align: center; border: 1px solid #aaa;
            border-radius: 4px; }
    .binview { font-family: monospace; color: #555; font-size: 0.9em; }
    .ops { display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 8px; margin: 16px 0; }
    button { font-size: 0.92em; padding: 11px 6px; cursor: pointer; border: 0;
             border-radius: 6px; background: #2563eb; color: white; font-weight: bold; }
    button:hover { background: #1d4ed8; }
    button.special { background: #7c3aed; }
    button.special:hover { background: #6d28d9; }
    button.danger { background: #db2777; }
    button.danger:hover { background: #be185d; }
    .leds-section { background: #fff; padding: 12px; border-radius: 8px;
                    border: 1px solid #ddd; margin-top: 12px; }
    .leds-section h3 { margin: 4px 0 8px 0; font-size: 0.95em; color: #444; }
    .leds { display: flex; gap: 8px; justify-content: center; flex-wrap: wrap; margin: 8px 0; }
    .ledbox { text-align: center; }
    .led { width: 36px; height: 36px; border-radius: 50%; background: #333;
           border: 2px solid #555; margin: 0 auto; }
    .led.on { background: #4ade80; box-shadow: 0 0 12px #4ade80; }
    .ledbox.extra .led { border-color: #f59e0b; }
    .ledbox.extra .led.on { background: #fbbf24; box-shadow: 0 0 12px #fbbf24; }
    .ledlabel { font-size: 0.7em; color: #666; margin-top: 3px; }
    .resultado { background: #fff; padding: 12px; border-radius: 8px;
                 margin-top: 12px; border: 1px solid #ddd; }
    .resultado .linha { margin: 4px 0; }
    .resultado .destaque { font-size: 1.18em; margin-top: 8px; padding-top: 8px;
                           border-top: 1px solid #eee; }
    .bench { background: #eef6ff; padding: 8px 10px; border-radius: 6px;
             margin-top: 8px; font-size: 0.92em; color: #1e40af; }
    .overflow { background: #fef3c7; color: #92400e;
                padding: 10px; border-radius: 6px; margin-top: 8px;
                border-left: 4px solid #f59e0b; font-size: 0.92em; }
    .erro { background: #fee2e2; color: #b91c1c; font-weight: bold;
            padding: 10px; border-radius: 6px; border: 2px solid #b91c1c; }
    code { background: #eef; padding: 1px 4px; border-radius: 3px;
           font-family: Consolas, monospace; }
    .legenda { font-size: 0.78em; color: #777; margin-top: 6px; }
  </style>
</head>
<body>
  <h1>Calculadora Binária — Lab 04 + Divisão</h1>
  <div class="sub">Soma, subtração, multiplicação, fatorial e divisão em complemento de 2.</div>

  <div class="row">
    <label>Operando A:</label>
    <input id="a" type="number" min="-32768" max="32767" step="1" value="10">
    <span class="binview" id="aBin">= 01010</span>
  </div>
  <div class="row">
    <label>Operando B:</label>
    <input id="b" type="number" min="-32768" max="32767" step="1" value="2">
    <span class="binview" id="bBin">= 0010</span>
  </div>

  <div class="ops">
    <button onclick="calcular('add')">SOMA</button>
    <button onclick="calcular('sub')">SUB</button>
    <button onclick="calcular('mul')">MULT</button>
    <button class="special" onclick="calcular('fat')">FATORIAL (A!)</button>
    <button class="danger" onclick="calcular('div')">DIV (A &divide; B)</button>
  </div>

  <div class="resultado" id="resultado">Escolha A, B e clique em uma operação.</div>

  <div class="leds-section">
    <h3>Display do resultado em complemento de 2</h3>
    <div class="leds" id="leds-virtuais"></div>
    <div class="legenda">
      LEDs <span style="color:#16a34a">verdes</span> = 4 bits originais (LSB).
      LEDs <span style="color:#d97706">laranja</span> = bits adicionais para valores fora de [-8, +7].
    </div>
  </div>

  <p style="font-size: 0.78em; color: #666; margin-top: 16px;">
    <b>Nota:</b> aritmética em C no ESP32; tempo medido com <code>micros()</code>.
    A divisão é implementada por subtrações sucessivas (simétrica à multiplicação
    por adições). Trata divisão por zero como exceção.
  </p>

  <script>
    function toC2(n, bits) {
      const mask = bits >= 32 ? 0xFFFFFFFF : ((1 << bits) - 1);
      const v = (n >>> 0) & mask;
      let s = v.toString(2).padStart(bits, '0');
      if (s.length > bits) s = s.slice(-bits);
      return s;
    }
    function bitsNeeded(n) {
      if (n >= -8 && n <= 7) return 4;
      let bits = 4, lo = -8, hi = 7;
      while (n < lo || n > hi) {
        bits++;
        lo *= 2;
        hi = hi * 2 + 1;
        if (bits >= 32) break;
      }
      return bits;
    }
    function refreshBin() {
      const a = parseInt(document.getElementById('a').value, 10);
      const b = parseInt(document.getElementById('b').value, 10);
      document.getElementById('aBin').textContent =
        Number.isInteger(a) ? '= ' + toC2(a, bitsNeeded(a)) : '= ----';
      document.getElementById('bBin').textContent =
        Number.isInteger(b) ? '= ' + toC2(b, bitsNeeded(b)) : '= ----';
    }
    document.getElementById('a').addEventListener('input', refreshBin);
    document.getElementById('b').addEventListener('input', refreshBin);

    async function calcular(op) {
      const a = parseInt(document.getElementById('a').value, 10);
      const b = parseInt(document.getElementById('b').value, 10);
      const div = document.getElementById('resultado');

      if (!Number.isInteger(a)) {
        div.innerHTML = '<div class="erro">A deve ser inteiro.</div>';
        return;
      }
      if (op !== 'fat' && !Number.isInteger(b)) {
        div.innerHTML = '<div class="erro">B deve ser inteiro.</div>';
        return;
      }
      if (op === 'fat' && a < 0) {
        div.innerHTML = '<div class="erro">&#9888; Fatorial não é definido para números negativos. Use A &ge; 0.</div>';
        atualizarLedsVirtuais('', 0);
        return;
      }
      if (op === 'fat' && a > 12) {
        div.innerHTML = '<div class="erro">&#9888; Fatorial limitado a A &le; 12 (overflow do int32).</div>';
        atualizarLedsVirtuais('', 0);
        return;
      }
      if (op === 'div' && b === 0) {
        div.innerHTML = '<div class="erro">&#9888; Divisão por zero indefinida.</div>';
        atualizarLedsVirtuais('', 0);
        return;
      }

      try {
        const resp = await fetch(`/calc?a=${a}&b=${b}&op=${op}`);
        const j = await resp.json();

        if (j.error) {
          div.innerHTML = '<div class="erro">&#9888; ' + j.errMsg + '</div>';
          return;
        }

        atualizarLedsVirtuais(j.bin_c2, j.bits_used);

        const sym = { add: '+', sub: '\u2212', mul: '\u00d7', div: '\u00f7' };
        let expr;
        if (op === 'fat') expr = `${a}!`;
        else expr = `${a} ${sym[op]} ${b}`;

        let html = '';
        html += `<div class="linha"><b>Operação:</b> ${expr}</div>`;
        html += `<div class="destaque"><b>Resultado:</b> ${j.result} &nbsp; <code>${j.bin_c2}</code> <small>(${j.bits_used} bits em c2)</small></div>`;
        if (op === 'div') {
          html += `<div class="linha"><b>Resto:</b> ${j.remainder}</div>`;
        }

        if (j.bits_used > 4) {
          html += `<div class="overflow"><b>&#9888; Não cabe em 4 bits de c2.</b><br>` +
                  `O resultado ${j.result} requer ${j.bits_used} bits em c2. ` +
                  `Os 4 LEDs físicos mostram os 4 LSB (<code>${j.leds_4bit}</code>), ` +
                  `que isoladamente representariam o valor ${j.leds_4bit_as_c2} em c2 de 4 bits.</div>`;
        }

        html += `<div class="bench">&#9201; Tempo de execução no ESP32: <b>${j.time_us} &micro;s</b></div>`;

        div.innerHTML = html;
      } catch (e) {
        div.innerHTML = '<div class="erro">Erro de comunicação: ' + e + '</div>';
      }
    }

    function atualizarLedsVirtuais(bin, bits_used) {
      const container = document.getElementById('leds-virtuais');
      container.innerHTML = '';
      if (!bin) return;
      for (let i = 0; i < bin.length; i++) {
        const bitIdx = bin.length - 1 - i;
        const isExtra = bitIdx >= 4;
        const box = document.createElement('div');
        box.className = 'ledbox' + (isExtra ? ' extra' : '');
        const led = document.createElement('div');
        led.className = 'led' + (bin[i] === '1' ? ' on' : '');
        box.appendChild(led);
        const lbl = document.createElement('div');
        lbl.className = 'ledlabel';
        lbl.textContent = 'b' + bitIdx + (bitIdx === bin.length - 1 ? ' (MSB)' : '');
        box.appendChild(lbl);
        container.appendChild(box);
      }
    }

    refreshBin();
  </script>
</body>
</html>
)rawliteral";

// Handlers HTTP

void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void handleCalc() {
  int a = server.arg("a").toInt();
  int b = server.arg("b").toInt();
  String op = server.arg("op");

  int result = 0;
  int remainder = 0;
  bool error = false;
  String errMsg = "";

  // Validações no backend
  if (op == "fat" && a < 0) {
    error = true;
    errMsg = "Fatorial não definido para A negativo.";
  } else if (op == "fat" && a > FAT_MAX) {
    error = true;
    errMsg = "Fatorial limitado a A <= " + String(FAT_MAX) + ".";
  } else if (op == "div" && b == 0) {
    error = true;
    errMsg = "Divisão por zero indefinida.";
  }

  unsigned long start = 0, elapsed = 0;

  if (!error) {
    start = micros();
    if (op == "add") {
      result = a + b;
    } else if (op == "sub") {
      result = a - b;
    } else if (op == "mul") {
      result = multiply(a, b);
    } else if (op == "fat") {
      result = factorial(a);
    } else if (op == "div") {
      result = divide(a, b, &remainder);
    } else {
      error = true;
      errMsg = "Operação desconhecida: " + op;
    }
    elapsed = micros() - start;
  }

  int leds4 = result & 0x0F;
  if (!error) atualizarLEDs(leds4);

  int bits = bitsNeeded(result);
  String binC2 = toC2(result, bits);
  int leds4AsC2 = (leds4 & 0x8) ? leds4 - 16 : leds4;

  String resp = "{";
  resp += "\"a\":" + String(a) + ",";
  resp += "\"b\":" + String(b) + ",";
  resp += "\"op\":\"" + op + "\",";
  resp += "\"result\":" + String(result) + ",";
  resp += "\"remainder\":" + String(remainder) + ",";
  resp += "\"bin_c2\":\"" + binC2 + "\",";
  resp += "\"bits_used\":" + String(bits) + ",";
  resp += "\"leds_4bit\":\"" + toC2(leds4, 4) + "\",";
  resp += "\"leds_4bit_as_c2\":" + String(leds4AsC2) + ",";
  resp += "\"time_us\":" + String(elapsed) + ",";
  resp += "\"error\":" + String(error ? "true" : "false") + ",";
  resp += "\"errMsg\":\"" + errMsg + "\"";
  resp += "}";

  Serial.printf("[CALC] op=%s a=%d b=%d -> result=%d rem=%d (%d bits, %lu us)\n",
                op.c_str(), a, b, result, remainder, bits, elapsed);

  server.send(200, "application/json", resp);
}

// Setup e loop

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(LED_BIT0, OUTPUT);
  pinMode(LED_BIT1, OUTPUT);
  pinMode(LED_BIT2, OUTPUT);
  pinMode(LED_BIT3, OUTPUT);
  atualizarLEDs(0);

  WiFi.softAP(ssid, password);
  IPAddress ip = WiFi.softAPIP();
  Serial.println();
  Serial.println("=== Calculadora — Lab 04 + Divisão (c2, com negativos) ===");
  Serial.printf("SSID: %s, Senha: %s\n", ssid, password);
  Serial.print("IP do AP: ");
  Serial.println(ip);

  server.on("/", handleRoot);
  server.on("/calc", handleCalc);
  server.begin();
  Serial.println("Servidor HTTP iniciado. Acesse http://192.168.4.1/");
}

void loop() {
  server.handleClient();
}
