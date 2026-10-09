/*
 * barbeiro.c -- CODIGO BASE (sem sincronizacao)
 * Laboratorio de Problemas Classicos de IPC - Parte 5 (Barbeiro Sonolento)
 *
 * Uma barbearia tem UM barbeiro, UMA cadeira de corte e CADEIRAS
 * cadeiras na sala de espera. Se nao ha clientes, o barbeiro dorme.
 * Quando um cliente chega:
 *   - se o barbeiro esta dormindo, o cliente o acorda;
 *   - se o barbeiro esta ocupado e ha cadeira livre, o cliente senta e espera;
 *   - se todas as cadeiras de espera estao ocupadas, o cliente vai embora.
 *
 * Este programa ainda NAO tem nenhuma sincronizacao: o barbeiro nao
 * dorme (fica "cortando cabelo de ninguem") e os clientes alteram o
 * contador de cadeiras ao mesmo tempo.
 *
 * Sua tarefa: seguir os TODOs numerados (TODO 0 a TODO 11) e inserir as
 * diretivas de semaforos necessarias. Nao altere a logica de negocio,
 * apenas adicione as chamadas de sincronizacao nos pontos indicados.
 *
 * Compilar:  gcc -Wall -Wextra -pthread barbeiro.c -o barbeiro
 * Executar:  ./barbeiro
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>

#define CADEIRAS              3       /* cadeiras na sala de espera */
#define N_CLIENTES            20
#define TEMPO_CORTE_US        20000
#define INTERVALO_CHEGADA_US  15000   /* intervalo maximo (aleatorio) entre chegadas */

/* ---------------------------------------------------------------
 * Ponto de injecao de atraso, usado nos experimentos da Parte 5.3
 * do roteiro. Deixe em 0 ate que o roteiro peca para alterar.
 * ------------------------------------------------------------- */
#define ATRASO_CLIENTE_US     0

int esperando = 0;   /* clientes sentados na sala de espera */

/* TODO 0: declare aqui os semaforos necessarios.
 *   sem_t clientes;    -> conta clientes esperando (o barbeiro dorme nele)
 *   sem_t barbeiros;   -> barbeiro pronto para cortar (o cliente espera nele)
 *   sem_t mutex;       -> exclusao mutua no acesso a 'esperando'
 */
 sem_t clientes;
 sem_t barbeiros;
 sem_t mutex;


/* ---- Infraestrutura auxiliar de log/verificacao. NAO faz parte do
 *      exercicio de sincronizacao: serve apenas para voce enxergar o
 *      resultado (correto ou com erro) e para detectar travamentos. ---- */
#define TIMEOUT_WATCHDOG_S 3
enum { C_CHEGANDO, C_ESPERANDO, C_ATENDIDO, C_DESISTIU };
volatile int situacao_cliente[N_CLIENTES];
volatile int barbeiro_dormindo = 0;
volatile int encerrar = 0;          /* main avisa o barbeiro que acabou o expediente */
volatile int chamados = 0;          /* quantos clientes o barbeiro ja chamou */
int atendidos = 0, desistentes = 0, cortes = 0, erros = 0;
volatile int progresso = 0;
pthread_mutex_t aux_mutex = PTHREAD_MUTEX_INITIALIZER;

long agora_us(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000000L + t.tv_nsec / 1000;
}

void verificar_sala(const char *quem) {
    int e = esperando;
    if (e < 0 || e > CADEIRAS) {
        pthread_mutex_lock(&aux_mutex);
        printf("*** ERRO DE CONSISTENCIA (%s): esperando=%d fora de [0,%d] ***\n",
               quem, e, CADEIRAS);
        erros++;
        pthread_mutex_unlock(&aux_mutex);
    }
}

void cortar_cabelo(void) {
    int n = __sync_add_and_fetch(&cortes, 1);
    pthread_mutex_lock(&aux_mutex);
    printf("[Barbeiro]   cortando cabelo (corte %d)\n", n);
    pthread_mutex_unlock(&aux_mutex);
    usleep(TEMPO_CORTE_US);
    __sync_add_and_fetch(&progresso, 1);
}

