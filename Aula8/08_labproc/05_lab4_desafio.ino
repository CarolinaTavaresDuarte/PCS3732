#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <math.h>

#define ITER  5000000L 
#define REPS  25 
#define NOPS  4 
#define NLARG 4

void print_bits(long long v, int n) {
    for (int i = n - 1; i >= 0; i--)
        putchar((v >> i) & 1LL ? '1' : '0');
}

static double tempo_ns(struct timespec t0, struct timespec t1) {
    return (t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec);
}
static double media(const double *v, int n) {
    double s = 0;
    for (int i = 0; i < n; i++) s += v[i];
    return s / n;
}
static double desvio(const double *v, int n, double m) {
    double s = 0;
    for (int i = 0; i < n; i++) s += (v[i] - m) * (v[i] - m);
    return sqrt(s / (n - 1));            /* desvio padrao amostral */
}

#define BENCH_BIN(NOME, TIPO, OP)                                    \
    static double NOME(void) {                                       \
        volatile TIPO a = 11, b = 3, sink = 0;                       \
        struct timespec t0, t1;                                      \
        clock_gettime(CLOCK_MONOTONIC, &t0);                         \
        for (long i = 0; i < ITER; i++) sink = (TIPO)(a OP b);       \
        clock_gettime(CLOCK_MONOTONIC, &t1);                         \
        (void)sink;                                                  \
        return tempo_ns(t0, t1) / (double)ITER;                      \
    }

#define BENCH_FAT(NOME, TIPO)                                        \
    static double NOME(void) {                                       \
        volatile int n = 10;                                         \
        volatile TIPO sink = 0;                                      \
        struct timespec t0, t1;                                      \
        clock_gettime(CLOCK_MONOTONIC, &t0);                         \
        for (long i = 0; i < ITER / 10; i++) {                       \
            TIPO f = 1;                                              \
            for (int k = 2; k <= n; k++) f *= (TIPO)k;               \
            sink = f;                                                \
        }                                                            \
        clock_gettime(CLOCK_MONOTONIC, &t1);                         \
        (void)sink;                                                  \
        return tempo_ns(t0, t1) / (double)(ITER / 10);               \
    }

/* Soma */
BENCH_BIN(soma8,  uint8_t,  +) BENCH_BIN(soma16, uint16_t, +)
BENCH_BIN(soma32, uint32_t, +) BENCH_BIN(soma64, uint64_t, +)
/* Subtracao */
BENCH_BIN(sub8,   uint8_t,  -) BENCH_BIN(sub16,  uint16_t, -)
BENCH_BIN(sub32,  uint32_t, -) BENCH_BIN(sub64,  uint64_t, -)
/* Multiplicacao */
BENCH_BIN(mul8,   uint8_t,  *) BENCH_BIN(mul16,  uint16_t, *)
BENCH_BIN(mul32,  uint32_t, *) BENCH_BIN(mul64,  uint64_t, *)
/* Fatorial */
BENCH_FAT(fat8,   uint8_t)     BENCH_FAT(fat16,  uint16_t)
BENCH_FAT(fat32,  uint32_t)    BENCH_FAT(fat64,  uint64_t)

static double (*bench_funcs[NOPS][NLARG])(void) = {
    { soma8, soma16, soma32, soma64 },
    { sub8,  sub16,  sub32,  sub64  },
    { mul8,  mul16,  mul32,  mul64  },
    { fat8,  fat16,  fat32,  fat64  },
};
static const char *nomes_op[NOPS]    = { "Soma", "Subtracao", "Multiplic.", "Fatorial" };
static const char *nomes_bits[NLARG] = { "8 bits", "16 bits", "32 bits", "64 bits" };

static void modo_calculadora(void) {
    char op;
    long long a = 0, b = 0;

    printf("\nOperacao (+ - * / !): ");
    if (scanf(" %c", &op) != 1) return;

    if (op == '!') {
        printf("Operando n (0-15): ");
        if (scanf("%lld", &a) != 1) return;
    } else {
        printf("Operando A (0-15): ");
        if (scanf("%lld", &a) != 1) return;
        printf("Operando B (0-15): ");
        if (scanf("%lld", &b) != 1) return;
    }

    if (a < 0 || a > 15 || (op != '!' && (b < 0 || b > 15)))
        printf("Aviso: operando fora de 0-15; resultado pode nao refletir 4 bits.\n");

    long long r = 0, resto = 0;
    int overflow = 0, tem_resto = 0, valido = 1;

    switch (op) {
        case '+': r = a + b; overflow = (r > 15 || r < -8); break;
        case '-': r = a - b; overflow = (r > 7  || r < -8); break;
        case '*': r = a * b; overflow = (r > 15 || r < -8); break;
        case '!':
            r = 1;
            for (long long i = 2; i <= a; i++) r *= i;
            overflow = (r > 15);
            break;
        case '/':
            /* --- Atividade 4: divisao por zero --- */
            if (b == 0) {
                printf("\n>> ERRO: divisao por zero.\n");
                printf("   Tratado no software (checagem b==0): o programa NAO trava.\n");
                printf("   No hardware: a instrucao ARMv8 SDIV/UDIV devolveria 0 (sem\n");
                printf("   excecao); o RISC-V devolveria todos os bits em 1. Nenhum gera trap.\n");
                valido = 0;
            } else {
                r = a / b;              /* quociente */
                resto = a % b;          /* resto     */
                tem_resto = 1;
            }
            break;
        default:
            printf("Operacao invalida.\n");
            valido = 0;
    }
    if (!valido) return;

    printf("\n----- RESULTADO -----\n");
    printf("Decimal: %lld\n", r);
    printf("4 bits (compl. 2): ");
    print_bits(r, 4);
    printf("\n");
    if (tem_resto) printf("Resto: %lld\n", resto);
    if (overflow)  printf(">> OVERFLOW: valor real acima nao cabe em 4 bits.\n");
}

