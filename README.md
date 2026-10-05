# Laboratório: Problemas Clássicos de IPC — Jantar dos Filósofos e Barbeiro Sonolento

## 1. Objetivos

Ao final deste laboratório você deve ser capaz de:

1. Descrever o **jantar dos filósofos** e o **barbeiro sonolento** e explicar quais problemas de sincronização cada um modela: **várias threads disputando vários recursos** (filósofos) e **coordenação entre quem presta e quem recebe um serviço, com fila limitada** (barbeiro).
2. Implementar os dois problemas com **semáforos POSIX** em C (`sem_t`, `sem_wait`, `sem_post`), inclusive a solução clássica de Tanenbaum para os filósofos.
3. Implementar os mesmos problemas com **monitores** em Java (`synchronized`, `wait()`, `notifyAll()`).
4. Provocar **deliberadamente** deadlock, livelock, *lost wakeup*, condição de corrida e trava vazada, inserindo atrasos e alterando a ordem das instruções, e explicar por que cada erro acontece.
5. Identificar as **quatro condições de Coffman** para deadlock e explicar qual delas cada solução dos filósofos elimina.
6. Comparar semáforos e monitores agora em problemas com **mais de uma condição de espera**.

> Não é necessário entregar nada neste laboratório. As perguntas ao longo do roteiro (Pergunta 1 a Pergunta 38) servem para fixação de conteúdo — **são a base da próxima prova** — então vale a pena parar, testar e responder cada uma antes de seguir em frente.

## 2. Pré-requisitos

- Ter feito o laboratório anterior (**produtor-consumidor com semáforos e monitores**). Este roteiro segue a mesma dinâmica e reaproveita os mesmos conceitos.
- **C**: `gcc` com suporte a `pthread` e `<semaphore.h>` (POSIX). No Windows, use **WSL**, uma máquina Linux ou um container Docker — a API POSIX de semáforos não está disponível nativamente no MSVC/MinGW puro.
- **Java**: JDK 11+ (`javac`, `java`).
- Terminal com acesso a `gcc`/`make` e `javac`/`java`.

## 3. Conceitos-chave (revisão rápida)

| Conceito | Definição |
|---|---|
| Deadlock (impasse) | Um conjunto de threads em que cada uma espera por um recurso que está com outra thread do mesmo conjunto. Ninguém avança, para sempre. |
| Condições de Coffman | As quatro condições **necessárias** para haver deadlock: (1) **exclusão mútua** — o recurso só pode ser usado por uma thread por vez; (2) **posse e espera** — a thread segura um recurso enquanto espera outro; (3) **não preempção** — ninguém pode tomar à força o recurso de outra thread; (4) **espera circular** — existe um ciclo T1 → T2 → … → T1 de "espera por recurso de". Eliminar **qualquer uma** delas impede o deadlock. |
| Livelock | As threads **não estão bloqueadas** — continuam executando, tentando, desistindo e tentando de novo —, mas nenhuma consegue progredir. |
| Inanição (*starvation*) | Uma thread específica espera indefinidamente, porque as outras sempre "passam na frente", embora o sistema como um todo continue progredindo. |
| *Lost wakeup* | Uma thread fica esperando por um sinal que nunca chega (ou chega para a thread errada), mesmo a condição pela qual ela espera já sendo verdadeira. |
| Trava vazada | Um lock/semáforo é adquirido, mas existe um caminho de execução em que ele nunca é liberado. |

## 4. Estrutura do repositório

```
lab-ipc-problemas-classicos/
├── README.md                          <- este roteiro
├── c/
│   ├── Makefile
│   ├── base/
│   │   ├── filosofos_garfos.c         <- filósofos, solução ingênua (TODO 0 a TODO 5)
│   │   ├── filosofos_estados.c        <- filósofos, solução de Tanenbaum (TODO 0 a TODO 7)
│   │   └── barbeiro.c                 <- barbeiro sonolento (TODO 0 a TODO 11)
│   └── solucao/                       <- os mesmos três arquivos, resolvidos
└── java/
    ├── filosofos/
    │   ├── base/    (Mesa.java com TODO 1 a TODO 4, Filosofo.java, Verificador.java, Main.java)
    │   └── solucao/
    └── barbeiro/
        ├── base/    (Barbearia.java com TODO 1 a TODO 7, Barbeiro.java, Cliente.java, Verificador.java, Main.java)
        └── solucao/
```

**Cenários.**
- **Filósofos**: **5 filósofos**, cada um faz **10 refeições**. Entre uma refeição e outra o filósofo pensa por um tempo aleatório (até 2 ms); cada refeição dura 2 ms. O filósofo `i` usa o garfo `i` (à sua esquerda) e o garfo `(i+1) % 5` (à sua direita).
- **Barbeiro**: **1 barbeiro**, **3 cadeiras** na sala de espera, **20 clientes** chegando em intervalos aleatórios (até 15 ms). Cada corte dura 20 ms.

**Infraestrutura de verificação.** Assim como no laboratório anterior, todos os programas já trazem um código de verificação que **não faz parte do exercício de sincronização** — ele só serve para você enxergar quando algo deu errado:

