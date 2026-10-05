/*
 * filosofos_garfos.c -- SOLUCAO DE REFERENCIA (solucao ingenua)
 * Laboratorio de Problemas Classicos de IPC - Parte 2 (Jantar dos Filosofos)
 *
 * Versao com os TODOs do codigo base resolvidos: um semaforo por garfo.
 * ATENCAO: esta solucao garante exclusao mutua sobre cada garfo, mas
 * PODE ENTRAR EM DEADLOCK (ver experimento E2 do roteiro). So consulte
 * este arquivo depois de ter tentado resolver o codigo base sozinho(a).
 *
 * Compilar:  gcc -Wall -Wextra -pthread filosofos_garfos.c -o filosofos_garfos
 * Executar:  ./filosofos_garfos
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

/* ---------------------------------------------------------------
 * Ponto de injecao de atraso, usado nos experimentos da Parte 2.3
 * do roteiro. Deixe em 0 ate que o roteiro peca para alterar.
 * ------------------------------------------------------------- */
#define ATRASO_ENTRE_GARFOS_US  0

/* TODO 0 resolvido: */
sem_t garfo[N_FILOSOFOS];   /* garfo[g] = 1 -> garfo g esta livre na mesa */


/* ---- Infraestrutura auxiliar de log/verificacao. NAO faz parte do
 *      exercicio de sincronizacao: serve apenas para voce enxergar o
 *      resultado (correto ou com erro) e para detectar travamentos. ---- */
#define TIMEOUT_WATCHDOG_S 3
enum { PENSANDO, COM_FOME, COM_GARFO_ESQ, COMENDO, SATISFEITO };
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
    situacao[i] = PENSANDO;
    usleep(rand_r(semente) % (TEMPO_PENSAR_US + 1));
}

void comer(int i, long inicio_fome) {
    long espera = agora_us() - inicio_fome;
    if (espera > maior_espera_us[i]) maior_espera_us[i] = espera;
    situacao[i] = COMENDO;

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
    int ultimo = -1, parado = 0;
    for (;;) {
        sleep(1);
        if (progresso != ultimo) { ultimo = progresso; parado = 0; continue; }
        if (++parado < TIMEOUT_WATCHDOG_S) continue;

        pthread_mutex_lock(&aux_mutex);
        printf("\n*** POSSIVEL DEADLOCK: nenhum filosofo comeu nos ultimos %d s ***\n",
               TIMEOUT_WATCHDOG_S);
        for (int i = 0; i < N_FILOSOFOS; i++) {
            printf("  Filosofo %d: ", i);
            switch (situacao[i]) {
                case PENSANDO:      printf("pensando\n"); break;
                case COM_FOME:      printf("com fome, esperando o garfo esquerdo (%d)\n", GARFO_ESQ(i)); break;
                case COM_GARFO_ESQ: printf("segurando o garfo %d, esperando o garfo direito (%d)\n",
                                           GARFO_ESQ(i), GARFO_DIR(i)); break;
                case COMENDO:       printf("comendo\n"); break;
                case SATISFEITO:    printf("ja terminou todas as refeicoes\n"); break;
            }
        }
        fflush(stdout);
        exit(1);
    }
    return NULL;
}
/* ---- fim da infraestrutura auxiliar ---- */

void *filosofo(void *arg) {
    int i = *(int *) arg;
    unsigned semente = (unsigned) time(NULL) ^ (i * 7919u);

    for (int r = 0; r < REFEICOES; r++) {
        pensar(i, &semente);

        situacao[i] = COM_FOME;
        long inicio_fome = agora_us();

        sem_wait(&garfo[GARFO_ESQ(i)]);   /* TODO 1 */

        situacao[i] = COM_GARFO_ESQ;
        if (ATRASO_ENTRE_GARFOS_US > 0) usleep(ATRASO_ENTRE_GARFOS_US);

        sem_wait(&garfo[GARFO_DIR(i)]);   /* TODO 2 */

        comer(i, inicio_fome);

        sem_post(&garfo[GARFO_ESQ(i)]);   /* TODO 3 */
        sem_post(&garfo[GARFO_DIR(i)]);   /* TODO 4 */
    }
    situacao[i] = SATISFEITO;
    return NULL;
}

int main(void) {
    pthread_t fil[N_FILOSOFOS], wd;
    int ids[N_FILOSOFOS];

    /* TODO 0b resolvido: */
    for (int i = 0; i < N_FILOSOFOS; i++)
        sem_init(&garfo[i], 0, 1);

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

    for (int i = 0; i < N_FILOSOFOS; i++)   /* TODO 5 */
        sem_destroy(&garfo[i]);

    return 0;
}
