#include <WiFi.h>
#include <WebServer.h>

const int LED_PIN   = 4;
const int SERVO_PIN = 5;

const int LED_FREQ_INITIAL = 5000;
const int LED_FREQ_MIN     = 1;
const int LED_FREQ_MAX     = 40000;
const int LED_RES_BITS     = 8;
const int LED_MAX_DUTY     = (1 << LED_RES_BITS) - 1;

const int SERVO_FREQ     = 50;
const int SERVO_RES_BITS = 14;
const int SERVO_MIN_DUTY = 410;    // ~0,5 ms -> 0°
const int SERVO_MAX_DUTY = 2048;   // ~2,5 ms -> 180°

const char* ssid     = "ControlePWM_ESP32_GrupoLindo";
const char* password = "12345678";

WebServer server(80);

int  currentLedPercent = 0;
int  currentLedFreq    = LED_FREQ_INITIAL;
bool currentLedBlink   = false;
int  currentServoAngle = 90;

unsigned long lastBlinkToggle = 0;
bool          blinkState      = false;

void atualizarLED() {
  if (currentLedBlink) return;
  int duty = map(currentLedPercent, 0, 100, 0, LED_MAX_DUTY);
  ledcWrite(LED_PIN, duty);
}

void setLedBrightness(int percent) {
  currentLedPercent = constrain(percent, 0, 100);
  atualizarLED();
}

void setLedFrequency(int freq) {
  freq = constrain(freq, LED_FREQ_MIN, LED_FREQ_MAX);
  currentLedFreq = freq;
  ledcChangeFrequency(LED_PIN, freq, LED_RES_BITS);
  atualizarLED();
}

void setLedBlink(bool enabled) {
  currentLedBlink = enabled;
  if (enabled) {
    blinkState = false;
    lastBlinkToggle = millis();
    ledcWrite(LED_PIN, 0);
  } else {
    atualizarLED();
  }
}

