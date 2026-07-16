import sys
import time
import RPi.GPIO as GPIO

LED_PIN = 17
BUZZER_PIN = 12
SERVO_PIN = 18
BTN_MAIS = 20
BTN_MENOS = 21

bpm = 60

GPIO.setmode(GPIO.BCM)
GPIO.setwarnings(False)

GPIO.setup(LED_PIN, GPIO.OUT)
GPIO.setup(BUZZER_PIN, GPIO.OUT)
GPIO.setup(SERVO_PIN, GPIO.OUT)

GPIO.setup(BTN_MAIS, GPIO.IN, pull_up_down=GPIO.PUD_UP)
GPIO.setup(BTN_MENOS, GPIO.IN, pull_up_down=GPIO.PUD_UP)

led_pwm = GPIO.PWM(LED_PIN, 1000)
servo_pwm = GPIO.PWM(SERVO_PIN, 50)

led_pwm.start(0)
servo_pwm.start(0)

GPIO.output(BUZZER_PIN, GPIO.LOW)


def brilho_led(valor):
    valor = max(0, min(100, valor))
    led_pwm.ChangeDutyCycle(valor)


def mover_servo(angulo):
    angulo = max(0, min(180, angulo))
    duty = 2.5 + angulo / 18
    servo_pwm.ChangeDutyCycle(duty)


def ligar_buzzer():
    GPIO.output(BUZZER_PIN, GPIO.HIGH)


def desligar_buzzer():
    GPIO.output(BUZZER_PIN, GPIO.LOW)


def teste_led():
    frequencias = [100, 500, 1000, 5000]

    for frequencia in frequencias:
        print("Frequência:", frequencia, "Hz")
        led_pwm.ChangeFrequency(frequencia)

        for duty in range(0, 101, 10):
            brilho_led(duty)
            time.sleep(0.1)

        for duty in range(100, -1, -10):
            brilho_led(duty)
            time.sleep(0.1)

    brilho_led(0)


def teste_servo():
    angulos = [45, 90, 135, 90]

    for angulo in angulos:
        print("Ângulo:", angulo)
        mover_servo(angulo)
        time.sleep(1)

    servo_pwm.ChangeDutyCycle(0)


def teste_buzzer():
    for _ in range(3):
        print("Buzzer ligado")
        ligar_buzzer()
        time.sleep(0.4)

        desligar_buzzer()
        time.sleep(0.4)


def metronomo():
    global bpm

    print("Metrônomo iniciado")
    print("BPM inicial:", bpm)
    print("Botão azul aumenta o BPM")
    print("Botão vermelho diminui o BPM")
    print("Ctrl+C para encerrar")

    proxima_batida = time.monotonic()
    lado = False

    estado_anterior_mais = GPIO.input(BTN_MAIS)
    estado_anterior_menos = GPIO.input(BTN_MENOS)

    ultimo_mais = 0
    ultimo_menos = 0

    while True:
        agora = time.monotonic()

        estado_mais = GPIO.input(BTN_MAIS)
        estado_menos = GPIO.input(BTN_MENOS)

        if estado_anterior_mais == GPIO.HIGH and estado_mais == GPIO.LOW:
            if agora - ultimo_mais > 0.25:
                bpm = min(240, bpm + 5)
                print("BPM:", bpm)
                ultimo_mais = agora

        if estado_anterior_menos == GPIO.HIGH and estado_menos == GPIO.LOW:
            if agora - ultimo_menos > 0.25:
                bpm = max(40, bpm - 5)
                print("BPM:", bpm)
                ultimo_menos = agora

        estado_anterior_mais = estado_mais
        estado_anterior_menos = estado_menos

        if agora >= proxima_batida:
            intervalo = 60.0 / bpm
            lado = not lado

            if lado:
                mover_servo(45)
            else:
                mover_servo(135)

            brilho_led(100)
            ligar_buzzer()

            time.sleep(0.08)

            brilho_led(0)
            desligar_buzzer()

            proxima_batida += intervalo

            if proxima_batida < time.monotonic():
                proxima_batida = time.monotonic()

        time.sleep(0.005)


def finalizar():
    desligar_buzzer()
    brilho_led(0)
    servo_pwm.ChangeDutyCycle(0)

    led_pwm.stop()
    servo_pwm.stop()

    GPIO.cleanup()


def main():
    if len(sys.argv) > 1:
        opcao = sys.argv[1].lower()
    else:
        print("1 - Testar LED")
        print("2 - Testar servomotor")
        print("3 - Testar buzzer")
        print("4 - Executar metrônomo")

        opcao = input("Opção: ").strip()

    if opcao in ["1", "led"]:
        teste_led()
    elif opcao in ["2", "servo"]:
        teste_servo()
    elif opcao in ["3", "buzzer"]:
        teste_buzzer()
    elif opcao in ["4", "metro", "metronomo"]:
        metronomo()
    else:
        print("Opção inválida")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nPrograma encerrado")
    finally:
        finalizar()
