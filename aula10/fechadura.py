#!/usr/bin/env python3
# =====================================================================
#  FECHADURA ELETRONICA - Raspberry Pi 3 + Freenove Projects Board
#  PCS3732 - Laboratorio de Processadores
#
#  No Thonny:
#    - Sistema completo: MODO = "main" e clique Run.
#    - Teste isolado: MODO = "lcd" | "keypad" | "buzzer" | "sensor" | "lock"
#    - Terminal: python3 fechadura.py keypad
# =====================================================================
import sys
import time
import hashlib
import hmac

import RPi.GPIO as GPIO

try:
    from smbus2 import SMBus
except ImportError:
    from smbus import SMBus

# =====================================================================
#  >>>>>>>>>>>>>>>>>>>>  PINOS (Freenove Projects Board)  <<<<<<<<<<<<<
#  Numeracao BCM, lida na serigrafia da placa.
# =====================================================================
MODO = "main"          # "main" | "lcd" | "keypad" | "buzzer" | "sensor" | "lock"

# Teclado 4x4 -> header de 8 pinos (GPIO16,20,21,26,19,13,6,5)
KEYPAD_ROWS = [16, 20, 21, 26]
KEYPAD_COLS = [19, 13, 6, 5]

# Buzzer (ative no header do buzzer da placa; ajuste se necessario)
BUZZER_PIN = 12   # CONFIRMAR no header do buzzer da placa (NAO pode ser 26 = teclado)

# Sensor de porta -> HC-SR04 no header (GND/GPIO15/GPIO14/5V)
ULTRA_TRIG = 14   # conecte o fio Trig do sensor no GPIO14
ULTRA_ECHO = 15   # conecte o fio Echo do sensor no GPIO15
ULTRA_THRESHOLD_CM = 8.0

# (alternativa) reed switch / botao como sensor digital
SENSOR_PIN = 4

# Trava (servo) -> header Servo (GPIO18)
LOCK_PIN = 18
LOCK_LOCKED_ANGLE = 0
LOCK_UNLOCKED_ANGLE = 90

# LCD I2C 16x2 -> header I2C (SCL/SDA/5V/GND)
LCD_ADDR = 0x27
I2C_BUS = 1

# Seguranca / regras
PASSWORD = "1234"
PASSWORD_MIN = 4
PASSWORD_MAX = 6
MAX_FAILURES = 3
COOLDOWN_SECONDS = 30
UNLOCK_SECONDS = 15
TYPING_TIMEOUT = 8
TAMPER_CONFIRM_SECONDS = 5   # filtro: porta aberta por X s antes de alarmar
SENSOR_PERIOD = 0.1            # le o ultrassonico a 10 Hz (saudavel p/ HC-SR04)
ERROR_MSG_SECONDS = 3.0        # tempo que a msg de erro fica na tela
# =====================================================================

LCD_CHR = 0b00000001
LCD_CMD = 0b00000000
LCD_EN  = 0b00000100
LCD_BL  = 0b00001000
LCD_LINE = [0x80, 0xC0, 0x94, 0xD4]


class LcdI2C:
    def __init__(self, addr=0x27, bus=1, cols=16, rows=2):
        self.addr = addr; self.cols = cols; self.rows = rows
        self.bus = SMBus(bus); self.backlight = LCD_BL
        self._init_display()
    def _raw(self, data):
        self.bus.write_byte(self.addr, data | self.backlight)
    def _pulse(self, data):
        time.sleep(0.0005); self._raw(data | LCD_EN)
        time.sleep(0.0005); self._raw(data & ~LCD_EN); time.sleep(0.0001)
    def _send(self, bits, mode):
        high = mode | (bits & 0xF0); low = mode | ((bits << 4) & 0xF0)
        self._raw(high); self._pulse(high); self._raw(low); self._pulse(low)
    def _init_display(self):
        time.sleep(0.02)
        for cmd in (0x33, 0x32): self._send(cmd, LCD_CMD)
        self._send(0x28, LCD_CMD); self._send(0x0C, LCD_CMD)
        self._send(0x06, LCD_CMD); self._send(0x01, LCD_CMD); time.sleep(0.005)
    def clear(self):
        self._send(0x01, LCD_CMD); time.sleep(0.005)
    def set_backlight(self, on):
        self.backlight = LCD_BL if on else 0; self._raw(0)
    def write(self, text, line=0):
        self._send(LCD_LINE[line], LCD_CMD)
        for ch in str(text).ljust(self.cols)[:self.cols]:
            self._send(ord(ch), LCD_CHR)
    def close(self):
        try: self.clear(); self.set_backlight(False); self.bus.close()
        except Exception: pass