- Filósofos: `*** ERRO: garfo X usado por dois filosofos ao mesmo tempo! ***`, o **máximo de filósofos comendo juntos** (com 5 filósofos, nunca pode passar de 2), a **maior espera** de cada filósofo e o tempo total.
- Barbeiro: `esperando` fora do intervalo `[0, CADEIRAS]`, cliente que sentou na cadeira **sem ser chamado** pelo barbeiro, número de cortes diferente do número de clientes atendidos.
- **Watchdog**: se o programa ficar alguns segundos sem nenhum progresso (nenhum filósofo comeu / nada aconteceu na barbearia), uma mensagem `*** POSSIVEL DEADLOCK ***` aparece sozinha, junto com **o que cada thread estava fazendo naquele momento**, e o processo é encerrado — você não precisa apertar Ctrl+C. **Leia essa "fotografia" com atenção: ela é a principal evidência para responder várias perguntas.**

Vários erros deste laboratório são **probabilísticos**. Quando o roteiro pedir para rodar várias vezes, use um laço (o programa termina com código 1 quando o watchdog dispara):

```bash
for i in $(seq 1 100); do ./solucao/filosofos_garfos > /dev/null || echo "travou na execucao $i"; done
```

---

## 5. Parte 1 — Os dois problemas

### 5.1 Jantar dos filósofos (Dijkstra, 1965)

Cinco filósofos sentam em uma mesa redonda. Cada um alterna entre **pensar** e **comer**. Há um prato na frente de cada filósofo e **um garfo entre cada par de vizinhos** — cinco garfos ao todo. Para comer, o filósofo precisa dos **dois** garfos ao seu lado; depois de comer, devolve os dois à mesa.

```
              F0
         g0        g1
      F4              F1
        g4          g2
          F3   g3   F2
```

Requisitos:

1. **Exclusão mútua**: um garfo só pode estar na mão de um filósofo por vez (dois vizinhos nunca comem ao mesmo tempo).
2. **Ausência de deadlock**: os filósofos não podem ficar todos parados esperando uns pelos outros.
3. **Paralelismo**: filósofos que não são vizinhos devem poder comer **ao mesmo tempo** (com 5 filósofos, até 2 comem juntos).
4. Idealmente, **ausência de inanição**: nenhum filósofo fica com fome para sempre.

O problema é um modelo de qualquer situação em que **cada thread precisa de mais de um recurso exclusivo ao mesmo tempo** (ex.: transferir dinheiro entre duas contas bloqueando as duas; um processo que precisa de impressora **e** scanner).

### 5.2 Barbeiro sonolento (Dijkstra, 1965)

Uma barbearia tem **um barbeiro**, **uma cadeira de corte** e **N cadeiras** na sala de espera.

- Se não há clientes, o barbeiro **dorme** (não pode ficar em espera ocupada, gastando CPU).
- Quando chega um cliente e o barbeiro está dormindo, o cliente **o acorda**.
- Se o barbeiro está ocupado e há cadeira livre, o cliente **senta e espera** a sua vez.
- Se todas as cadeiras de espera estão ocupadas, o cliente **vai embora**.

O problema é um modelo de **servidor com fila limitada** (um servidor web com fila de conexões, uma central de atendimento, um *driver* de disco). Repare na semelhança com o produtor-consumidor: os clientes "produzem" pedidos de corte, o barbeiro "consome" — mas aqui o produtor **não espera** quando a fila está cheia, ele **desiste**; e, além disso, o cliente precisa **esperar ser chamado**, ou seja, há sinalização nos **dois sentidos**.

---

## 6. Parte 2 — Filósofos em C: a solução ingênua (um semáforo por garfo)

### 6.1 Rodando o código base sem nenhuma sincronização

```bash
cd c
make base
./base/filosofos_garfos
```

> **Pergunta 1.** Rode o programa 3 a 5 vezes. Quantas mensagens `*** ERRO ***` aparecem? Qual o valor de `Max. filosofos comendo juntos`? Por que, com 5 garfos e cada filósofo precisando de 2, esse valor **nunca** poderia passar de 2 em um programa correto?
>
> **Pergunta 2.** Repare que, mesmo com dezenas de erros, todo filósofo fez as 10 refeições e a "maior espera" é praticamente zero. Por que um programa sem sincronização pode parecer **mais rápido e "produtivo"** que um programa correto? Que lição isso traz sobre testar programas concorrentes só olhando se "terminou"?

### 6.2 Preenchendo os TODOs

A ideia mais natural é representar **cada garfo por um semáforo binário**: pegar o garfo é `sem_wait`, devolver é `sem_post`. Abra [`c/base/filosofos_garfos.c`](c/base/filosofos_garfos.c) e resolva os TODOs:

| TODO | O que fazer | Onde |
|---|---|---|
| 0 / 0b | Declarar `sem_t garfo[N_FILOSOFOS];` e inicializar cada um com `sem_init(&garfo[i], 0, 1)` | topo do arquivo / início de `main` |
| 1 | `sem_wait(&garfo[GARFO_ESQ(i)])` — pegar o garfo da esquerda | laço de `filosofo` |
| 2 | `sem_wait(&garfo[GARFO_DIR(i)])` — pegar o garfo da direita | antes de `comer(...)` |
| 3 | `sem_post(&garfo[GARFO_ESQ(i)])` — devolver o garfo da esquerda | depois de `comer(...)` |
| 4 | `sem_post(&garfo[GARFO_DIR(i)])` — devolver o garfo da direita | logo após o TODO 3 |
| 5 | `sem_destroy` em todos os garfos (boa prática) | final de `main` |

