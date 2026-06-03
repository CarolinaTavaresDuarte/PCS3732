/*
 * PCS3732 — Laboratório 04 — DESAFIO ADICIONAL
 * Versão expandida com operação de DIVISÃO inteira
 *
 * Adiciona ao sketch principal:
 *   - Operação 'div' (divisão inteira)
 *   - Tratamento de exceção: divisão por zero retorna erro
 *
 * Mantém todas as operações anteriores (soma, subtração, multiplicação,
 * fatorial) com a mesma interface e benchmarking.
 *
 * Pinagem ESP32-C3: GPIOs 2, 3, 4, 5 (LSB - MSB)
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

const int MUL_MAX_OPERAND = 65535;
const int FAT_MAX = 12;

int multiply(int a, int b) {
  int result = 0;
  for (int i = 0; i < b; i++) result += a;
  return result;
}

int factorial(int n) {
  if (n <= 1) return 1;
  int result = 1;
  for (int i = 2; i <= n; i++) result *= i;
  return result;
}

// Divisao inteira com tratamento de excecao.
// Retorna o quociente inteiro e o resto pelo parametro out_remainder.
// Se b == 0, retorna 0 e seta a flag de erro (tratada pelo handler).
int divide(int a, int b, int *out_remainder) {
  if (b == 0) {
    *out_remainder = 0;
    return 0;
  }
  *out_remainder = a % b;
  return a / b;
}

// Utilitarios

void atualizarLEDs(int v) {
  digitalWrite(LED_BIT0, (v >> 0) & 1);
  digitalWrite(LED_BIT1, (v >> 1) & 1);
  digitalWrite(LED_BIT2, (v >> 2) & 1);
  digitalWrite(LED_BIT3, (v >> 3) & 1);
}

String toBinN(unsigned int v, int bits) {
  String s = "";
  for (int i = bits - 1; i >= 0; i--) s += ((v >> i) & 1) ? '1' : '0';
  return s;
}

// HTML/JS embarcado (com 5 botoes: SOMA, SUB, MULT, FATORIAL, DIV)
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Calculadora — Lab 04 + Divisao</title>
  <style>
    body { font-family: Arial, sans-serif; max-width: 580px; margin: 18px auto;
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
    button { font-size: 0.92em; padding: 11px 8px; cursor: pointer; border: 0;
             border-radius: 6px; background: #2563eb; color: white; font-weight: bold; }
    button:hover { background: #1d4ed8; }
    button.special { background: #7c3aed; }
    button.special:hover { background: #6d28d9; }
    button.danger { background: #db2777; }
    button.danger:hover { background: #be185d; }
    .leds { display: flex; gap: 12px; justify-content: center; margin: 18px 0; }
    .ledbox { text-align: center; }
    .led { width: 40px; height: 40px; border-radius: 50%; background: #333;
           border: 2px solid #555; margin: 0 auto; }
    .led.on { background: #4ade80; box-shadow: 0 0 14px #4ade80; }
    .ledlabel { font-size: 0.7em; color: #666; margin-top: 4px; }
    .resultado { background: #fff; padding: 12px; border-radius: 8px;
                 margin-top: 12px; border: 1px solid #ddd; }
    .resultado .linha { margin: 4px 0; }
    .resultado .destaque { font-size: 1.15em; margin-top: 8px; padding-top: 8px;
                           border-top: 1px solid #eee; }
    .bench { background: #eef6ff; padding: 8px 10px; border-radius: 6px;
             margin-top: 8px; font-size: 0.92em; color: #1e40af; }
    .overflow { background: #fef3c7; color: #92400e; font-weight: bold;
                padding: 8px 10px; border-radius: 6px; margin-top: 8px;
                border-left: 4px solid #f59e0b; font-size: 0.9em; }
    .erro { background: #fee2e2; color: #b91c1c; font-weight: bold;
            padding: 10px; border-radius: 6px; border: 2px solid #b91c1c; }
    code { background: #eef; padding: 1px 4px; border-radius: 3px;
           font-family: Consolas, monospace; }
  </style>
</head>
<body>
  <h1>Calculadora Binaria — Lab 04 + Divisao</h1>
  <div class="sub">Operacoes: soma, subtracao, multiplicacao, fatorial e divisao inteira.</div>

  <div class="row">
    <label>Operando A:</label>
    <input id="a" type="number" min="0" max="65535" step="1" value="10">
    <span class="binview" id="aBin">= 0000 1010</span>
  </div>
  <div class="row">
    <label>Operando B:</label>
    <input id="b" type="number" min="0" max="65535" step="1" value="2">
    <span class="binview" id="bBin">= 0000 0010</span>
  </div>

  <div class="ops">
    <button onclick="calcular('add')">SOMA</button>
    <button onclick="calcular('sub')">SUB</button>
    <button onclick="calcular('mul')">MULT</button>
    <button class="special" onclick="calcular('fat')">FATORIAL (A!)</button>
    <button class="danger" onclick="calcular('div')">DIV (A &divide; B)</button>
  </div>

  <div class="leds">
    <div class="ledbox"><div class="led" id="led3"></div><div class="ledlabel">b3</div></div>
    <div class="ledbox"><div class="led" id="led2"></div><div class="ledlabel">b2</div></div>
    <div class="ledbox"><div class="led" id="led1"></div><div class="ledlabel">b1</div></div>
    <div class="ledbox"><div class="led" id="led0"></div><div class="ledlabel">b0</div></div>
  </div>

  <div class="resultado" id="resultado">Escolha A, B e clique em uma operacao.</div>

  <p style="font-size: 0.78em; color: #666; margin-top: 16px;">
    <b>Nota:</b> aritmetica em C no ESP32; tempo medido com <code>micros()</code>.
    A divisao inclui tratamento de excecao para divisor zero.
  </p>

  <script>
    function toBin(v, bits) {
      if (v < 0) v = (v >>> 0);
      const s = v.toString(2).padStart(bits, '0').slice(-bits);
      return s.replace(/(.{4})(?=.)/g, '$1 ');
    }
    function refreshBin() {
      const a = parseInt(document.getElementById('a').value, 10) || 0;
      const b = parseInt(document.getElementById('b').value, 10) || 0;
      document.getElementById('aBin').textContent = '= ' + toBin(a, 8);
      document.getElementById('bBin').textContent = '= ' + toBin(b, 8);
    }
    document.getElementById('a').addEventListener('input', refreshBin);
    document.getElementById('b').addEventListener('input', refreshBin);

    async function calcular(op) {
      const a = parseInt(document.getElementById('a').value, 10);
      const b = parseInt(document.getElementById('b').value, 10);
      const div = document.getElementById('resultado');

      if (!Number.isInteger(a) || a < 0) {
        div.innerHTML = '<div class="erro">A deve ser inteiro &ge; 0.</div>';
        return;
      }
      if (op !== 'fat' && (!Number.isInteger(b) || b < 0)) {
        div.innerHTML = '<div class="erro">B deve ser inteiro &ge; 0.</div>';
        return;
      }

      try {
        const resp = await fetch(`/calc?a=${a}&b=${b}&op=${op}`);
        const j = await resp.json();

        if (j.error) {
          div.innerHTML = '<div class="erro">&#9888; ' + j.errMsg + '</div>';
          return;
        }

        atualizarLeds(j.leds_4bit);

        const sym = { add: '+', sub: '\u2212', mul: '\u00d7', div: '\u00f7' };
        let expr;
        if (op === 'fat') expr = `${a}!`;
        else expr = `${a} ${sym[op]} ${b}`;

        let html = '';
        html += `<div class="linha"><b>Operacao:</b> ${expr}</div>`;
        html += `<div class="destaque"><b>Resultado:</b> ${j.result} &nbsp; <code>${j.bin_full}</code></div>`;
        if (op === 'div') {
          html += `<div class="linha"><b>Resto:</b> ${j.remainder}</div>`;
        }
        html += `<div class="linha"><b>LEDs (4 LSB):</b> <code>${j.leds_4bit}</code></div>`;

        if (j.overflow_4bit) {
          html += `<div class="overflow">&#9888; Resultado (${j.result}) nao cabe em 4 bits. LEDs mostram apenas os 4 bits menos significativos.</div>`;
        }

        html += `<div class="bench">&#9201; Tempo de execucao no ESP32: <b>${j.time_us} &micro;s</b></div>`;

        div.innerHTML = html;
      } catch (e) {
        div.innerHTML = '<div class="erro">Erro de comunicacao: ' + e + '</div>';
      }
    }

    function atualizarLeds(bin) {
      const padded = bin.padStart(4, '0');
      for (let i = 0; i < 4; i++) {
        const led = document.getElementById('led' + (3 - i));
        if (padded[i] === '1') led.classList.add('on');
        else led.classList.remove('on');
      }
    }

    refreshBin();
  </script>
</body>
</html>
)rawliteral";

// Handler de calculo

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

  if (op == "mul" && (a > MUL_MAX_OPERAND || b > MUL_MAX_OPERAND)) {
    error = true;
    errMsg = "Operandos limitados a 65535 para multiplicacao.";
  } else if (op == "fat" && a > FAT_MAX) {
    error = true;
    errMsg = "Fatorial limitado a n=" + String(FAT_MAX) + " (overflow do int32).";
  } else if (op == "div" && b == 0) {
    error = true;
    errMsg = "Divisao por zero indefinida.";
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
      errMsg = "Operacao desconhecida: " + op;
    }
    elapsed = micros() - start;
  }

  int leds = result & 0x0F;
  if (!error) atualizarLEDs(leds);

  bool overflow_4bit = (result < 0) || (result > 15);

  unsigned int resU = (unsigned int)result;
  String binFull = toBinN(resU & 0xFFFF, 16);
  String binFmt = "";
  for (int i = 0; i < 16; i++) {
    binFmt += binFull[i];
    if (i % 4 == 3 && i < 15) binFmt += ' ';
  }

  String resp = "{";
  resp += "\"a\":" + String(a) + ",";
  resp += "\"b\":" + String(b) + ",";
  resp += "\"op\":\"" + op + "\",";
  resp += "\"result\":" + String(result) + ",";
  resp += "\"remainder\":" + String(remainder) + ",";
  resp += "\"bin_full\":\"" + binFmt + "\",";
  resp += "\"leds_4bit\":\"" + toBinN(leds, 4) + "\",";
  resp += "\"overflow_4bit\":" + String(overflow_4bit ? "true" : "false") + ",";
  resp += "\"time_us\":" + String(elapsed) + ",";
  resp += "\"error\":" + String(error ? "true" : "false") + ",";
  resp += "\"errMsg\":\"" + errMsg + "\"";
  resp += "}";

  Serial.printf("[CALC] op=%s a=%d b=%d -> result=%d rem=%d (%lu us)\n",
                op.c_str(), a, b, result, remainder, elapsed);

  server.send(200, "application/json", resp);
}

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
  Serial.println("=== Calculadora 4 bits — Lab 04 + Divisao ===");
  Serial.printf("SSID: %s, Senha: %s\n", ssid, password);
  Serial.print("IP do AP: ");
  Serial.println(ip);

  server.on("/", handleRoot);
  server.on("/calc", handleCalc);
  server.begin();
  Serial.println("Servidor HTTP iniciado.");
}

void loop() {
  server.handleClient();
}