class Keypad:
    LAYOUT = [['1','2','3','A'],['4','5','6','B'],
              ['7','8','9','C'],['*','0','#','D']]
    def __init__(self, row_pins, col_pins, debounce=0.20):
        self.rows = row_pins; self.cols = col_pins; self.debounce = debounce
        for r in self.rows: GPIO.setup(r, GPIO.OUT, initial=GPIO.LOW)
        for c in self.cols: GPIO.setup(c, GPIO.IN, pull_up_down=GPIO.PUD_DOWN)
        self._held = None; self._last_edge = 0.0
    def _scan(self):
        found = None
        for ri, r in enumerate(self.rows):
            GPIO.output(r, GPIO.HIGH)
            for ci, c in enumerate(self.cols):
                if GPIO.input(c) == GPIO.HIGH: found = self.LAYOUT[ri][ci]
            GPIO.output(r, GPIO.LOW)
        return found
    def get_key(self):
        key = self._scan(); now = time.monotonic()
        if key is None: self._held = None; return None
        if key == self._held: return None
        if now - self._last_edge < self.debounce: return None
        self._held = key; self._last_edge = now; return key


class Buzzer:
    def __init__(self, pin):
        self.pin = pin; GPIO.setup(pin, GPIO.OUT, initial=GPIO.LOW)
        self._steps = []; self._idx = 0; self._active = False
    def _play(self, seq):
        now = time.monotonic(); t = now; self._steps = []
        for on, dur in seq: t += dur; self._steps.append((on, t))
        self._idx = 0; self._active = True
    def beep_ok(self):   self._play([(True, 0.12)])
    def beep_fail(self): self._play([(True, 0.6)])
    def beep_key(self):  self._play([(True, 0.03)])
    def beep_alert(self):
        seq = []
        for _ in range(5): seq += [(True, 0.1), (False, 0.1)]
        self._play(seq)
    def is_busy(self): return self._active
    def silence(self):
        self._active = False; GPIO.output(self.pin, GPIO.LOW)
    def update(self):
        if not self._active: return
        now = time.monotonic(); on, until = self._steps[self._idx]
        GPIO.output(self.pin, GPIO.HIGH if on else GPIO.LOW)
        if now >= until:
            self._idx += 1
            if self._idx >= len(self._steps):
                GPIO.output(self.pin, GPIO.LOW); self._active = False


class DoorSensor:
    def __init__(self, pin, closed_level=GPIO.LOW):
        self.pin = pin; self.closed_level = closed_level
        GPIO.setup(pin, GPIO.IN, pull_up_down=GPIO.PUD_UP)
    def is_closed(self):
        return GPIO.input(self.pin) == self.closed_level


class UltrasonicSensor:
    def __init__(self, trig, echo, threshold_cm=8.0):
        self.trig = trig; self.echo = echo; self.threshold = threshold_cm
        GPIO.setup(trig, GPIO.OUT, initial=GPIO.LOW)
        GPIO.setup(echo, GPIO.IN); time.sleep(0.05)
    def measure_cm(self):
        GPIO.output(self.trig, GPIO.HIGH); time.sleep(0.00001)
        GPIO.output(self.trig, GPIO.LOW)
        t0 = time.monotonic(); timeout = t0 + 0.03; start = t0
        while GPIO.input(self.echo) == 0:
            start = time.monotonic()
            if start > timeout: return float('inf')
        stop = start; timeout = time.monotonic() + 0.03
        while GPIO.input(self.echo) == 1:
            stop = time.monotonic()
            if stop > timeout: return float('inf')
        return ((stop - start) * 34300.0) / 2.0
    def is_closed(self):
        return self.measure_cm() <= self.threshold


class Lock:
    def __init__(self, pin, locked_angle=0, unlocked_angle=90):
        self.pin = pin; self.locked_angle = locked_angle
        self.unlocked_angle = unlocked_angle
        GPIO.setup(pin, GPIO.OUT); self.pwm = GPIO.PWM(pin, 50); self.pwm.start(0)
        self._stop_at = 0.0; self._moving = False; self.is_locked = True
        self._goto(locked_angle)
    def _goto(self, angle):
        duty = 2.5 + (angle / 180.0) * 10.0
        self.pwm.ChangeDutyCycle(duty); self._moving = True
        self._stop_at = time.monotonic() + 0.5
    def lock(self):   self.is_locked = True;  self._goto(self.locked_angle)
    def unlock(self): self.is_locked = False; self._goto(self.unlocked_angle)
    def update(self):
        if self._moving and time.monotonic() >= self._stop_at:
            self.pwm.ChangeDutyCycle(0); self._moving = False
    def close(self):
        try: self.pwm.stop()
        except Exception: pass