```bash
make base
./base/filosofos_garfos
```

> **Pergunta 3.** Rode 5 vezes. Nenhum `*** ERRO ***` deve aparecer e `Max. filosofos comendo juntos` deve ser no máximo 2. Compare o tempo total com o da Pergunta 1.
>
> **Pergunta 4.** Repare que aqui **não existe um `mutex`** como no produtor-consumidor. Por que ele não é necessário? Qual é o "dado compartilhado" que cada semáforo protege?

### 6.3 Experimentos — inserindo atrasos e provocando erros

Use sempre o mesmo procedimento: faça a alteração indicada no seu `c/base/filosofos_garfos.c` (já com os TODOs preenchidos), rode `make base && ./base/filosofos_garfos`, observe com atenção o resultado e **restaure o código antes de passar para o próximo experimento** (a não ser que o roteiro diga o contrário).

**E1 — O deadlock existe, mas é raro.**
Sem alterar nada, rode o programa 200 vezes seguidas:
```bash
for i in $(seq 1 200); do ./base/filosofos_garfos > /dev/null || echo "travou na execucao $i"; done
```
> **Pergunta 5.** Alguma execução travou? Em nossos testes, foi aproximadamente **1 em cada 200**. Se você rodasse o programa só 5 vezes (como na Pergunta 3), qual seria a chance de não perceber o problema? Por que bugs de concorrência raros são considerados **mais perigosos** que bugs frequentes?

**E2 — Atraso entre os dois garfos (deadlock garantido).**
Mude:
```c
#define ATRASO_ENTRE_GARFOS_US  0
```
para
```c
#define ATRASO_ENTRE_GARFOS_US  100000   /* 100 ms */
```
> **Pergunta 6.** O programa termina? Leia a "fotografia" impressa pelo watchdog: o que cada filósofo está segurando e pelo que está esperando? Desenhe o **grafo de espera** (F0 → F1 → … → F4 → F0) e explique por que o atraso transforma um deadlock raro em um deadlock quase certo (pense em "ampliar a janela", como no laboratório anterior).
>
> **Pergunta 7.** Verifique, uma por uma, as **quatro condições de Coffman** nesse cenário: onde está a exclusão mútua? a posse e espera? a não preempção? a espera circular?

Para os experimentos E3 a E5, **mantenha `ATRASO_ENTRE_GARFOS_US = 100000`** — assim, se a correção não funcionar, o deadlock aparece com certeza.

**E3 — Quebrando a espera circular: um filósofo "canhoto".**
Faça o **último** filósofo pegar os garfos na ordem inversa (primeiro o da direita, depois o da esquerda). Substitua as linhas dos TODOs 1 e 2 por:
```c
int primeiro = GARFO_ESQ(i), segundo = GARFO_DIR(i);
if (i == N_FILOSOFOS - 1) {        /* o ultimo filosofo e "canhoto" */
    primeiro = GARFO_DIR(i);
    segundo  = GARFO_ESQ(i);
}
sem_wait(&garfo[primeiro]);
situacao[i] = COM_GARFO_ESQ;
if (ATRASO_ENTRE_GARFOS_US > 0) usleep(ATRASO_ENTRE_GARFOS_US);
sem_wait(&garfo[segundo]);
```
> **Pergunta 8.** O deadlock desaparece? Qual das quatro condições de Coffman foi eliminada? Generalize: essa ideia é equivalente a **numerar os recursos e sempre pegá-los em ordem crescente** — mostre que, com essa regra, o filósofo 4 pega primeiro o garfo 0 e depois o 4. Por que essa regra torna impossível formar um ciclo?

**E4 — Quebrando a posse e espera: o "garçom" (no máximo N-1 à mesa).**
Volte à versão original (sem o canhoto) e acrescente um semáforo de contagem `sala`, inicializado com `N_FILOSOFOS - 1`:
```c
sem_t sala;                                   /* junto com o TODO 0  */
sem_init(&sala, 0, N_FILOSOFOS - 1);          /* junto com o TODO 0b */
```
No laço do filósofo, faça `sem_wait(&sala);` **antes** do TODO 1 e `sem_post(&sala);` **depois** do TODO 4.
> **Pergunta 9.** O deadlock desaparece? Por que, se no máximo 4 filósofos tentam pegar garfos ao mesmo tempo, **pelo menos um deles** sempre consegue os dois? Agora inicialize `sala` com `N_FILOSOFOS` (em vez de `N_FILOSOFOS - 1`) e rode de novo: o que acontece e por quê?

