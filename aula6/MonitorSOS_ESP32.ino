/*
 * PCS3732 — Laboratório 06
 * Sistema de Monitoramento Inteligente: ADC (LDR) + Interrupção (Botão SOS)
 * + Desafio: Semáforo Inteligente com Modo Noturno e Botão de Pedestre
 *
 * Pinagem ESP32-C3:
 *   GPIO 0 - AO do módulo LDR (entrada analógica ADC1_CH0)
 *   GPIO 4 - LED externo (anodo via resistor 330 Ohm)
 *   GPIO 6 - Botão SOS / pedestre (INPUT_PULLUP; outro terminal no GND)
 *   GPIO 8 - LED RGB built-in (WS2812). Já na placa.
 *   3V3 - VCC do módulo LDR
 *   GND - GND do módulo LDR + perna oposta do botão
 */

#include <WiFi.h>
#include <WebServer.h>

const int LDR_PIN     = 0;
const int LED_EXT_PIN = 4;
const int BUTTON_PIN  = 6;
const int RGB_LED_PIN = 8;

const int ADC_BITS = 12;
const int ADC_MAX  = (1 << ADC_BITS) - 1;
const uint8_t RGB_BRIGHT = 80;

const unsigned long YELLOW_BLINK_HALF_PERIOD_MS = 1000;
const unsigned long NIGHT_BLINK_HALF_PERIOD_MS  = 500;
const unsigned long EMERGENCY_DURATION_MS       = 3000;
const unsigned long DEBOUNCE_MS                 = 50;
const unsigned long TRAFFIC_GREEN_MS  = 5000;
const unsigned long TRAFFIC_YELLOW_MS = 2000;
const unsigned long TRAFFIC_RED_MS    = 5000;

const char* ssid     = "MonitorSOS_grupo_lindo";
const char* password = "12345678";

WebServer server(80);

enum SystemMode { MODE_MONITOR, MODE_TRAFFIC };
SystemMode currentMode = MODE_MONITOR;

int  currentLDR   = 0;
int  ldrThreshold = 2000;
bool lowLight     = false;

volatile bool          buttonTrigger     = false;
volatile unsigned long lastInterruptTime = 0;

unsigned long emergencyEndTime = 0;
bool          emergencyActive  = false;
bool pedestrianPending = false;

unsigned long lastBlinkToggle = 0;
bool          blinkState      = false;

enum TrafficState { TR_GREEN, TR_YELLOW, TR_RED };
TrafficState  trafficState      = TR_GREEN;
unsigned long lastTrafficChange = 0;

void IRAM_ATTR onButtonPressed() {
  unsigned long now = millis();
  if (now - lastInterruptTime > DEBOUNCE_MS) {
    buttonTrigger     = true;
    lastInterruptTime = now;
  }
}

void setRgbColor(uint8_t r, uint8_t g, uint8_t b) {
  rgbLedWrite(RGB_LED_PIN, r, g, b);
}
void rgbOff()    { setRgbColor(0, 0, 0); }
void rgbYellow() { setRgbColor(RGB_BRIGHT, RGB_BRIGHT, 0); }
void rgbRed()    { setRgbColor(RGB_BRIGHT, 0, 0); }
void rgbGreen()  { setRgbColor(0, RGB_BRIGHT, 0); }

const char* trafficStateName() {
  switch (trafficState) {
    case TR_GREEN:  return "VERDE";
    case TR_YELLOW: return "AMARELO";
    case TR_RED:    return "VERMELHO";
  }
  return "?";
}

void updateMonitorMode() {
  unsigned long now = millis();
  currentLDR = analogRead(LDR_PIN);
  lowLight   = (currentLDR > ldrThreshold);

  if (emergencyActive && now > emergencyEndTime) {
    emergencyActive = false;
  }

  if (emergencyActive) {
    rgbRed();
  }
  else if (lowLight) {
    if (now - lastBlinkToggle >= YELLOW_BLINK_HALF_PERIOD_MS) {
      blinkState      = !blinkState;
      lastBlinkToggle = now;
    }
    if (blinkState) rgbYellow();
    else            rgbOff();
  }
  else {
    rgbOff();
  }
}