IDLE, TYPING, UNLOCKED, COOLDOWN, ALARM = range(5)

def sha256(text): return hashlib.sha256(text.encode()).hexdigest()


class KeyBuffer:
    def __init__(self, min_len, max_len):
        self.min = min_len; self.max = max_len; self.buf = ""
    def reset(self): self.buf = ""
    def feed(self, key):
        if key in "0123456789":
            if len(self.buf) < self.max: self.buf += key
            return "digit"
        if key == "*": self.buf = self.buf[:-1]; return "backspace"
        if key == "#": return "submit"
        if key == "D": self.buf = ""; return "cancel"
        return None


class FechaduraFSM:
    def __init__(self, lcd, keypad, buzzer, sensor, lock, password_hash,
                 clock=time.monotonic):
        self.lcd=lcd; self.keypad=keypad; self.buzzer=buzzer
        self.sensor=sensor; self.lock=lock
        self.password_hash=password_hash; self.now=clock
        self.state=IDLE; self.kb=KeyBuffer(PASSWORD_MIN, PASSWORD_MAX)
        self.failures=0; self.armed=False; self._open_since=None; self._msg_until=0.0
        self.t_state=self.now(); self.t_last_key=self.now()
        self._last_lcd=(None,None); self._show("Fechadura","Iniciando...")
    def _show(self, l0="", l1=""):
        if (l0,l1)!=self._last_lcd:
            self.lcd.write(l0,0); self.lcd.write(l1,1); self._last_lcd=(l0,l1)
    def _go(self, state):
        self.state=state; self.t_state=self.now(); self._open_since=None
    def _password_ok(self, entry):
        return hmac.compare_digest(sha256(entry), self.password_hash)
    def _masked(self): return "*"*len(self.kb.buf)
    def _flash(self, l0, l1, secs):
        self._show(l0,l1); self._msg_until=self.now()+secs
    def step(self, key, door_closed):
        now=self.now()
        if door_closed and not self.armed and self.state in (IDLE,TYPING,COOLDOWN):
            self.armed=True
        # violacao fisica (RF3) com filtro anti-ruido
        if self.armed and self.state in (IDLE,TYPING,COOLDOWN):
            if not door_closed:
                if self._open_since is None: self._open_since=now
                elif now-self._open_since>=TAMPER_CONFIRM_SECONDS:
                    self._trigger_alarm()
            else:
                self._open_since=None

        if self.state==IDLE:
            if now>=self._msg_until:
                self._show("Digite a senha","e tecle #")
            if key is not None:
                if self.kb.feed(key)=="digit":
                    self.buzzer.beep_key(); self.t_last_key=now; self._go(TYPING)
        elif self.state==TYPING:
            if key is not None:
                self.t_last_key=now; r=self.kb.feed(key)
                if r in ("digit","backspace"):
                    self.buzzer.beep_key(); self._show("Senha:",self._masked())
                elif r=="cancel": self.kb.reset(); self._go(IDLE)
                elif r=="submit": self._submit()
            elif now-self.t_last_key>TYPING_TIMEOUT:
                self.kb.reset(); self._go(IDLE)
            else: self._show("Senha:",self._masked())
        elif self.state==UNLOCKED:
            self._show("Acesso liberado","Porta destranc.")
            if door_closed and now-self.t_state>1.0: self._relock()
            elif now-self.t_state>UNLOCK_SECONDS and door_closed: self._relock()
        elif self.state==COOLDOWN:
            restante=int(COOLDOWN_SECONDS-(now-self.t_state))+1
            self._show("Bloqueado","Aguarde %02ds"%max(restante,0))
            if now-self.t_state>=COOLDOWN_SECONDS:
                self.failures=0; self.kb.reset(); self._go(IDLE)
        elif self.state==ALARM:
            self._show("!! VIOLACAO !!","Senha p/ limpar")
            if not self.buzzer.is_busy(): self.buzzer.beep_alert()
            if key is not None and self.kb.feed(key)=="submit":
                if self._password_ok(self.kb.buf):
                    self.buzzer.silence(); self.kb.reset()
                    self.armed=False; self.lock.lock(); self._go(IDLE)
                else: self.kb.reset()
    def _submit(self):
        entry=self.kb.buf
        if len(entry)<PASSWORD_MIN:
            self.buzzer.beep_fail(); self._flash("Senha curta","Min %d digitos"%PASSWORD_MIN, ERROR_MSG_SECONDS)
            self.kb.reset(); self._go(IDLE); return
        if self._password_ok(entry):
            self.failures=0; self.kb.reset(); self.buzzer.beep_ok()
            self.lock.unlock(); self.armed=False; self._go(UNLOCKED)
        else:
            self.failures+=1; self.kb.reset(); self.buzzer.beep_fail()
            if self.failures>=MAX_FAILURES:
                self._show("Acesso negado","Bloqueando..."); self._go(COOLDOWN)
            else:
                self._flash("Acesso negado","Tentativa %d/%d"%(self.failures,MAX_FAILURES), ERROR_MSG_SECONDS)
                self._go(IDLE)
    def _relock(self):
        self.lock.lock(); self.kb.reset(); self.armed=False; self._go(IDLE)
    def _trigger_alarm(self):
        self.buzzer.beep_alert(); self.kb.reset(); self._go(ALARM)