**E5 — Pegar, tentar, devolver: livelock.**
Volte à versão original e troque os TODOs 1 e 2 por uma estratégia "educada": pega o garfo esquerdo, **tenta** pegar o direito com `sem_trywait` (que não bloqueia: retorna -1 se o semáforo estiver em 0) e, se não conseguir, **devolve o esquerdo** e tenta de novo:
```c
int tentativas = 0;
for (;;) {
    tentativas++;
    sem_wait(&garfo[GARFO_ESQ(i)]);
    situacao[i] = COM_GARFO_ESQ;
    if (ATRASO_ENTRE_GARFOS_US > 0) usleep(ATRASO_ENTRE_GARFOS_US);

    if (sem_trywait(&garfo[GARFO_DIR(i)]) == 0)
        break;                              /* conseguiu os dois garfos */

    sem_post(&garfo[GARFO_ESQ(i)]);         /* nao conseguiu: devolve o esquerdo */
    situacao[i] = COM_FOME;
    if (ATRASO_ENTRE_GARFOS_US > 0) usleep(ATRASO_ENTRE_GARFOS_US);
}
if (tentativas > 1)
    printf("[Filosofo %d] precisou de %d tentativas\n", i, tentativas);
```
> **Pergunta 10.** Rode 3 vezes. Observe quantas tentativas alguns filósofos precisam (em nossos testes, até ~30) e o tempo total. Em algumas execuções o watchdog pode até disparar — mas **os filósofos não estão bloqueados**. Qual condição de Coffman essa estratégia elimina? Por que, mesmo assim, o sistema pode ficar sem progresso? Explique a diferença entre **deadlock** e **livelock**.
>
> **Pergunta 11.** Troque a **segunda** linha `if (ATRASO_ENTRE_GARFOS_US > 0) usleep(ATRASO_ENTRE_GARFOS_US);` (a que vem depois de devolver o garfo) por uma espera **aleatória**:
> ```c
> usleep(rand_r(&semente) % (ATRASO_ENTRE_GARFOS_US + 1));
> ```
> O número máximo de tentativas cai? Por que a aleatoriedade quebra o livelock? (Essa é a mesma ideia do *backoff* exponencial aleatório usado pelo Ethernet quando há colisão.)

---

## 7. Parte 3 — Filósofos em C: a solução de Tanenbaum (estados)

A solução de Tanenbaum (livro *Sistemas Operacionais Modernos*) muda o ponto de vista: em vez de proteger cada garfo, ela guarda o **estado** de cada filósofo — `PENSANDO`, `COM_FOME` ou `COMENDO` — em um vetor protegido por um `mutex`. Um filósofo só passa para `COMENDO` se **nenhum dos dois vizinhos** estiver comendo, ou seja, pega os dois garfos **de uma vez só, ou nenhum**. Se não puder comer, ele bloqueia em um semáforo **próprio**, `s[i]`, e será acordado por um vizinho quando esse vizinho devolver os garfos.

```
pegar_garfos(i):                    devolver_garfos(i):
  down(mutex)                         down(mutex)
  estado[i] = COM_FOME                estado[i] = PENSANDO
  testar(i)                           testar(ESQUERDO(i))
  up(mutex)                           testar(DIREITO(i))
  down(s[i])                          up(mutex)

testar(i):
  se estado[i] == COM_FOME e nenhum vizinho esta COMENDO:
      estado[i] = COMENDO
      up(s[i])
```

### 7.1 Rodando o código base sem nenhuma sincronização

```bash
make base
./base/filosofos_estados
```

> **Pergunta 12.** Rode algumas vezes. Os erros aparecem mesmo existindo a função `testar()`, que verifica se os vizinhos estão comendo. Por que essa verificação **sozinha** não garante nada? (Dica: o que acontece hoje, no código base, quando `testar(i)` **não** consegue colocar o filósofo `i` em `COMENDO`? Ele espera?)

### 7.2 Preenchendo os TODOs

Abra [`c/base/filosofos_estados.c`](c/base/filosofos_estados.c) e resolva os TODOs:

| TODO | O que fazer | Onde |
|---|---|---|
| 0 / 0b | Declarar `sem_t mutex;` e `sem_t s[N_FILOSOFOS];` e inicializá-los | topo do arquivo / início de `main` |
| 1 | `sem_post(&s[i])` — autoriza o filósofo `i` a comer | dentro do `if` de `testar` |
| 2 | `sem_wait(&mutex)` | início de `pegar_garfos` |
| 3 | `sem_post(&mutex)` | depois de `testar(i)` em `pegar_garfos` |
| 4 | `sem_wait(&s[i])` — bloqueia se não conseguiu os garfos | final de `pegar_garfos` |
| 5 | `sem_wait(&mutex)` | início de `devolver_garfos` |
| 6 | `sem_post(&mutex)` | final de `devolver_garfos` |
| 7 | `sem_destroy` em todos os semáforos (boa prática) | final de `main` |

Valores iniciais corretos (TODO 0b): `mutex = 1` e `s[i] = 0` para todo `i`.

> **Pergunta 13.** Rode 5 vezes (e depois 200 vezes com o laço da Seção 4). Nenhum erro e nenhum travamento devem aparecer, e `Max. filosofos comendo juntos` deve ser 2.
>
> **Pergunta 14.** Repare em um detalhe curioso: quando o filósofo consegue os garfos **logo de cara**, quem faz o `sem_post(&s[i])` é **ele mesmo** (dentro de `testar(i)`), e o `sem_wait(&s[i])` logo depois passa direto. Quando ele **não** consegue, quem faz o `sem_post` é um **vizinho**, dentro de `devolver_garfos`. Por que é importante que o `sem_post` "adiantado" não se perca, mesmo sendo feito **antes** do `sem_wait`? Isso funcionaria com `wait()`/`notify()` de Java?

### 7.3 Experimentos — inserindo atrasos e provocando erros

Mesmo procedimento: altere, rode, observe e **restaure** antes do próximo.

