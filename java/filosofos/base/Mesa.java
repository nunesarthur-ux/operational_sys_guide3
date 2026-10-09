/*
 * Mesa.java -- CODIGO BASE (sem sincronizacao)
 * Laboratorio de Problemas Classicos de IPC - Parte 4 (Filosofos com Monitores)
 *
 * Esta classe guarda o ESTADO de cada filosofo (PENSANDO, COM_FOME,
 * COMENDO), como na solucao de Tanenbaum em C. Mas ela AINDA NAO E UM
 * MONITOR: pegarGarfos() nao espera os vizinhos terminarem de comer.
 *
 * Sua tarefa: seguir os TODOs numerados (TODO 1 a TODO 4) e transformar
 * esta classe em um monitor correto, usando 'synchronized', 'wait()' e
 * 'notifyAll()'. Compare com a versao em C: repare que aqui nao existe
 * mutex explicito, nem um semaforo por filosofo, nem a funcao testar().
 */
public class Mesa {

    static final int PENSANDO = 0;
    static final int COM_FOME = 1;
    static final int COMENDO  = 2;

    private final int n;
    private final int[] estado;

    public Mesa(int n) {
        this.n = n;
        this.estado = new int[n];   // todos comecam PENSANDO
    }

    private int esquerdo(int i) { return (i + n - 1) % n; }  // vizinho da esquerda
    private int direito(int i)  { return (i + 1) % n; }      // vizinho da direita

    /* TODO 1: adicione o modificador 'synchronized' a este metodo. */
    public synchronized void pegarGarfos(int i) throws InterruptedException {
        estado[i] = COM_FOME;

        /* TODO 2: enquanto algum vizinho estiver comendo, o filosofo
         *         deve aguardar. Use um laco 'while' (nao 'if'!):
         *
         *   while (estado[esquerdo(i)] == COMENDO || estado[direito(i)] == COMENDO) {
         *       wait();
         *   }
         */
        if (estado[esquerdo(i)] == COMENDO || estado[direito(i)] == COMENDO) {
            wait();
        }
        estado[i] = COMENDO;
    }

    /* TODO 3: adicione o modificador 'synchronized' a este metodo. */
    public synchronized void devolverGarfos(int i) {
        estado[i] = PENSANDO;

        /* TODO 4: avise os filosofos que estao esperando que os garfos
         *         ficaram livres:
         *
         *   notifyAll();
         */
        notifyAll();
    }

    /* Infraestrutura auxiliar (usada pelo watchdog), NAO faz parte do exercicio. */
    public String descreverEstado(int i) {
        return new String[] { "PENSANDO", "COM_FOME", "COMENDO" }[estado[i]];
    }
}
