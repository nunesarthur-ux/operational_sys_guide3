/*
 * filosofos_estados.c -- SOLUCAO DE REFERENCIA
 * Laboratorio de Problemas Classicos de IPC - Parte 3 (Jantar dos Filosofos)
 *
 * Versao com os TODOs do codigo base resolvidos (solucao de Tanenbaum).
 * So consulte este arquivo depois de ter tentado resolver o codigo base
 * sozinho(a) -- ver Parte 3 do roteiro (README.md).
 *
 * Compilar:  gcc -Wall -Wextra -pthread filosofos_estados.c -o filosofos_estados
 * Executar:  ./filosofos_estados
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>

#define N_FILOSOFOS        5
#define REFEICOES          10     /* refeicoes por filosofo */
#define TEMPO_PENSAR_US    2000   /* tempo maximo (aleatorio) pensando */
#define TEMPO_COMER_US     2000

#define GARFO_ESQ(i)  (i)
#define GARFO_DIR(i)  (((i) + 1) % N_FILOSOFOS)
#define ESQUERDO(i)   (((i) + N_FILOSOFOS - 1) % N_FILOSOFOS)  /* vizinho da esquerda */
#define DIREITO(i)    (((i) + 1) % N_FILOSOFOS)                /* vizinho da direita  */

#define PENSANDO  0
#define COM_FOME  1
#define COMENDO   2

/* ---------------------------------------------------------------
 * Ponto de injecao de atraso, usado nos experimentos da Parte 3.3
 * do roteiro. Deixe em 0 ate que o roteiro peca para alterar.
 * ------------------------------------------------------------- */
#define ATRASO_TESTAR_US  0

int estado[N_FILOSOFOS];   /* estado de cada filosofo (comeca PENSANDO) */

/* TODO 0 resolvido: */
sem_t mutex;            /* exclusao mutua no acesso ao vetor estado[]   */
sem_t s[N_FILOSOFOS];   /* filosofo i bloqueia em s[i] se nao puder comer */


/* ---- Infraestrutura auxiliar de log/verificacao. NAO faz parte do
 *      exercicio de sincronizacao: serve apenas para voce enxergar o
 *      resultado (correto ou com erro) e para detectar travamentos. ---- */
#define TIMEOUT_WATCHDOG_S 3
enum { S_PENSANDO, S_COM_FOME, S_COMENDO, S_SATISFEITO };
volatile int situacao[N_FILOSOFOS];
int uso_garfo[N_FILOSOFOS];
int refeicoes[N_FILOSOFOS];
long maior_espera_us[N_FILOSOFOS];
int comendo_agora = 0, max_comendo_juntos = 0, erros = 0;
volatile int progresso = 0;
pthread_mutex_t aux_mutex = PTHREAD_MUTEX_INITIALIZER;

long agora_us(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000000L + t.tv_nsec / 1000;
}

void pensar(int i, unsigned *semente) {
    situacao[i] = S_PENSANDO;
    usleep(rand_r(semente) % (TEMPO_PENSAR_US + 1));
}

void comer(int i, long inicio_fome) {
    long espera = agora_us() - inicio_fome;
    if (espera > maior_espera_us[i]) maior_espera_us[i] = espera;
    situacao[i] = S_COMENDO;

    int e = __sync_add_and_fetch(&uso_garfo[GARFO_ESQ(i)], 1);
    int d = __sync_add_and_fetch(&uso_garfo[GARFO_DIR(i)], 1);
    int juntos = __sync_add_and_fetch(&comendo_agora, 1);

    pthread_mutex_lock(&aux_mutex);
    if (juntos > max_comendo_juntos) max_comendo_juntos = juntos;
    printf("[Filosofo %d] comendo (refeicao %2d) com garfos %d e %d\n",
           i, refeicoes[i] + 1, GARFO_ESQ(i), GARFO_DIR(i));
    if (e > 1) {
        printf("*** ERRO: garfo %d usado por dois filosofos ao mesmo tempo! ***\n", GARFO_ESQ(i));
        erros++;
    }
    if (d > 1) {
        printf("*** ERRO: garfo %d usado por dois filosofos ao mesmo tempo! ***\n", GARFO_DIR(i));
        erros++;
    }
    pthread_mutex_unlock(&aux_mutex);

    usleep(TEMPO_COMER_US);

    __sync_sub_and_fetch(&comendo_agora, 1);
    __sync_sub_and_fetch(&uso_garfo[GARFO_ESQ(i)], 1);
    __sync_sub_and_fetch(&uso_garfo[GARFO_DIR(i)], 1);
    refeicoes[i]++;
    __sync_add_and_fetch(&progresso, 1);
}