**E6 — Atraso dentro da região crítica, com sincronização correta.**
Mude `#define ATRASO_TESTAR_US 0` para `1000` (1 ms). Esse atraso fica **entre** a verificação dos vizinhos e a atribuição `estado[i] = COMENDO`.
> **Pergunta 15.** O resultado continua correto? O que mudou no tempo total? Explique por que esse atraso não causa erro **enquanto o `mutex` estiver lá**.

**E7 — Removendo o `mutex`, mantendo `s[i]`.**
Mantenha `ATRASO_TESTAR_US = 1000` e comente as quatro linhas `sem_wait(&mutex)` / `sem_post(&mutex)` (TODOs 2, 3, 5 e 6).
> **Pergunta 16.** Aparecem `*** ERRO ***`? Reconstrua o cenário: os filósofos 1 e 2 (vizinhos) executam `testar` ao mesmo tempo; cada um verifica que o outro **ainda não** está `COMENDO`, os dois esperam 1 ms, e então… Compare com o experimento E4 do laboratório anterior (produtor-consumidor sem `mutex`): é a mesma categoria de erro?

**E8 — Inicialização errada de `s[i]`.**
Inicialize os semáforos dos filósofos com 1 em vez de 0: `sem_init(&s[i], 0, 1);`
> **Pergunta 17.** O que acontece? Qual é o significado de `s[i] = 1` "no início do jantar", em termos do problema? Por que o primeiro `sem_wait(&s[i])` de cada filósofo deixa de bloquear quando deveria?

**E9 — Esquecendo de avisar um vizinho.**
Em `devolver_garfos`, comente a linha `testar(ESQUERDO(i));`.
> **Pergunta 18.** Rode algumas vezes (o problema não aparece em toda execução). Leia a fotografia do watchdog: algum filósofo está `COM_FOME`, bloqueado em `pegar_garfos()`, com **os dois vizinhos `PENSANDO`** (ou já satisfeitos)? Por que ninguém o acorda? Esse é um caso de deadlock ou de *lost wakeup*? Justifique.

**E10 — Bloqueando segurando o `mutex`.**
Em `pegar_garfos`, mova o `sem_wait(&s[i]);` (TODO 4) para **antes** do `sem_post(&mutex);` (TODO 3).
> **Pergunta 19.** O programa trava? Explique o ciclo de espera: o filósofo que não conseguiu os garfos dorme em `s[i]` **segurando o `mutex`**; quem poderia acordá-lo precisa de quê? Compare com o experimento E2 do laboratório anterior (`mutex` antes de `vazio` no produtor): qual é a regra geral que os dois experimentos ensinam?

**Inanição.**
> **Pergunta 20.** Olhe a coluna `Maior espera (ms)` na saída da solução correta. A solução de Tanenbaum é livre de deadlock, mas **não** é livre de inanição. Descreva uma sequência em que os filósofos 0 e 2 se revezam de tal forma que o filósofo 1 nunca encontra os dois vizinhos sem comer. Por que isso é difícil de observar na prática, mas é possível em teoria?

### 7.4 Consultando a solução de referência

Depois de concluir as Partes 2 e 3, compare com [`c/solucao/filosofos_garfos.c`](c/solucao/filosofos_garfos.c) e [`c/solucao/filosofos_estados.c`](c/solucao/filosofos_estados.c):

```bash
make solucao
./solucao/filosofos_garfos
./solucao/filosofos_estados
```

> **Pergunta 21.** A solução de referência de `filosofos_garfos.c` é "correta"? Em que sentido sim e em que sentido não? Das três correções que você testou (canhoto, garçom, Tanenbaum), qual permite o **maior paralelismo** e qual é a **mais simples** de implementar?

---

## 8. Parte 4 — Filósofos em Java com monitores

Em Java, a solução por estados fica bem mais curta: não é preciso `mutex` explícito (o `synchronized` já garante exclusão mútua), nem um semáforo por filósofo, nem a função `testar()` — basta o filósofo **esperar em laço** enquanto algum vizinho estiver comendo, e quem devolve os garfos avisa a todos com `notifyAll()`.

### 8.1 Rodando o código base sem sincronização

```bash
cd java/filosofos/base
javac *.java
java Main
```

> **Pergunta 22.** Rode 3 a 5 vezes. Os erros são do mesmo tipo dos que você viu na Parte 2.1 (C)? Por que era esperado que sim?

### 8.2 Preenchendo os TODOs

Abra [`java/filosofos/base/Mesa.java`](java/filosofos/base/Mesa.java) e resolva os TODOs:

| TODO | O que fazer | Onde |
|---|---|---|
| 1 | Adicionar `synchronized` à assinatura de `pegarGarfos` | `public synchronized void pegarGarfos(int i)` |
| 2 | `while (estado[esquerdo(i)] == COMENDO \|\| estado[direito(i)] == COMENDO) { wait(); }` | entre `estado[i] = COM_FOME` e `estado[i] = COMENDO` |
| 3 | Adicionar `synchronized` à assinatura de `devolverGarfos` | `public synchronized void devolverGarfos(int i)` |
| 4 | `notifyAll();` | final de `devolverGarfos` |

```bash
javac *.java
java Main
```

> **Pergunta 23.** Rode 5 vezes: nenhum erro, nenhum travamento e no máximo 2 comendo juntos. Agora faça a correspondência entre as duas implementações: na versão Java, o que faz o papel do `mutex`? E do `s[i]`? E da função `testar()`? Por que a versão Java **não precisa** que alguém teste os vizinhos e "autorize" quem está esperando?