def test_lcd():
    lcd=LcdI2C(addr=LCD_ADDR,bus=I2C_BUS)
    lcd.write("Hello World",0); lcd.write("LCD I2C OK",1)
    print("Texto no LCD? endereco e fiacao OK."); time.sleep(3); lcd.close()

def test_keypad():
    kp=Keypad(KEYPAD_ROWS,KEYPAD_COLS)
    print("Pressione teclas (1x cada). Ctrl+C sai.")
    try:
        while True:
            k=kp.get_key()
            if k: print("Tecla:",k)
            time.sleep(0.01)
    except KeyboardInterrupt: pass

def test_buzzer():
    bz=Buzzer(BUZZER_PIN); print("OK, FAIL, ALERT...")
    for fn in (bz.beep_ok,bz.beep_fail,bz.beep_alert):
        fn(); t=time.monotonic()+1.5
        while time.monotonic()<t: bz.update(); time.sleep(0.005)

def test_sensor():
    s=UltrasonicSensor(ULTRA_TRIG,ULTRA_ECHO,ULTRA_THRESHOLD_CM)
    print("Aproxime/afaste a mao do sensor. Ctrl+C sai.")
    try:
        while True:
            d=s.measure_cm()
            print("%.1f cm -> %s"%(d,"FECHADA" if d<=ULTRA_THRESHOLD_CM else "ABERTA"))
            time.sleep(0.2)
    except KeyboardInterrupt: pass

def test_lock():
    lk=Lock(LOCK_PIN,LOCK_LOCKED_ANGLE,LOCK_UNLOCKED_ANGLE)
    print("Trancando/destrancando 3x...")
    try:
        for _ in range(3):
            lk.unlock(); t=time.monotonic()+1.0
            while time.monotonic()<t: lk.update(); time.sleep(0.01)
            lk.lock(); t=time.monotonic()+1.0
            while time.monotonic()<t: lk.update(); time.sleep(0.01)
    finally: lk.close()


def main():
    lcd=LcdI2C(addr=LCD_ADDR,bus=I2C_BUS)
    keypad=Keypad(KEYPAD_ROWS,KEYPAD_COLS)
    buzzer=Buzzer(BUZZER_PIN)
    sensor=UltrasonicSensor(ULTRA_TRIG,ULTRA_ECHO,ULTRA_THRESHOLD_CM)
    lock=Lock(LOCK_PIN,LOCK_LOCKED_ANGLE,LOCK_UNLOCKED_ANGLE)
    fsm=FechaduraFSM(lcd,keypad,buzzer,sensor,lock,sha256(PASSWORD))
    print("Fechadura rodando. Ctrl+C sai.")
    last_check=0.0; door_closed=True
    try:
        while True:
            now=time.monotonic()
            key=keypad.get_key()
            if now-last_check>=SENSOR_PERIOD:
                door_closed=sensor.is_closed(); last_check=now
            fsm.step(key,door_closed)
            buzzer.update(); lock.update()
            time.sleep(0.01)
    except KeyboardInterrupt: pass
    finally: lock.close(); lcd.close()


if __name__=="__main__":
    GPIO.setmode(GPIO.BCM); GPIO.setwarnings(False)
    modo=sys.argv[1] if len(sys.argv)>1 else MODO
    try:
        {"lcd":test_lcd,"keypad":test_keypad,"buzzer":test_buzzer,
         "sensor":test_sensor,"lock":test_lock,"main":main}.get(modo,main)()
    finally:
        GPIO.cleanup()