void receber_corte(int id) {
    int n = __sync_add_and_fetch(&atendidos, 1);
    situacao_cliente[id] = C_ATENDIDO;
    pthread_mutex_lock(&aux_mutex);
    printf("[Cliente %2d] foi chamado para a cadeira do barbeiro\n", id);
    if (n > chamados) {
        printf("*** ERRO: cliente %d sentou na cadeira sem ser chamado pelo barbeiro! ***\n", id);
        erros++;
    }
    pthread_mutex_unlock(&aux_mutex);
    __sync_add_and_fetch(&progresso, 1);
}

void desistir(int id) {
    __sync_add_and_fetch(&desistentes, 1);
    situacao_cliente[id] = C_DESISTIU;
    pthread_mutex_lock(&aux_mutex);
    printf("[Cliente %2d] sala de espera cheia -- foi embora\n", id);
    pthread_mutex_unlock(&aux_mutex);
    __sync_add_and_fetch(&progresso, 1);
}

void *watchdog(void *arg) {
    (void) arg;
    int ultimo = -1, parado = 0;
    for (;;) {
        sleep(1);
        if (progresso != ultimo) { ultimo = progresso; parado = 0; continue; }
        if (++parado < TIMEOUT_WATCHDOG_S) continue;

        int cont[4] = {0};
        for (int i = 0; i < N_CLIENTES; i++) cont[situacao_cliente[i]]++;
        pthread_mutex_lock(&aux_mutex);
        printf("\n*** POSSIVEL DEADLOCK: nada aconteceu na barbearia nos ultimos %d s ***\n",
               TIMEOUT_WATCHDOG_S);
        printf("  Barbeiro ......................... %s\n",
               barbeiro_dormindo ? "dormindo (bloqueado esperando cliente)" : "acordado");
        printf("  esperando (sala de espera) ....... %d\n", esperando);
        printf("  Clientes chegando/na porta ....... %d\n", cont[C_CHEGANDO]);
        printf("  Clientes sentados esperando ...... %d\n", cont[C_ESPERANDO]);
        printf("  Clientes ja atendidos ............ %d\n", cont[C_ATENDIDO]);
        printf("  Clientes que desistiram .......... %d\n", cont[C_DESISTIU]);
        printf("  Expediente encerrado pelo main? .. %s\n", encerrar ? "sim" : "nao");
        fflush(stdout);
        exit(1);
    }
    return NULL;
}
/* ---- fim da infraestrutura auxiliar ---- */

void *barbeiro(void *arg) {
    (void) arg;
    for (;;) {
        barbeiro_dormindo = 1;

        /* TODO 1: dormir ate que chegue um cliente
         *   sem_wait(&clientes);
         */
        sem_wait(&clientes);
        barbeiro_dormindo = 0;
        if (encerrar) break;   /* (infra) acordado pelo main no fim do expediente */

        /* TODO 2: entrar na regiao critica
         *   sem_wait(&mutex);
         */
        sem_wait(&mutex);
        esperando--;           /* um cliente sai da sala de espera... */
        verificar_sala("barbeiro");
        chamados++;            /* (infra) */

        /* TODO 3: ...e e chamado para a cadeira de corte
         *   sem_post(&barbeiros);
         */
        sem_post(&barbeiros);
        /* TODO 4: sair da regiao critica
         *   sem_post(&mutex);
         */
        sem_post(&mutex);

        cortar_cabelo();
    }
    return NULL;
}