### 8.3 Experimentos — inserindo atrasos e provocando erros

Faça cada alteração, rode, observe o resultado e desfaça antes do próximo experimento.

**E11 — Trocar `while` por `if` na condição de espera.**
> **Pergunta 24.** Rode 5 vezes. Aparecem `*** ERRO ***`? Explique com um cenário concreto: o filósofo 1 espera porque **os dois** vizinhos (0 e 2) estão comendo. O filósofo 0 termina e chama `notifyAll()`. O que o filósofo 1 faz ao acordar, se a condição for testada com `if`? Por que o `while` resolve?

**E12 — Usar `notify()` em vez de `notifyAll()`.**
> **Pergunta 25.** Rode 20 vezes (com um laço). Em nossos testes, **não travou nenhuma vez** — mas o tempo total ficou um pouco maior. Isso prova que `notify()` está correto aqui? Descreva o que acontece quando `notify()` acorda um filósofo que **ainda não pode** comer (um vizinho dele continua comendo) enquanto outro, que **poderia** comer, continua dormindo. Guarde essa resposta: você vai comparar com o barbeiro (experimento E23).

**E13 — Esquecer o `synchronized` em um dos métodos.**
Remova `synchronized` **apenas** de `devolverGarfos`.
> **Pergunta 26.** O que acontece? Qual exceção é lançada e em que linha? Por que o compilador **não** reclamou? Relacione com a Pergunta 13 do laboratório anterior.

**E14 — Comer dentro do monitor.**
Em `pegarGarfos`, logo depois de `estado[i] = COMENDO;`, acrescente:
```java
Thread.sleep(Main.TEMPO_COMER_MS);
```
(como se o filósofo comesse **sem sair** do método `synchronized`).
> **Pergunta 27.** O resultado continua correto? Compare o tempo total com o da versão correta (em nossos testes, ~160 ms → ~250 ms). Enquanto um filósofo "come" dentro do monitor, o que acontece com um vizinho do **outro lado da mesa** que quer apenas **devolver** os garfos? Por que a regra prática é "faça dentro do monitor só o mínimo necessário para mudar o estado"?

---

## 9. Parte 5 — Barbeiro sonolento em C com semáforos

A solução clássica (Tanenbaum) usa **três semáforos** e um contador:

- `clientes` (contagem, inicial 0) — quantos clientes estão esperando. **O barbeiro dorme nele.**
- `barbeiros` (inicial 0) — o barbeiro está pronto para atender. **O cliente espera nele** até ser chamado.
- `mutex` (binário, inicial 1) — exclusão mútua no acesso a `esperando`.
- `int esperando` — quantos clientes estão sentados na sala de espera (precisa existir porque **não é possível ler o valor de um semáforo** de forma confiável para decidir se o cliente desiste).

### 9.1 Rodando o código base sem nenhuma sincronização

```bash
cd c
make base
./base/barbeiro
```

> **Pergunta 28.** Rode algumas vezes. O que o barbeiro faz quando não há clientes? Que valores estranhos aparecem para `esperando`? Por que o número de cortes e o número de clientes atendidos não batem? Em um sistema real, qual seria o custo de um barbeiro (servidor) que "não dorme"?

### 9.2 Preenchendo os TODOs

Abra [`c/base/barbeiro.c`](c/base/barbeiro.c) e resolva os TODOs:

| TODO | O que fazer | Onde |
|---|---|---|
| 0 / 0b | Declarar e inicializar `clientes = 0`, `barbeiros = 0`, `mutex = 1` | topo do arquivo / início de `main` |
| 1 | `sem_wait(&clientes)` — dorme até chegar um cliente | início do laço do `barbeiro` |
| 2 | `sem_wait(&mutex)` | antes de `esperando--` |
| 3 | `sem_post(&barbeiros)` — chama o próximo cliente | depois de `esperando--` |
| 4 | `sem_post(&mutex)` | antes de `cortar_cabelo()` |
| 5 | `sem_wait(&mutex)` | início de `cliente` |
| 6 | `sem_post(&clientes)` — avisa (acorda) o barbeiro | depois de `esperando++` |
| 7 | `sem_post(&mutex)` | logo após o TODO 6 |
| 8 | `sem_wait(&barbeiros)` — espera ser chamado | antes de `receber_corte(id)` |
| 9 | `sem_post(&mutex)` — **também** no caminho da desistência | início do `else` |
| 10 | `sem_post(&clientes)` — acorda o barbeiro no fim do expediente | `main`, depois de `encerrar = 1` |
| 11 | `sem_destroy` nos três semáforos (boa prática) | final de `main` |

```bash
make base
./base/barbeiro
```

> **Pergunta 29.** Rode 5 vezes: `Erros detectados` deve ser 0, `esperando final` deve ser 0 e `atendidos + desistiram` deve dar 20. Em seguida, compare com o produtor-consumidor: qual semáforo do barbeiro corresponde ao `cheio`? Existe algum semáforo que corresponda ao `vazio`? Por que **não** existe — o que o cliente faz no lugar de esperar uma vaga?
>
> **Pergunta 30.** Por que o TODO 10 é necessário? O que acontece com uma thread bloqueada em `sem_wait` se ninguém nunca mais fizer `sem_post` naquele semáforo? Essa é uma técnica comum para encerrar threads "servidoras" — por que só mudar a variável `encerrar` não basta?