static void modo_benchmark(void) {
    printf("\n===== BENCHMARK - Raspberry Pi 3 (ARMv8, 64 bits) =====\n");
    printf("ITER=%ld operacoes/medida, REPS=%d medidas. Aguarde...\n\n", ITER, REPS);
    printf("Tempo por operacao em NANOSSEGUNDOS (media +/- desvio padrao)\n\n");

    printf("%-11s|", "Operacao");
    for (int j = 0; j < NLARG; j++) printf(" %-15s|", nomes_bits[j]);
    printf("\n-----------+");
    for (int j = 0; j < NLARG; j++) printf("----------------+");
    printf("\n");

    for (int i = 0; i < NOPS; i++) {
        printf("%-11s|", nomes_op[i]);
        for (int j = 0; j < NLARG; j++) {
            double amostras[REPS], m, d;
            for (int r = 0; r < REPS; r++) amostras[r] = bench_funcs[i][j]();
            m = media(amostras, REPS);
            d = desvio(amostras, REPS, m);
            printf(" %6.2f +/- %-4.2f|", m, d);
        }
        printf("\n");
    }
    printf("\nLeitura dos dados (p/ a analise da Ativ. 2):\n");
    printf(" - Soma/subtracao/multiplicacao dao tempos parecidos: sao dominadas\n");
    printf("   pelo overhead do laco e dos acessos, nao pela ULA (sao ~1 ciclo).\n");
    printf(" - No Rasp3 (64 bits) as 4 larguras ficam parecidas: tudo cabe no\n");
    printf("   registrador nativo. No ESP32 (32 bits) o caso 64 bits fica mais\n");
    printf("   lento, pois exige varias instrucoes encadeadas (carry).\n");
    printf(" - Compare com o ESP32 (160 MHz) x Rasp3 (1.2 GHz): ~7.5x de clock.\n");
}

static void modo_demo(void) {
    printf("\n===== CASOS ESPECIAIS (evidencias p/ relatorio) =====\n");

    printf("\n[1] Complemento de dois (subtracao negativa)\n");
    printf("    3 - 5 = %d  ->  4 bits: ", 3 - 5);
    print_bits(3 - 5, 4);
    printf("   (padrao de -2)\n");

    printf("\n[2] Overflow na soma\n");
    printf("    9 + 8 = %d  (> 15, nao cabe em 4 bits)\n", 9 + 8);

    printf("\n[3] Overflow na multiplicacao\n");
    printf("    6 * 7 = %d  (nibble so guardaria: ", 6 * 7);
    print_bits(6 * 7, 4);
    printf(")\n");

    printf("\n[4] Numeros grandes - fatorial (liga com a Ativ. 5)\n");
    long long f = 1;
    for (int k = 2; k <= 13; k++) f *= k;
    long long mag = f; int nb = 0;
    while (mag > 0) { mag >>= 1; nb++; }
    printf("    13! = %lld  ->  precisa de %d bits\n", f, nb);
    printf("    Cabe no registrador de 64 bits do Rasp3, mas NAO em 32 bits do ESP32.\n");

    printf("\n[5] Divisao por zero (Ativ. 4)\n");
    printf("    ARMv8: devolve 0  |  RISC-V: devolve todos os bits 1  |  nenhum gera trap.\n");
    printf("    Em C tratamos no software (checando o divisor antes de dividir).\n");
}

int main(void) {
    int opcao = -1;
    printf("=========================================================\n");
    printf(" Calculadora Binaria 4 bits + Benchmark\n");
    printf(" Raspberry Pi 3 - ARM Cortex-A53 (ARMv8, 64 bits)\n");
    printf("=========================================================\n");

    do {
        printf("\n--- MENU ---\n");
        printf(" 1 - Calculadora (operacoes uma a uma)      [Ativ. 1 e 4]\n");
        printf(" 2 - Benchmark de tempo (media e desvio)    [Ativ. 2]\n");
        printf(" 3 - Casos especiais (evidencias)           [relatorio]\n");
        printf(" 0 - Sair\n");
        printf("Escolha: ");
        if (scanf("%d", &opcao) != 1) break;

        switch (opcao) {
            case 1: modo_calculadora(); break;
            case 2: modo_benchmark();   break;
            case 3: modo_demo();        break;
            case 0: printf("Encerrando.\n"); break;
            default: printf("Opcao invalida.\n");
        }
    } while (opcao != 0);

    return 0;
}