void *watchdog(void *arg) {
    (void) arg;
    static const char *nome_estado[] = { "PENSANDO", "COM_FOME", "COMENDO" };
    int ultimo = -1, parado = 0;
    for (;;) {
        sleep(1);
        if (progresso != ultimo) { ultimo = progresso; parado = 0; continue; }
        if (++parado < TIMEOUT_WATCHDOG_S) continue;

        pthread_mutex_lock(&aux_mutex);
        printf("\n*** POSSIVEL DEADLOCK: nenhum filosofo comeu nos ultimos %d s ***\n",
               TIMEOUT_WATCHDOG_S);
        for (int i = 0; i < N_FILOSOFOS; i++) {
            const char *onde = situacao[i] == S_SATISFEITO ? "ja terminou todas as refeicoes"
                             : situacao[i] == S_COM_FOME   ? "dentro de pegar_garfos()"
                             : situacao[i] == S_COMENDO    ? "comendo / devolvendo garfos"
                             :                               "pensando";
            int est = estado[i];
            printf("  Filosofo %d: estado[%d]=%-8s | %s\n", i, i,
                   (est >= 0 && est <= 2) ? nome_estado[est] : "?", onde);
        }
        fflush(stdout);
        exit(1);
    }
    return NULL;
}
/* ---- fim da infraestrutura auxiliar ---- */

void testar(int i) {
    if (estado[i] == COM_FOME &&
        estado[ESQUERDO(i)] != COMENDO &&
        estado[DIREITO(i)] != COMENDO) {

        if (ATRASO_TESTAR_US > 0) usleep(ATRASO_TESTAR_US);

        estado[i] = COMENDO;

        sem_post(&s[i]);       /* TODO 1 */
    }
}

void pegar_garfos(int i) {
    sem_wait(&mutex);          /* TODO 2 */

    estado[i] = COM_FOME;
    testar(i);                 /* tenta pegar os dois garfos de uma vez */

    sem_post(&mutex);          /* TODO 3 */
    sem_wait(&s[i]);           /* TODO 4: bloqueia se nao conseguiu os garfos */
}

void devolver_garfos(int i) {
    sem_wait(&mutex);          /* TODO 5 */

    estado[i] = PENSANDO;
    testar(ESQUERDO(i));       /* o vizinho da esquerda pode comer agora? */
    testar(DIREITO(i));        /* e o vizinho da direita?                 */

    sem_post(&mutex);          /* TODO 6 */
}

void *filosofo(void *arg) {
    int i = *(int *) arg;
    unsigned semente = (unsigned) time(NULL) ^ (i * 7919u);

    for (int r = 0; r < REFEICOES; r++) {
        pensar(i, &semente);

        situacao[i] = S_COM_FOME;
        long inicio_fome = agora_us();

        pegar_garfos(i);
        comer(i, inicio_fome);
        devolver_garfos(i);
    }
    situacao[i] = S_SATISFEITO;
    return NULL;
}

int main(void) {
    pthread_t fil[N_FILOSOFOS], wd;
    int ids[N_FILOSOFOS];

    /* TODO 0b resolvido: */
    sem_init(&mutex, 0, 1);
    for (int i = 0; i < N_FILOSOFOS; i++)
        sem_init(&s[i], 0, 0);

    long inicio = agora_us();
    pthread_create(&wd, NULL, watchdog, NULL);
    pthread_detach(wd);

    for (int i = 0; i < N_FILOSOFOS; i++) {
        ids[i] = i;
        pthread_create(&fil[i], NULL, filosofo, &ids[i]);
    }
    for (int i = 0; i < N_FILOSOFOS; i++) pthread_join(fil[i], NULL);

    pthread_mutex_lock(&aux_mutex);
    printf("\n===== RESULTADO FINAL =====\n");
    printf("Filosofo | Refeicoes | Maior espera (ms)\n");
    for (int i = 0; i < N_FILOSOFOS; i++)
        printf("   %d     |    %2d     |   %8.1f\n", i, refeicoes[i], maior_espera_us[i] / 1000.0);
    printf("Refeicoes esperadas por filosofo ... %d\n", REFEICOES);
    printf("Max. filosofos comendo juntos ...... %d (limite possivel: %d)\n",
           max_comendo_juntos, N_FILOSOFOS / 2);
    printf("Erros detectados ................... %d\n", erros);
    printf("Tempo total ........................ %.1f ms\n", (agora_us() - inicio) / 1000.0);
    pthread_mutex_unlock(&aux_mutex);

    sem_destroy(&mutex);       /* TODO 7 */
    for (int i = 0; i < N_FILOSOFOS; i++)
        sem_destroy(&s[i]);

    return 0;
}