### 9.3 Experimentos — inserindo atrasos e provocando erros

Mesmo procedimento: altere, rode, observe e **restaure** antes do próximo.

**E15 — Quantidade de cadeiras (desempenho, não corretude).**
Rode com `CADEIRAS` igual a 1, 3 e 10 e anote `Desistiram` e `Tempo total` de cada caso.
> **Pergunta 31.** Como o número de desistências varia (em nossos testes: ~13, ~8 e ~1)? E o tempo total? Algum caso apresentou erro? Relacione com o tamanho da fila de conexões pendentes de um servidor (`listen(sock, backlog)`): o que acontece com uma conexão que chega quando o *backlog* está cheio?

**E16 — Removendo o `mutex`, com atraso no cliente.**
Comente **todos** os `sem_wait(&mutex)` / `sem_post(&mutex)` (TODOs 2, 4, 5, 7 e 9) e mude `ATRASO_CLIENTE_US` para `20000`. Esse atraso fica **entre** o teste `esperando < CADEIRAS` e o `esperando++`.
> **Pergunta 32.** Rode 3 vezes. Aparece `esperando` **maior que** `CADEIRAS`? Explique o cenário: vários clientes chegam quase juntos, todos veem uma cadeira livre… Esse é o padrão "verificar-e-depois-agir" (*check-then-act*). Dê outro exemplo do mundo real com o mesmo defeito (ex.: reserva de assentos, saque em conta bancária).

**E17 — Cliente espera o barbeiro segurando o `mutex`.**
No cliente, mova o `sem_wait(&barbeiros);` (TODO 8) para **antes** do `sem_post(&mutex);` (TODO 7).
> **Pergunta 33.** O programa trava? Pela fotografia do watchdog, onde está parado o barbeiro (ele acordou?) e onde estão os clientes? Desenhe o ciclo de espera. É o mesmo erro do E10 e do E2 do laboratório anterior?

**E18 e E19 — Inicialização errada.**
Primeiro, inicialize `clientes` com 1 (`sem_init(&clientes, 0, 1)`). Depois restaure e inicialize `barbeiros` com 1.
> **Pergunta 34.** Explique, para cada caso, a mensagem de erro que aparece. Com `clientes = 1`, o que o barbeiro "acha" que existe logo no início? Com `barbeiros = 1`, o que o primeiro cliente consegue fazer sem que o barbeiro o tenha chamado?

**E20 — Esquecendo de acordar o barbeiro no fim do expediente.**
Comente o TODO 10.
> **Pergunta 35.** O que a fotografia do watchdog mostra (barbeiro dormindo? expediente encerrado?)? Por que todos os clientes terminaram normalmente e, mesmo assim, o programa não termina?

**E21 — Trava vazada na desistência.**
Mude `CADEIRAS` para 1 (para forçar desistências) e comente o TODO 9 (`sem_post(&mutex)` no `else`).
> **Pergunta 36.** O que acontece logo após o **primeiro** cliente desistir? Por que esse erro passaria despercebido em um teste com muitas cadeiras (em que ninguém desiste)? Que construção de linguagem (presente em Java, mas não em C) torna esse tipo de esquecimento impossível?

### 9.4 Consultando a solução de referência

```bash
make solucao
./solucao/barbeiro
```

Compare com [`c/solucao/barbeiro.c`](c/solucao/barbeiro.c).

---

## 10. Parte 6 — Barbeiro sonolento em Java com monitores

No monitor, cada cliente que senta recebe uma **senha** (`minhaSenha`), e o barbeiro, ao chamar o próximo, incrementa `chamados`. O cliente espera **até que a sua senha tenha sido chamada** (`chamados > minhaSenha`). Repare que agora existem **duas** situações de espera diferentes no **mesmo** monitor: o barbeiro esperando cliente e o cliente esperando ser chamado.

### 10.1 Rodando o código base sem sincronização

```bash
cd java/barbeiro/base
javac *.java
java Main
```

> **Pergunta 37.** Que erros aparecem? São os mesmos que você viu na versão C (Parte 5.1)?

### 10.2 Preenchendo os TODOs

Abra [`java/barbeiro/base/Barbearia.java`](java/barbeiro/base/Barbearia.java) e resolva os TODOs:

| TODO | O que fazer | Onde |
|---|---|---|
| 1 | Adicionar `synchronized` a `entrar` | `public synchronized boolean entrar(int id)` |
| 2 | `notifyAll();` — acorda o barbeiro | depois de `esperando++` |
| 3 | `while (chamados <= minhaSenha) { wait(); }` — espera a sua vez | antes de `verificador.chamado(...)` |
| 4 | Adicionar `synchronized` a `chamarProximo` | `public synchronized boolean chamarProximo()` |
| 5 | `while (esperando == 0 && !fechada) { wait(); }` — barbeiro dorme | início de `chamarProximo` |
| 6 | `notifyAll();` — avisa os clientes que uma senha foi chamada | depois de `chamados++` |
| 7 | `notifyAll();` — acorda o barbeiro no fim do expediente | dentro de `fechar` |

```bash
javac *.java
java Main
```

Rode 5 vezes: `Erros detectados` deve ser sempre 0.

### 10.3 Experimentos — inserindo atrasos e provocando erros