void updateTrafficMode() {
  unsigned long now = millis();
  currentLDR = analogRead(LDR_PIN);
  lowLight   = (currentLDR > ldrThreshold);

  if (lowLight) {
    if (now - lastBlinkToggle >= NIGHT_BLINK_HALF_PERIOD_MS) {
      blinkState      = !blinkState;
      lastBlinkToggle = now;
    }
    if (blinkState) rgbYellow();
    else            rgbOff();
    return;
  }

  unsigned long elapsed = now - lastTrafficChange;

  if (pedestrianPending && trafficState == TR_GREEN) {
    if (elapsed < TRAFFIC_GREEN_MS - 500) {
      lastTrafficChange = now - (TRAFFIC_GREEN_MS - 500);
    }
    pedestrianPending = false;
  }

  unsigned long duration = 0;
  switch (trafficState) {
    case TR_GREEN:  duration = TRAFFIC_GREEN_MS;  break;
    case TR_YELLOW: duration = TRAFFIC_YELLOW_MS; break;
    case TR_RED:    duration = TRAFFIC_RED_MS;    break;
  }

  if (elapsed > duration) {
    switch (trafficState) {
      case TR_GREEN:  trafficState = TR_YELLOW; break;
      case TR_YELLOW: trafficState = TR_RED;    break;
      case TR_RED:    trafficState = TR_GREEN;  break;
    }
    lastTrafficChange = now;
  }

  switch (trafficState) {
    case TR_GREEN:  rgbGreen();  break;
    case TR_YELLOW: rgbYellow(); break;
    case TR_RED:    rgbRed();    break;
  }
}

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Monitor SOS — ESP32-C3</title>
  <style>
    * { box-sizing: border-box; }
    body { font-family: Arial, sans-serif; max-width: 580px; margin: 14px auto;
           padding: 12px; background: #f4f4f8; color: #222; }
    h1 { font-size: 1.4em; margin: 0 0 4px 0; color: #1f3864; }
    .sub { color: #555; font-size: 0.9em; margin-bottom: 16px; }
    .card { background: #fff; padding: 14px 16px; border-radius: 8px;
            margin: 10px 0; border: 1px solid #ddd;
            box-shadow: 0 1px 3px rgba(0,0,0,0.06); }
    .card h2 { margin: 0 0 12px 0; font-size: 1.1em; color: #1f3864; }
    .row { display: flex; align-items: center; gap: 12px; margin: 8px 0; }
    .row label { min-width: 90px; font-size: 0.88em; color: #444; }
    input[type="range"] { flex: 1; }
    input[type="number"] { width: 90px; padding: 6px 8px; font-size: 1em;
                            border: 1px solid #bbb; border-radius: 5px;
                            font-family: Consolas, monospace; }
    .barra-bg { flex: 1; height: 18px; background: #eee; border-radius: 4px; overflow: hidden; }
    .barra-fg { height: 100%; background: linear-gradient(90deg, #1e3a8a, #3b82f6, #fbbf24);
                width: 0%; transition: width 0.3s; }
    .estado-card { padding: 16px; border-radius: 8px; text-align: center; font-size: 1.1em;
                   font-weight: bold; }
    .estado-NORMAL  { background: #d1fae5; color: #065f46; }
    .estado-BAIXA   { background: #fef3c7; color: #92400e; animation: piscaY 2s infinite; }
    .estado-EMERG   { background: #fee2e2; color: #991b1b; animation: piscaR 1s infinite; }
    @keyframes piscaY { 50% { opacity: 0.5; } }
    @keyframes piscaR { 50% { opacity: 0.6; } }
    .botoes { display: grid; grid-template-columns: repeat(2, 1fr); gap: 8px; margin-top: 10px; }
    .botoes button { padding: 10px; font-size: 0.95em; cursor: pointer; border: 1px solid #ccc;
                     border-radius: 6px; background: #fff; font-weight: bold; }
    .botoes button:hover { background: #eef; }
    .botoes button.ativo { background: #2563eb; color: white; border-color: #2563eb; }
    .sos-btn { width: 100%; padding: 14px; font-size: 1.1em; font-weight: bold;
               border: 2px solid #b91c1c; background: #fee2e2; color: #991b1b;
               border-radius: 6px; cursor: pointer; margin-top: 10px; }
    .sos-btn:active { background: #b91c1c; color: white; }
    .info { font-size: 0.78em; color: #777; margin-top: 12px; text-align: center; }
    .led-vis { width: 60px; height: 60px; border-radius: 50%; margin: 10px auto;
               background: #222; border: 3px solid #111; transition: all 0.3s; }
    .led-yellow { background: #facc15; box-shadow: 0 0 22px #facc15; }
    .led-red    { background: #dc2626; box-shadow: 0 0 22px #dc2626; }
    .led-green  { background: #16a34a; box-shadow: 0 0 22px #16a34a; }
  </style>
</head>
<body>
  <h1>Monitor SOS &mdash; ESP32-C3</h1>
  <div class="sub">ADC (LDR) + Interrup&ccedil;&atilde;o de hardware. LED RGB built-in (GPIO 8).</div>

  <div class="card">
    <h2>Modo de Opera&ccedil;&atilde;o</h2>
    <div class="botoes">
      <button id="modoMonitor" onclick="setMode('monitor')">Monitoramento</button>
      <button id="modoSemaforo" onclick="setMode('traffic')">Sem&aacute;foro</button>
    </div>
  </div>

  <div class="card">
    <h2>LED RGB built-in</h2>
    <div id="ledVis" class="led-vis"></div>
    <div id="estado" class="estado-card estado-NORMAL">NORMAL</div>
  </div>

  <div class="card">
    <h2>Sensor LDR</h2>
    <div class="row">
      <label>Leitura:</label>
      <div class="barra-bg"><div id="barraLDR" class="barra-fg"></div></div>
      <span id="valLDR" style="font-family: Consolas, monospace; font-weight: bold; min-width: 60px; text-align: right;">0</span>
    </div>
    <div class="row">
      <label>Limiar:</label>
      <input id="limiar" type="range" min="0" max="4095" value="2000">
      <input id="limiarN" type="number" min="0" max="4095" value="2000">
    </div>
  </div>

  <button class="sos-btn" onclick="triggerSOS()">&#9888;&#65039; Simular SOS / Pedestre (via web)</button>

  <div class="info" id="info">Aguardando dados...</div>

  <script>
    const estadoEl    = document.getElementById('estado');
    const ledVis      = document.getElementById('ledVis');
    const valLDREl    = document.getElementById('valLDR');
    const barraLDREl  = document.getElementById('barraLDR');
    const limiarEl    = document.getElementById('limiar');
    const limiarNEl   = document.getElementById('limiarN');
    const modoMon     = document.getElementById('modoMonitor');
    const modoSem     = document.getElementById('modoSemaforo');
    const info        = document.getElementById('info');

    function updateUI(j) {
      if (j.mode === 'monitor') { modoMon.classList.add('ativo'); modoSem.classList.remove('ativo'); }
      else                      { modoSem.classList.add('ativo'); modoMon.classList.remove('ativo'); }

      valLDREl.textContent = j.ldr;
      barraLDREl.style.width = (j.ldr * 100 / 4095) + '%';

      if (document.activeElement !== limiarEl && document.activeElement !== limiarNEl) {
        limiarEl.value  = j.threshold;
        limiarNEl.value = j.threshold;
      }

      ledVis.className = 'led-vis';
      if (j.rgb === 'YELLOW') ledVis.classList.add('led-yellow');
      else if (j.rgb === 'RED') ledVis.classList.add('led-red');
      else if (j.rgb === 'GREEN') ledVis.classList.add('led-green');

      estadoEl.className = 'estado-card';
      if (j.mode === 'monitor') {
        if (j.emergency) {
          estadoEl.classList.add('estado-EMERG');
          estadoEl.textContent = '🚨 EMERGÊNCIA (' + j.emergency_remaining_ms + ' ms)';
        } else if (j.low_light) {
          estadoEl.classList.add('estado-BAIXA');
          estadoEl.textContent = '⚠️ BAIXA LUMINOSIDADE';
        } else {
          estadoEl.classList.add('estado-NORMAL');
          estadoEl.textContent = '✅ NORMAL';
        }
      } else {
        if (j.low_light) {
          estadoEl.className = 'estado-card estado-BAIXA';
          estadoEl.textContent = '🌙 MODO NOTURNO (amarelo a 1 Hz)';
        } else {
          estadoEl.className = 'estado-card estado-NORMAL';
          estadoEl.textContent = '🚦 ' + j.traffic_state;
        }
      }

      info.textContent = 'LDR=' + j.ldr + '/4095 | Limiar=' + j.threshold + ' | RGB=' + j.rgb;
    }

    function send(params) {
      return fetch('/set?' + params.toString())
        .then(r => r.json()).then(updateUI)
        .catch(e => info.textContent = 'Erro: ' + e);
    }

    function setMode(m)   { const p = new URLSearchParams(); p.append('mode', m); send(p); }
    function triggerSOS() { const p = new URLSearchParams(); p.append('trigger', '1'); send(p); }
    function setLimiar(v) { const p = new URLSearchParams(); p.append('threshold', v); send(p); }

    limiarEl.addEventListener('change', e => { limiarNEl.value = e.target.value; setLimiar(e.target.value); });
    limiarEl.addEventListener('input',  e => { limiarNEl.value = e.target.value; });
    limiarNEl.addEventListener('change', e => { limiarEl.value = e.target.value; setLimiar(e.target.value); });

    setInterval(() => {
      fetch('/state').then(r => r.json()).then(updateUI).catch(()=>{});
    }, 500);

    fetch('/state').then(r => r.json()).then(updateUI);
  </script>
</body>
</html>
)rawliteral";

const char* currentRgbName() {
  if (currentMode == MODE_MONITOR) {
    if (emergencyActive) return "RED";
    if (lowLight && blinkState) return "YELLOW";
    return "OFF";
  } else {
    if (lowLight) return blinkState ? "YELLOW" : "OFF";
    switch (trafficState) {
      case TR_GREEN:  return "GREEN";
      case TR_YELLOW: return "YELLOW";
      case TR_RED:    return "RED";
    }
  }
  return "OFF";
}

String stateJson() {
  String r = "{";
  r += "\"mode\":\"";
  r += (currentMode == MODE_MONITOR ? "monitor" : "traffic");
  r += "\",";
  r += "\"ldr\":"        + String(currentLDR) + ",";
  r += "\"threshold\":"  + String(ldrThreshold) + ",";
  r += "\"low_light\":"  + String(lowLight ? "true" : "false") + ",";
  r += "\"emergency\":"  + String(emergencyActive ? "true" : "false") + ",";
  r += "\"emergency_remaining_ms\":";
  if (emergencyActive) {
    long rem = (long)emergencyEndTime - (long)millis();
    if (rem < 0) rem = 0;
    r += String(rem);
  } else {
    r += "0";
  }
  r += ",";
  r += "\"traffic_state\":\"" + String(trafficStateName()) + "\",";
  r += "\"rgb\":\"" + String(currentRgbName()) + "\"";
  r += "}";
  return r;
}

void handleRoot()  { server.send_P(200, "text/html; charset=utf-8", INDEX_HTML); }
void handleState() { server.send(200, "application/json", stateJson()); }

void handleSet() {
  if (server.hasArg("mode")) {
    String m = server.arg("mode");
    if (m == "monitor") currentMode = MODE_MONITOR;
    else if (m == "traffic") {
      currentMode = MODE_TRAFFIC;
      trafficState = TR_GREEN;
      lastTrafficChange = millis();
    }
    rgbOff();
    Serial.printf("[MODE] %s\n", m.c_str());
  }
  if (server.hasArg("threshold")) {
    ldrThreshold = constrain(server.arg("threshold").toInt(), 0, ADC_MAX);
    Serial.printf("[THRESHOLD] %d\n", ldrThreshold);
  }
  if (server.hasArg("trigger")) {
    buttonTrigger = true;
    Serial.println("[TRIGGER] simulado via web");
  }
  server.send(200, "application/json", stateJson());
}

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(LED_EXT_PIN, OUTPUT);
  digitalWrite(LED_EXT_PIN, LOW);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  analogReadResolution(ADC_BITS);

  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), onButtonPressed, FALLING);

  rgbOff();

  WiFi.softAP(ssid, password);
  IPAddress ip = WiFi.softAPIP();
  Serial.println();
  Serial.println("=== Lab 06: Monitor SOS + Semaforo (LED RGB built-in) ===");
  Serial.printf("SSID: %s, Senha: %s\n", ssid, password);
  Serial.print("IP do AP: ");
  Serial.println(ip);
  Serial.printf("LDR     -> GPIO %d (ADC1_CH0, %d bits)\n", LDR_PIN, ADC_BITS);
  Serial.printf("LED EXT -> GPIO %d (segue estado do botao)\n", LED_EXT_PIN);
  Serial.printf("BUTTON  -> GPIO %d (INPUT_PULLUP, IRQ FALLING)\n", BUTTON_PIN);
  Serial.printf("LED RGB -> GPIO %d (WS2812 built-in)\n", RGB_LED_PIN);

  server.on("/",      handleRoot);
  server.on("/state", handleState);
  server.on("/set",   handleSet);
  server.begin();
  Serial.println("Servidor HTTP iniciado. Acesse http://192.168.4.1/");

  lastTrafficChange = millis();
}

void loop() {
  server.handleClient();

  digitalWrite(LED_EXT_PIN, digitalRead(BUTTON_PIN) == LOW ? HIGH : LOW);

  if (buttonTrigger) {
    buttonTrigger = false;
    Serial.printf("[ISR-RX] botao pressionado em t=%lu ms\n", lastInterruptTime);
    if (currentMode == MODE_MONITOR) {
      emergencyActive  = true;
      emergencyEndTime = millis() + EMERGENCY_DURATION_MS;
    } else {
      pedestrianPending = true;
    }
  }

  if (currentMode == MODE_MONITOR) updateMonitorMode();
  else                              updateTrafficMode();
}