void setServoAngle(int angle) {
  angle = constrain(angle, 0, 180);
  int duty = map(angle, 0, 180, SERVO_MIN_DUTY, SERVO_MAX_DUTY);
  ledcWrite(SERVO_PIN, duty);
  currentServoAngle = angle;
}

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Controle PWM — ESP32</title>
  <style>
    * { box-sizing: border-box; }
    body { font-family: Arial, sans-serif; max-width: 540px; margin: 14px auto;
           padding: 12px; background: #f4f4f8; color: #222; }
    h1 { font-size: 1.4em; margin: 0 0 4px 0; color: #1f3864; }
    .sub { color: #555; font-size: 0.9em; margin-bottom: 16px; }
    .card { background: #fff; padding: 14px 16px; border-radius: 8px;
            margin: 10px 0; border: 1px solid #ddd;
            box-shadow: 0 1px 3px rgba(0,0,0,0.06); }
    .card h2 { margin: 0 0 12px 0; font-size: 1.1em; color: #1f3864; }
    .row { display: flex; align-items: center; gap: 12px; margin: 8px 0; }
    .row label { min-width: 80px; font-size: 0.88em; color: #444; }
    input[type="range"] { flex: 1; height: 36px; cursor: pointer; }
    input[type="number"] { width: 100px; padding: 6px 8px; font-size: 1em;
                            border: 1px solid #bbb; border-radius: 5px;
                            font-family: Consolas, monospace; }
    .valor { font-family: Consolas, monospace; font-size: 1.2em; font-weight: bold;
             min-width: 80px; text-align: right; }
    .led-card .valor { color: #d97706; }
    .servo-card .valor { color: #16a34a; }
    .desabilitado { opacity: 0.4; pointer-events: none; }
    .botoes { display: grid; grid-template-columns: repeat(6, 1fr);
              gap: 5px; margin-top: 8px; }
    .botoes-4 { grid-template-columns: repeat(4, 1fr); }
    .botoes button { padding: 8px 4px; font-size: 0.82em; cursor: pointer;
                     border: 1px solid #ccc; border-radius: 5px;
                     background: #fff; font-weight: bold; }
    .botoes button:hover { background: #eef; }
    .botoes button.ativo { background: #2563eb; color: white; border-color: #2563eb; }
    .piscar-btn { width: 100%; padding: 12px; font-size: 1em; cursor: pointer;
                  border: 2px solid #d97706; border-radius: 6px;
                  background: #fff; color: #d97706; font-weight: bold;
                  margin-top: 10px; }
    .piscar-btn.ativo { background: #d97706; color: white;
                        animation: pulse 1s infinite; }
    @keyframes pulse { 50% { opacity: 0.6; } }
    .estado { background: #eef6ff; padding: 10px; border-radius: 6px;
              font-size: 0.85em; color: #1e40af; text-align: center;
              margin-top: 12px; font-family: Consolas, monospace; }
  </style>
</head>
<body>
  <h1>Controle PWM &mdash; ESP32-C3</h1>
  <div class="sub">PWM em hardware (LEDC). LED canal 0 | Servo canal 2.</div>

  <div class="card led-card">
    <h2>&#128161; LED</h2>

    <div id="brilhoArea">
      <div class="row">
        <label>Brilho:</label>
        <input id="led" type="range" min="0" max="100" value="0">
        <span class="valor"><span id="ledVal">0</span>%</span>
      </div>
      <div class="botoes botoes-4">
        <button onclick="setLed(0)">0%</button>
        <button onclick="setLed(25)">25%</button>
        <button onclick="setLed(50)">50%</button>
        <button onclick="setLed(100)">100%</button>
      </div>
    </div>

    <div class="row" style="margin-top: 14px;">
      <label>Frequ&ecirc;ncia:</label>
      <input id="freq" type="number" min="1" max="40000" value="5000">
      <span class="valor">Hz</span>
    </div>
    <div class="botoes">
      <button onclick="setFreq(1)">1 Hz</button>
      <button onclick="setFreq(10)">10 Hz</button>
      <button onclick="setFreq(100)">100 Hz</button>
      <button onclick="setFreq(1000)">1 kHz</button>
      <button onclick="setFreq(5000)">5 kHz</button>
      <button onclick="setFreq(10000)">10 kHz</button>
    </div>

    <button id="piscarBtn" class="piscar-btn" onclick="togglePiscar()">
      Modo Piscar: <span id="piscarEstado">OFF</span>
    </button>
  </div>

  <div class="card servo-card">
    <h2>&#9881;&#65039; Servo &mdash; Posi&ccedil;&atilde;o</h2>
    <div class="row">
      <input id="servo" type="range" min="0" max="180" value="90">
      <span class="valor"><span id="servoVal">90</span>&deg;</span>
    </div>
    <div class="botoes botoes-4">
      <button onclick="setServo(0)">0&deg;</button>
      <button onclick="setServo(90)">90&deg;</button>
      <button onclick="setServo(180)">180&deg;</button>
      <button id="sweepBtn" onclick="toggleSweep()">Sweep</button>
    </div>
  </div>

  <div class="estado" id="estado">Aguardando comando...</div>

  <script>
    const ledSlider   = document.getElementById('led');
    const freqInput   = document.getElementById('freq');
    const servoSlider = document.getElementById('servo');
    const ledVal      = document.getElementById('ledVal');
    const servoVal    = document.getElementById('servoVal');
    const estado      = document.getElementById('estado');
    const piscarBtn   = document.getElementById('piscarBtn');
    const piscarEst   = document.getElementById('piscarEstado');
    const brilhoArea  = document.getElementById('brilhoArea');
    const sweepBtn    = document.getElementById('sweepBtn');

    let blinkAtivo = false;
    let pendingTimer = null;

    function envia(params) {
      fetch('/set?' + params.toString())
        .then(r => r.json())
        .then(j => atualizarUI(j))
        .catch(e => estado.textContent = 'Erro: ' + e);
    }

    function atualizarUI(j) {
      blinkAtivo = j.blink;
      ledVal.textContent   = j.led;
      ledSlider.value      = j.led;
      freqInput.value      = j.freq;
      servoVal.textContent = j.servo;
      servoSlider.value    = j.servo;
      piscarEst.textContent = j.blink ? 'ON' : 'OFF';
      if (j.blink) {
        piscarBtn.classList.add('ativo');
        brilhoArea.classList.add('desabilitado');
      } else {
        piscarBtn.classList.remove('ativo');
        brilhoArea.classList.remove('desabilitado');
      }
      estado.textContent = `LED ${j.led}% @ ${j.freq} Hz`
                         + (j.blink ? ' [PISCAR]' : '')
                         + `  |  Servo ${j.servo}°`;
    }

    function agenda(fn) {
      if (pendingTimer) return;
      pendingTimer = setTimeout(() => {
        pendingTimer = null;
        fn();
      }, 50);
    }

    ledSlider.addEventListener('input', () => {
      ledVal.textContent = ledSlider.value;
      agenda(() => {
        const p = new URLSearchParams();
        p.append('led', ledSlider.value);
        envia(p);
      });
    });

    servoSlider.addEventListener('input', () => {
      servoVal.textContent = servoSlider.value;
      agenda(() => {
        const p = new URLSearchParams();
        p.append('servo', servoSlider.value);
        envia(p);
      });
    });

    freqInput.addEventListener('change', () => {
      const p = new URLSearchParams();
      p.append('freq', freqInput.value);
      envia(p);
    });

    function setLed(v) {
      ledSlider.value = v;
      ledVal.textContent = v;
      const p = new URLSearchParams();
      p.append('led', v);
      envia(p);
    }

    function setFreq(v) {
      freqInput.value = v;
      const p = new URLSearchParams();
      p.append('freq', v);
      envia(p);
    }

    function setServo(v) {
      if (sweeping) toggleSweep();
      servoSlider.value = v;
      servoVal.textContent = v;
      const p = new URLSearchParams();
      p.append('servo', v);
      envia(p);
    }

    function togglePiscar() {
      const p = new URLSearchParams();
      p.append('blink', blinkAtivo ? 'off' : 'on');
      envia(p);
    }

    let sweeping = false;
    async function toggleSweep() {
      if (sweeping) {
        sweeping = false;
        sweepBtn.classList.remove('ativo');
        sweepBtn.textContent = 'Sweep';
        return;
      }
      sweeping = true;
      sweepBtn.classList.add('ativo');
      sweepBtn.textContent = 'Stop';
      const step = 5;
      while (sweeping) {
        for (let a = 0; a <= 180 && sweeping; a += step) {
          servoSlider.value = a;
          servoVal.textContent = a;
          const p = new URLSearchParams();
          p.append('servo', a);
          envia(p);
          await new Promise(r => setTimeout(r, 30));
        }
        for (let a = 180; a >= 0 && sweeping; a -= step) {
          servoSlider.value = a;
          servoVal.textContent = a;
          const p = new URLSearchParams();
          p.append('servo', a);
          envia(p);
          await new Promise(r => setTimeout(r, 30));
        }
      }
    }

    fetch('/state').then(r => r.json()).then(atualizarUI);
  </script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

String stateJson() {
  String r = "{";
  r += "\"led\":"   + String(currentLedPercent) + ",";
  r += "\"freq\":"  + String(currentLedFreq) + ",";
  r += "\"blink\":" + String(currentLedBlink ? "true" : "false") + ",";
  r += "\"servo\":" + String(currentServoAngle);
  r += "}";
  return r;
}

void handleSet() {
  bool changed = false;
  if (server.hasArg("led")) {
    setLedBrightness(server.arg("led").toInt());
    changed = true;
  }
  if (server.hasArg("freq")) {
    setLedFrequency(server.arg("freq").toInt());
    changed = true;
  }
  if (server.hasArg("blink")) {
    String b = server.arg("blink");
    setLedBlink(b == "on" || b == "1" || b == "true");
    changed = true;
  }
  if (server.hasArg("servo")) {
    setServoAngle(server.arg("servo").toInt());
    changed = true;
  }
  server.send(200, "application/json", stateJson());

  if (changed) {
    Serial.printf("[SET] led=%d%% freq=%d Hz blink=%s servo=%d\xc2\xb0\n",
                  currentLedPercent, currentLedFreq,
                  currentLedBlink ? "ON" : "OFF",
                  currentServoAngle);
  }
}

void handleState() {
  server.send(200, "application/json", stateJson());
}

void setup() {
  Serial.begin(115200);
  delay(200);

  if (!ledcAttachChannel(LED_PIN, LED_FREQ_INITIAL, LED_RES_BITS, 0)) {
    Serial.println("[ERRO] Falha ao configurar PWM do LED!");
  }
  if (!ledcAttachChannel(SERVO_PIN, SERVO_FREQ, SERVO_RES_BITS, 2)) {
    Serial.println("[ERRO] Falha ao configurar PWM do Servo!");
  }
  setLedBrightness(0);
  setServoAngle(90);

  WiFi.softAP(ssid, password);
  IPAddress ip = WiFi.softAPIP();
  Serial.println();
  Serial.println("=== Lab 05: Controle PWM (LED + Servo) ===");
  Serial.printf("SSID: %s, Senha: %s\n", ssid, password);
  Serial.print("IP do AP: ");
  Serial.println(ip);
  Serial.printf("LED:   GPIO %d, %d Hz inicial, %d bits (canal 0, freq ajustavel %d..%d Hz)\n",
                LED_PIN, LED_FREQ_INITIAL, LED_RES_BITS,
                LED_FREQ_MIN, LED_FREQ_MAX);
  Serial.printf("Servo: GPIO %d, %d Hz, %d bits (canal 2, duty %d..%d, pulse 0,5..2,5 ms)\n",
                SERVO_PIN, SERVO_FREQ, SERVO_RES_BITS,
                SERVO_MIN_DUTY, SERVO_MAX_DUTY);

  server.on("/",      handleRoot);
  server.on("/set",   handleSet);
  server.on("/state", handleState);
  server.begin();
  Serial.println("Servidor HTTP iniciado. Acesse http://192.168.4.1/");
}

void loop() {
  server.handleClient();

  if (currentLedBlink) {
    unsigned long now = millis();
    unsigned long halfPeriod = 500 / currentLedFreq;
    if (halfPeriod < 1) halfPeriod = 1;
    if (now - lastBlinkToggle >= halfPeriod) {
      blinkState = !blinkState;
      ledcWrite(LED_PIN, blinkState ? LED_MAX_DUTY : 0);
      lastBlinkToggle = now;
    }
  }
}