void *cliente(void *arg) {
    int id = *(int *) arg;

    /* TODO 5: entrar na regiao critica
     *   sem_wait(&mutex);
     */
    sem_wait(&mutex);

    if (esperando < CADEIRAS) {
        if (ATRASO_CLIENTE_US > 0) usleep(ATRASO_CLIENTE_US);

        esperando++;           /* senta na sala de espera */
        situacao_cliente[id] = C_ESPERANDO;
        verificar_sala("cliente");
        pthread_mutex_lock(&aux_mutex);
        printf("[Cliente %2d] sentou na sala de espera (esperando=%d)\n", id, esperando);
        pthread_mutex_unlock(&aux_mutex);
        __sync_add_and_fetch(&progresso, 1);

        /* TODO 6: avisar que ha mais um cliente (acorda o barbeiro se
         *         ele estiver dormindo)
         *   sem_post(&clientes);
         */
        sem_post(&clientes);

        /* TODO 7: sair da regiao critica
         *   sem_post(&mutex);
         */
        sem_post(&mutex);

        /* TODO 8: esperar o barbeiro ficar livre e chama-lo
         *   sem_wait(&barbeiros);
         */
        sem_wait(&barbeiros);
        receber_corte(id);
    } else {
        /* TODO 9: sair da regiao critica (sim, tambem neste caminho!)
         *   sem_post(&mutex);
         */
        sem_post(&mutex);
        desistir(id);
    }
    return NULL;
}

int main(void) {
    pthread_t barb, cli[N_CLIENTES], wd;
    int ids[N_CLIENTES];
    unsigned semente = (unsigned) time(NULL);

    /* TODO 0b: inicialize aqui os semaforos declarados no TODO 0.
     *   sem_init(&clientes, 0, 0);    // nenhum cliente no inicio
     *   sem_init(&barbeiros, 0, 0);   // barbeiro ainda nao chamou ninguem
     *   sem_init(&mutex, 0, 1);       // binario, comeca "livre"
     */
    sem_init(&clientes, 0, 0);
    sem_init(&barbeiros, 0, 0);
    sem_init(&mutex, 0, 1);
    long inicio = agora_us();
    pthread_create(&wd, NULL, watchdog, NULL);
    pthread_detach(wd);
    pthread_create(&barb, NULL, barbeiro, NULL);

    for (int i = 0; i < N_CLIENTES; i++) {
        usleep(rand_r(&semente) % (INTERVALO_CHEGADA_US + 1));
        ids[i] = i;
        pthread_create(&cli[i], NULL, cliente, &ids[i]);
    }
    for (int i = 0; i < N_CLIENTES; i++) pthread_join(cli[i], NULL);

    encerrar = 1;   /* fim do expediente */

    /* TODO 10: o barbeiro pode estar dormindo (bloqueado no TODO 1).
     *          Acorde-o para que ele perceba que o expediente acabou.
     *   sem_post(&clientes);
     */
    sem_post(&clientes);
    pthread_join(barb, NULL);

    pthread_mutex_lock(&aux_mutex);
    printf("\n===== RESULTADO FINAL =====\n");
    printf("Clientes que chegaram .... %d\n", N_CLIENTES);
    printf("Atendidos ................ %d\n", atendidos);
    printf("Desistiram ............... %d\n", desistentes);
    printf("Cortes feitos ............ %d\n", cortes);
    printf("esperando final .......... %d (deveria ser 0)\n", esperando);
    if (atendidos + desistentes != N_CLIENTES) {
        printf("*** ERRO: atendidos + desistentes != clientes que chegaram ***\n");
        erros++;
    }
    if (cortes != atendidos) {
        printf("*** ERRO: o barbeiro fez %d cortes, mas %d clientes foram atendidos ***\n",
               cortes, atendidos);
        erros++;
    }
    if (esperando != 0) erros++;
    printf("Erros detectados ......... %d\n", erros);
    printf("Tempo total .............. %.1f ms\n", (agora_us() - inicio) / 1000.0);
    pthread_mutex_unlock(&aux_mutex);

    /* TODO 11 (opcional, mas recomendado): destrua os semaforos
     *   sem_destroy(&clientes);
     *   sem_destroy(&barbeiros);
     *   sem_destroy(&mutex);
     */
    sem_destroy(&clientes);
    sem_destroy(&barbeiros);
    sem_destroy(&mutex);
    return 0;
}