Faça cada alteração, rode, observe o resultado e desfaça antes do próximo experimento.

**E22 — `if` em vez de `while` na espera do cliente (TODO 3).**
**E23 — `notify()` em vez de `notifyAll()` (nos três pontos).**
**E24 — Cortar o cabelo dentro do monitor:** em `chamarProximo`, logo depois do `notifyAll()` do TODO 6, acrescente `Thread.sleep(Main.TEMPO_CORTE_MS);`. Compare `Desistiram` e `Tempo total` com a versão correta.
**E25 — Esquecer o TODO 7.** Comente o `notifyAll()` de `fechar` e rode 5 vezes. Depois, **mantendo o TODO 7 comentado**, acrescente `Thread.sleep(200);` em `Main.java`, logo antes de `barbearia.fechar();`, e rode de novo.
**E26 — Remover `synchronized` só de `entrar`.**

> **Pergunta 38.** Responda para cada experimento, com base no que você **observou**:
>
> a) **E22:** qual erro aparece? Quem acorda o cliente "antes da hora", se o barbeiro ainda não chamou a senha dele? (Dica: há outros `notifyAll()` no monitor além do barbeiro.)
>
> b) **E23:** em nossos testes, o programa travou em **todas** as execuções. Pela fotografia do watchdog (`WAITING` / `BLOCKED`), quem ficou esperando? Explique por que aqui a troca é fatal e nos filósofos (E12) não foi, considerando que em **um único monitor** Java existe **uma única fila de espera** para condições diferentes (barbeiro esperando cliente × cliente esperando a vez). Como `java.util.concurrent.locks.Condition` (várias condições por lock) resolveria isso sem precisar de `notifyAll()`?
>
> c) **E24:** por que o número de desistências aumenta, se as cadeiras não mudaram? Enquanto o barbeiro corta dentro do monitor, um cliente que acabou de chegar consegue sequer **olhar** se há cadeira livre? Em qual estado (`WAITING` ou `BLOCKED`) ficam esses clientes?
>
> d) **E25:** por que, sem o `sleep(200)`, o programa quase sempre termina normalmente, mesmo com o bug? E por que com o `sleep` ele trava sempre? O que isso ensina sobre bugs que dependem da **temporização** (e sobre a utilidade de inserir atrasos para testá-los)?
>
> e) **E26:** qual exceção aparece? Compare com o E13: a causa é a mesma?

### 10.4 Consultando as soluções de referência (Java)

```bash
cd java/filosofos/solucao && javac *.java && java Main
cd ../../barbeiro/solucao && javac *.java && java Main
```

Compare com [`java/filosofos/solucao/Mesa.java`](java/filosofos/solucao/Mesa.java) e [`java/barbeiro/solucao/Barbearia.java`](java/barbeiro/solucao/Barbearia.java).

---

## 11. Parte 7 — Síntese

Preencha a tabela abaixo com base no que você observou (não apenas teoria) — é um bom resumo para revisar antes da prova:

| Critério | Filósofos (C) | Filósofos (Java) | Barbeiro (C) | Barbeiro (Java) |
|---|---|---|---|---|
| Quantos semáforos / condições de espera existem? | | | | |
| Qual estado compartilhado precisa de exclusão mútua? | | | | |
| Em qual experimento você viu deadlock? Qual foi o ciclo? | | | | |
| Em qual experimento você viu *lost wakeup*? | | | | |
| `if` × `while` fez diferença? | | | | |
| Qual erro foi detectado só pelo watchdog (sem `*** ERRO ***`)? | | | | |

> **Para pensar (sem número, para discutir em sala).**
>
> 1. Nos dois problemas, a regra "**nunca bloqueie esperando por uma condição enquanto segura o `mutex`**" apareceu (E10, E17). Em Java, `wait()` dentro de `synchronized` parece violar essa regra — mas não viola. Por quê?
> 2. Na solução clássica do barbeiro, o cliente "vai embora" logo depois de ser chamado (`receber_corte` retorna na hora), enquanto o barbeiro ainda está cortando. Que semáforo extra seria necessário para o cliente só sair da cadeira **depois** que o corte terminar? (Esse padrão chama-se *rendezvous*.)
> 3. Como você generalizaria o barbeiro para **vários barbeiros**? Quais semáforos mudariam de valor inicial e qual verificação do `Verificador` deixaria de valer?
> 4. Comparando as cinco soluções que você implementou (garfos, canhoto, garçom, Tanenbaum em C, monitor em Java), qual você escolheria para um sistema real e por quê?

---

## 12. Dicas de depuração

- **O programa travou e o watchdog não disparou?** Provavelmente há progresso parcial (algumas threads ainda avançam). Use Ctrl+C e reduza o problema (menos filósofos, menos clientes).
- **Muitas linhas de log difíceis de ler?** Filtre a saída: `./base/barbeiro | grep -E "ERRO|DEADLOCK|Erros detectados"`.
- **Para Java**, além da fotografia do watchdog, você pode obter o estado de todas as threads de um programa travado com `jstack <pid>` (descubra o `pid` com `jps`). O `jstack` também detecta e mostra **deadlocks entre monitores** automaticamente.
- **Restaure o código** depois de cada experimento. Uma boa prática é copiar o arquivo antes (`cp base/barbeiro.c /tmp/barbeiro_ok.c`) ou usar `git stash` / `git checkout -- arquivo`.
