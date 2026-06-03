/*
 * PCS3732 — Laboratório 04
 * Expansão da Calculadora 4 bits: multiplicação e fatorial
 *
 * Requisitos atendidos:
 *   RF1: Operação de multiplicação (por adições sucessivas — sem usar '*').
 *   RF2: Operação de fatorial (iterativo).
 *   RF3: Regressão — soma e subtração mantidas.
 *   RNF1: Operandos expandidos para 16 bits (0..65535).
 *   RNF2: Toda a lógica em C no microcontrolador.
 *   RNF3: Tempo de execução medido com micros() e retornado no JSON.
 *
 * Pinagem ESP32-C3 (mesma do Lab 03):
 *   GPIO 2 -> LED bit 0 (LSB)
 *   GPIO 3 -> LED bit 1
 *   GPIO 4 -> LED bit 2
 *   GPIO 5 -> LED bit 3 (MSB dos 4 LSB do resultado)
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

// Limites para evitar overflow do int32:
// - multiplicacao por adicoes sucessivas: a*b deve caber em int (~2.1B)
// - fatorial: 12! = 479.001.600 cabe; 13! estoura
const int MUL_MAX_OPERAND = 65535;
const int FAT_MAX = 12;

// Multiplicacao por adicoes sucessivas
int multiply(int a, int b) {
  int result = 0;
  for (int i = 0; i < b; i++) {
    result += a;
  }
  return result;
}

// Fatorial iterativo.
int factorial(int n) {
  if (n <= 1) return 1;
  int result = 1;
  for (int i = 2; i <= n; i++) {
    result *= i;
  }
  return result;
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

// HTML/JS embarcado
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Calculadora — Lab 04</title>
  <style>
    body { font-family: Arial, sans-serif; max-width: 560px; margin: 18px auto;
           padding: 12px; background: #f4f4f8; color: #222; }
    h1 { font-size: 1.3em; margin-bottom: 4px; }
    .sub { color: #555; font-size: 0.9em; margin-bottom: 14px; }
    .row { margin: 10px 0; display: flex; align-items: center; gap: 10px; }
    label { display: inline-block; width: 110px; font-weight: bold; }
    input[type="number"] { font-family: monospace; font-size: 1.15em; padding: 6px;
            width: 100px; text-align: center; border: 1px solid #aaa;
            border-radius: 4px; }
    .binview { font-family: monospace; color: #555; font-size: 0.9em; }
    .ops { display: grid; grid-template-columns: 1fr 1fr; gap: 8px; margin: 16px 0; }
    button { font-size: 0.98em; padding: 12px; cursor: pointer; border: 0;
             border-radius: 6px; background: #2563eb; color: white; font-weight: bold; }
    button:hover { background: #1d4ed8; }
    button.special { background: #7c3aed; }
    button.special:hover { background: #6d28d9; }
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
    .erro { color: #b91c1c; font-weight: bold; }
    code { background: #eef; padding: 1px 4px; border-radius: 3px;
           font-family: Consolas, monospace; }
  </style>
</head>
<body>
  <h1>Calculadora Binaria — Lab 04</h1>
  <div class="sub">Operacoes: soma, subtracao, multiplicacao e fatorial. LEDs mostram os 4 bits menos significativos.</div>

  <div class="row">
    <label>Operando A:</label>
    <input id="a" type="number" min="0" max="65535" step="1" value="3">
    <span class="binview" id="aBin">= 0000 0011</span>
  </div>
  <div class="row">
    <label>Operando B:</label>
    <input id="b" type="number" min="0" max="65535" step="1" value="2">
    <span class="binview" id="bBin">= 0000 0010</span>
  </div>

  <div class="ops">
    <button onclick="calcular('add')">SOMA  (A + B)</button>
    <button onclick="calcular('sub')">SUB  (A &minus; B)</button>
    <button onclick="calcular('mul')">MULT  (A &times; B)</button>
    <button class="special" onclick="calcular('fat')">FATORIAL  (A!)</button>
  </div>

  <div class="leds">
    <div class="ledbox"><div class="led" id="led3"></div><div class="ledlabel">b3</div></div>
    <div class="ledbox"><div class="led" id="led2"></div><div class="ledlabel">b2</div></div>
    <div class="ledbox"><div class="led" id="led1"></div><div class="ledlabel">b1</div></div>
    <div class="ledbox"><div class="led" id="led0"></div><div class="ledlabel">b0</div></div>
  </div>

  <div class="resultado" id="resultado">Escolha A, B e clique em uma operacao.</div>

  <p style="font-size: 0.78em; color: #666; margin-top: 16px;">
    <b>Nota:</b> a aritmetica e executada em C no ESP32. O JavaScript apenas envia
    a requisicao e exibe o resultado. O tempo mostrado e medido com <code>micros()</code>
    no firmware.
  </p>

  <script>
    function toBin(v, bits) {
      if (v < 0) v = (v >>> 0);
      const s = v.toString(2).padStart(bits, '0').slice(-bits);
      // separador a cada 4 bits, da direita para a esquerda
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
        div.innerHTML = '<span class="erro">A deve ser inteiro &ge; 0.</span>';
        return;
      }
      if (op !== 'fat' && (!Number.isInteger(b) || b < 0)) {
        div.innerHTML = '<span class="erro">B deve ser inteiro &ge; 0.</span>';
        return;
      }

      try {
        const resp = await fetch(`/calc?a=${a}&b=${b}&op=${op}`);
        const j = await resp.json();

        if (j.error) {
          div.innerHTML = '<span class="erro">' + j.errMsg + '</span>';
          return;
        }

        atualizarLeds(j.leds_4bit);

        const sym = { add: '+', sub: '\u2212', mul: '\u00d7' };
        const expr = op === 'fat' ? `${a}!` : `${a} ${sym[op]} ${b}`;

        let html = '';
        html += `<div class="linha"><b>Operacao:</b> ${expr}</div>`;
        html += `<div class="destaque"><b>Resultado:</b> ${j.result} &nbsp; <code>${j.bin_full}</code></div>`;
        html += `<div class="linha"><b>LEDs (4 LSB):</b> <code>${j.leds_4bit}</code></div>`;

        if (j.overflow_4bit) {
          html += `<div class="overflow">&#9888; Resultado (${j.result}) nao cabe em 4 bits. Os LEDs mostram apenas os 4 bits menos significativos.</div>`;
        }

        html += `<div class="bench">&#9201; Tempo de execucao no ESP32: <b>${j.time_us} &micro;s</b></div>`;

        div.innerHTML = html;
      } catch (e) {
        div.innerHTML = '<span class="erro">Erro de comunicacao: ' + e + '</span>';
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

// Handlers HTTP

void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void handleCalc() {
  int a = server.arg("a").toInt();
  int b = server.arg("b").toInt();
  String op = server.arg("op");

  int result = 0;
  bool error = false;
  String errMsg = "";

  // Validacoes especificas por operacao
  if (op == "mul" && (a > MUL_MAX_OPERAND || b > MUL_MAX_OPERAND)) {
    error = true;
    errMsg = "Operandos limitados a 65535 para multiplicacao.";
  } else if (op == "fat" && a > FAT_MAX) {
    error = true;
    errMsg = "Fatorial limitado a n=" + String(FAT_MAX) + " (overflow do int32).";
  }

  unsigned long start = 0, elapsed = 0;

  if (!error) {
    // Medicao de tempo (RNF3): micros() tem resolucao de ~1us no ESP32-C3
    start = micros();
    if (op == "add") {
      result = a + b;
    } else if (op == "sub") {
      result = a - b;
    } else if (op == "mul") {
      result = multiply(a, b);          // por adicoes sucessivas
    } else if (op == "fat") {
      result = factorial(a);            // iterativo
    } else {
      error = true;
      errMsg = "Operacao desconhecida: " + op;
    }
    elapsed = micros() - start;
  }

  // Saida nos LEDs: 4 LSB do resultado (mascaramento conforme Lab 03)
  int leds = result & 0x0F;
  if (!error) atualizarLEDs(leds);

  // Sinaliza se resultado nao cabe em 4 bits (para LEDs)
  bool overflow_4bit = (result < 0) || (result > 15);

  // Resposta JSON
  // Representa o resultado em 16 bits (suficiente para mostrar ate 65535;
  // valores maiores aparecem truncados visualmente mas o campo "result" tem o valor completo)
  unsigned int resU = (unsigned int)result;
  String binFull = toBinN(resU & 0xFFFF, 16);
  // separador a cada 4 bits para leitura
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
  resp += "\"bin_full\":\"" + binFmt + "\",";
  resp += "\"leds_4bit\":\"" + toBinN(leds, 4) + "\",";
  resp += "\"overflow_4bit\":" + String(overflow_4bit ? "true" : "false") + ",";
  resp += "\"time_us\":" + String(elapsed) + ",";
  resp += "\"error\":" + String(error ? "true" : "false") + ",";
  resp += "\"errMsg\":\"" + errMsg + "\"";
  resp += "}";

  Serial.printf("[CALC] op=%s a=%d b=%d -> result=%d (%lu us) ov4=%d\n",
                op.c_str(), a, b, result, elapsed, overflow_4bit ? 1 : 0);

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
  Serial.println("=== Calculadora 4 bits — Lab 04 (multiplicacao + fatorial) ===");
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