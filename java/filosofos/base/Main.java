public class Main {

    static final int N_FILOSOFOS     = 5;
    static final int REFEICOES       = 10;  // refeicoes por filosofo
    static final int TEMPO_PENSAR_MS = 2;   // tempo maximo (aleatorio) pensando
    static final int TEMPO_COMER_MS  = 2;

    static final int TIMEOUT_WATCHDOG_S = 3;

    public static void main(String[] args) throws InterruptedException {
        Mesa mesa = new Mesa(N_FILOSOFOS);
        Verificador verificador = new Verificador(N_FILOSOFOS);
        Thread[] filosofos = new Thread[N_FILOSOFOS];

        long inicio = System.nanoTime();
        for (int i = 0; i < N_FILOSOFOS; i++) {
            filosofos[i] = new Thread(new Filosofo(mesa, i, verificador), "Filosofo-" + i);
            filosofos[i].start();
        }
        iniciarWatchdog(mesa, verificador, filosofos);

        for (Thread t : filosofos) t.join();
        verificador.imprimirResultado(System.nanoTime() - inicio);
    }

    /* Infraestrutura auxiliar, NAO faz parte do exercicio de sincronizacao:
     * se nenhum filosofo comer por TIMEOUT_WATCHDOG_S segundos, mostra o que
     * cada thread esta fazendo e encerra o programa. */
    static void iniciarWatchdog(Mesa mesa, Verificador verificador, Thread[] filosofos) {
        Thread wd = new Thread(() -> {
            int ultimo = -1, parado = 0;
            while (true) {
                try { Thread.sleep(1000); } catch (InterruptedException e) { return; }
                int atual = verificador.progresso.get();
                if (atual != ultimo) { ultimo = atual; parado = 0; continue; }
                if (++parado < TIMEOUT_WATCHDOG_S) continue;

                System.out.println("\n*** POSSIVEL DEADLOCK: nenhum filosofo comeu nos ultimos "
                        + TIMEOUT_WATCHDOG_S + " s ***");
                for (int i = 0; i < filosofos.length; i++) {
                    System.out.printf("  Filosofo %d: estado=%-8s | thread %s%n", i,
                            mesa.descreverEstado(i), filosofos[i].getState());
                }
                System.out.println("  (WAITING = parado em wait(); BLOCKED = esperando para entrar no monitor;"
                        + " TERMINATED = ja terminou)");
                System.exit(1);
            }
        });
        wd.setDaemon(true);
        wd.start();
    }
}
