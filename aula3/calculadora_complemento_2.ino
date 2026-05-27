#include <WiFi.h>
#include <WebServer.h>

const int LED_BIT0 = 2;
const int LED_BIT1 = 3;
const int LED_BIT2 = 4;
const int LED_BIT3 = 5;

const char* ssid     = "Calculadora_grupo_lindo";
const char* password = "12345678";

WebServer server(80);

// ===========================================================================
// Página HTML/JS embarcada
// ===========================================================================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Calculadora 4 bits — PCS3732</title>
  <style>
    body { font-family: Arial, sans-serif; max-width: 540px; margin: 18px auto;
           padding: 12px; background: #f4f4f8; color: #222; }
    h1 { font-size: 1.3em; margin-bottom: 4px; }
    .sub { color: #555; font-size: 0.9em; margin-bottom: 14px; }
    .row { margin: 10px 0; display: flex; align-items: center; gap: 10px; }
    label { display: inline-block; width: 110px; font-weight: bold; }
    input[type="number"] { font-family: monospace; font-size: 1.2em; padding: 6px;
            width: 80px; text-align: center; border: 1px solid #aaa;
            border-radius: 4px; }
    .binview { font-family: monospace; color: #555; font-size: 0.95em; }
    button { font-size: 1em; padding: 10px 22px; margin: 6px 4px;
             cursor: pointer; border: 0; border-radius: 6px;
             background: #2563eb; color: white; font-weight: bold; }
    button:hover { background: #1d4ed8; }
    .leds { display: flex; gap: 14px; justify-content: center; margin: 18px 0; }
    .ledbox { text-align: center; }
    .led { width: 44px; height: 44px; border-radius: 50%; background: #333;
           border: 2px solid #555; margin: 0 auto; }
    .led.on { background: #4ade80; box-shadow: 0 0 16px #4ade80; }
    .ledlabel { font-size: 0.75em; color: #666; margin-top: 4px; }
    .resultado { background: #fff; padding: 12px; border-radius: 8px;
                 margin-top: 12px; border: 1px solid #ddd; }
    .resultado .linha { margin: 4px 0; }
    .resultado .destaque { font-size: 1.15em; margin-top: 10px; padding-top: 8px;
                           border-top: 1px solid #eee; }
    .overflow { background: #fee2e2; color: #b91c1c; font-weight: bold;
                padding: 10px; border-radius: 6px; margin-top: 10px;
                border: 2px solid #b91c1c; }
    .erro { color: #b91c1c; font-weight: bold; }
    code { background: #eef; padding: 1px 4px; border-radius: 3px; }
  </style>
</head>
<body>
  <h1>Calculadora Binaria 4 bits</h1>
  <div class="sub">Complemento de 2 - intervalo valido: <b>-8</b> a <b>+7</b></div>

  <div class="row">
    <label>Operando A:</label>
    <input id="a" type="number" min="-8" max="7" step="1" value="3">
    <span class="binview" id="aBin">= 0011</span>
  </div>
  <div class="row">
    <label>Operando B:</label>
    <input id="b" type="number" min="-8" max="7" step="1" value="2">
    <span class="binview" id="bBin">= 0010</span>
  </div>

  <div class="row" style="justify-content: center;">
    <button onclick="calcular('add')">SOMA  (A + B)</button>
    <button onclick="calcular('sub')">SUB  (A &minus; B)</button>
  </div>

  <div class="leds">
    <div class="ledbox"><div class="led" id="led3"></div><div class="ledlabel">b3 (MSB)</div></div>
    <div class="ledbox"><div class="led" id="led2"></div><div class="ledlabel">b2</div></div>
    <div class="ledbox"><div class="led" id="led1"></div><div class="ledlabel">b1</div></div>
    <div class="ledbox"><div class="led" id="led0"></div><div class="ledlabel">b0 (LSB)</div></div>
  </div>

  <div class="resultado" id="resultado">Digite A e B e clique numa operacao.</div>

  <p style="font-size: 0.8em; color: #666; margin-top: 16px;">
    <b>Nota:</b> O JavaScript converte o decimal para binario em complemento de 2 e envia para o ESP32.
    Toda a <b>aritmetica</b> e executada em C no ESP32, conforme exigido pelo enunciado.
  </p>

  <script>
    function decToC2Bin4(n) {
      const v = (n & 0xF);
      return v.toString(2).padStart(4, '0');
    }

    function refreshBin() {
      const a = parseInt(document.getElementById('a').value, 10);
      const b = parseInt(document.getElementById('b').value, 10);
      document.getElementById('aBin').textContent = Number.isInteger(a) && a >= -8 && a <= 7
        ? '= ' + decToC2Bin4(a) : '= ----';
      document.getElementById('bBin').textContent = Number.isInteger(b) && b >= -8 && b <= 7
        ? '= ' + decToC2Bin4(b) : '= ----';
    }
    document.getElementById('a').addEventListener('input', refreshBin);
    document.getElementById('b').addEventListener('input', refreshBin);

    async function calcular(op) {
      const aRaw = document.getElementById('a').value.trim();
      const bRaw = document.getElementById('b').value.trim();
      const div = document.getElementById('resultado');

      const a = parseInt(aRaw, 10);
      const b = parseInt(bRaw, 10);
      if (!Number.isInteger(a) || a < -8 || a > 7 ||
          !Number.isInteger(b) || b < -8 || b > 7) {
        div.innerHTML = '<span class="erro">Entrada invalida! A e B devem ser inteiros no intervalo [-8, +7].</span>';
        return;
      }

      const aBin = decToC2Bin4(a);
      const bBin = decToC2Bin4(b);

      const esperado = (op === 'add') ? (a + b) : (a - b);
      const opSimbolo = (op === 'add') ? '+' : '\u2212';

      try {
        const resp = await fetch(`/calc?a=${aBin}&b=${bBin}&op=${op}`);
        const j = await resp.json();
        atualizarLeds(j.bin);

        let html = '';
        html += `<div class="linha"><b>A</b> = ${a} &nbsp;(<code>${j.a_bin}</code>)</div>`;
        html += `<div class="linha"><b>B</b> = ${b} &nbsp;(<code>${j.b_bin}</code>)</div>`;
        html += `<div class="linha"><b>Esperado matematicamente:</b> ${a} ${opSimbolo} ${b} = <b>${esperado}</b></div>`;
        html += `<div class="destaque"><b>Resultado nos 4 bits:</b> <code>${j.bin}</code> = <b>${j.dec}</b></div>`;

        if (j.overflow) {
          html += `<div class="overflow">&#9888; OVERFLOW! O resultado matematico (${esperado}) esta fora do intervalo [-8, +7].<br>`;
          html += `Os 4 bits "rolaram" e mostram ${j.dec} em vez de ${esperado}.</div>`;
        }
        div.innerHTML = html;
      } catch (e) {
        div.innerHTML = '<span class="erro">Erro ao comunicar com o ESP32: ' + e + '</span>';
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

// ===========================================================================
// Logica de calculo
// ===========================================================================

int parseBin4(const String &s) {
  return (int)strtol(s.c_str(), NULL, 2) & 0x0F;
}

int signExtend4(int v) {
  v &= 0x0F;
  return (v & 0x08) ? (v - 16) : v;
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

// ===========================================================================
// Handlers HTTP
// ===========================================================================

void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void handleCalc() {
  String paramA = server.arg("a");
  String paramB = server.arg("b");
  String op     = server.arg("op");

  int valA = parseBin4(paramA);
  int valB = parseBin4(paramB);

  int sA = signExtend4(valA);
  int sB = signExtend4(valB);
  int bruto;
  if (op == "add") {
    bruto = sA + sB;
  } else {
    bruto = sA - sB;
  }

  int resMasked = bruto & 0x0F;

  int signA = (valA >> 3) & 1;
  int signB = (valB >> 3) & 1;
  int signR = (resMasked >> 3) & 1;
  bool overflow;
  if (op == "add") {
    overflow = (signA == signB) && (signR != signA);
  } else {
    overflow = (signA != signB) && (signR != signA);
  }

  atualizarLEDs(resMasked);

  int resDec = signExtend4(resMasked);
  String resp = "{";
  resp += "\"a_bin\":\""   + toBin4(valA)      + "\",";
  resp += "\"a_dec\":"     + String(sA)        + ",";
  resp += "\"b_bin\":\""   + toBin4(valB)      + "\",";
  resp += "\"b_dec\":"     + String(sB)        + ",";
  resp += "\"bin\":\""     + toBin4(resMasked) + "\",";
  resp += "\"dec\":"       + String(resDec)    + ",";
  resp += "\"overflow\":"  + String(overflow ? "true" : "false");
  resp += "}";

  Serial.printf("[CALC] %s(%d) %s %s(%d) = %s(%d) OV=%d\n",
                toBin4(valA).c_str(), sA,
                (op == "add" ? "+" : "-"),
                toBin4(valB).c_str(), sB,
                toBin4(resMasked).c_str(), resDec,
                overflow ? 1 : 0);

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
  Serial.println("=== Calculadora 4 bits - Complemento de 2 ===");
  Serial.printf("SSID: %s\n", ssid);
  Serial.printf("Senha: %s\n", password);
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