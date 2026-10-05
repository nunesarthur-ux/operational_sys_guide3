import java.util.concurrent.ThreadLocalRandom;

public class Main {

    static final int CADEIRAS             = 3;    // cadeiras na sala de espera
    static final int N_CLIENTES           = 20;
    static final int TEMPO_CORTE_MS       = 20;
    static final int INTERVALO_CHEGADA_MS = 15;   // intervalo maximo (aleatorio) entre chegadas

    static final int TIMEOUT_WATCHDOG_S = 3;

    public static void main(String[] args) throws InterruptedException {
        Verificador verificador = new Verificador();
        Barbearia barbearia = new Barbearia(CADEIRAS, verificador);
        Thread[] clientes = new Thread[N_CLIENTES];

        long inicio = System.nanoTime();
        Thread barbeiro = new Thread(new Barbeiro(barbearia, verificador), "Barbeiro");
        barbeiro.start();
        iniciarWatchdog(barbearia, verificador, barbeiro, clientes);

        for (int i = 0; i < N_CLIENTES; i++) {
            Thread.sleep(ThreadLocalRandom.current().nextInt(INTERVALO_CHEGADA_MS + 1));
            clientes[i] = new Thread(new Cliente(barbearia, i, verificador), "Cliente-" + i);
            clientes[i].start();
        }
        for (Thread t : clientes) t.join();

        barbearia.fechar();   // fim do expediente
        barbeiro.join();

        verificador.imprimirResultado(barbearia, System.nanoTime() - inicio);
    }

    /* Infraestrutura auxiliar, NAO faz parte do exercicio de sincronizacao:
     * se nada acontecer na barbearia por TIMEOUT_WATCHDOG_S segundos,
     * mostra o que cada thread esta fazendo e encerra o programa. */
    static void iniciarWatchdog(Barbearia barbearia, Verificador verificador,
                                Thread barbeiro, Thread[] clientes) {
        Thread wd = new Thread(() -> {
            int ultimo = -1, parado = 0;
            while (true) {
                try { Thread.sleep(1000); } catch (InterruptedException e) { return; }
                int atual = verificador.progresso.get();
                if (atual != ultimo) { ultimo = atual; parado = 0; continue; }
                if (++parado < TIMEOUT_WATCHDOG_S) continue;

                System.out.println("\n*** POSSIVEL DEADLOCK: nada aconteceu na barbearia nos ultimos "
                        + TIMEOUT_WATCHDOG_S + " s ***");
                System.out.println("  Barbearia: " + barbearia.descrever());
                System.out.println("  Barbeiro:  thread " + barbeiro.getState());
                for (int i = 0; i < clientes.length; i++) {
                    if (clientes[i] != null && clientes[i].getState() != Thread.State.TERMINATED) {
                        System.out.println("  Cliente " + i + ": thread " + clientes[i].getState());
                    }
                }
                System.out.println("  (WAITING = parado em wait(); BLOCKED = esperando para entrar no monitor;"
                        + " clientes que ja terminaram nao aparecem)");
                System.exit(1);
            }
        });
        wd.setDaemon(true);
        wd.start();
    }
}
