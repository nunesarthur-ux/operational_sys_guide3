import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicIntegerArray;

/*
 * Verificador.java -- Infraestrutura auxiliar de log/verificacao.
 * NAO faz parte do exercicio de sincronizacao: serve apenas para voce
 * enxergar o resultado (correto ou com erro) ao final da execucao.
 */
public class Verificador {

    private final int n;
    private final AtomicIntegerArray usoGarfo;
    private final AtomicInteger comendoAgora = new AtomicInteger();
    private final AtomicInteger maxComendoJuntos = new AtomicInteger();
    final AtomicInteger erros = new AtomicInteger();
    final AtomicInteger progresso = new AtomicInteger();
    final int[] refeicoes;
    final long[] maiorEsperaNs;

    public Verificador(int n) {
        this.n = n;
        this.usoGarfo = new AtomicIntegerArray(n);
        this.refeicoes = new int[n];
        this.maiorEsperaNs = new long[n];
    }

    public void comer(int i, long inicioFome) throws InterruptedException {
        maiorEsperaNs[i] = Math.max(maiorEsperaNs[i], System.nanoTime() - inicioFome);
        int esq = i, dir = (i + 1) % n;

        int e = usoGarfo.incrementAndGet(esq);
        int d = usoGarfo.incrementAndGet(dir);
        maxComendoJuntos.accumulateAndGet(comendoAgora.incrementAndGet(), Math::max);

        synchronized (this) {
            System.out.printf("[Filosofo %d] comendo (refeicao %2d) com garfos %d e %d%n",
                    i, refeicoes[i] + 1, esq, dir);
            if (e > 1) erro("garfo " + esq + " usado por dois filosofos ao mesmo tempo!");
            if (d > 1) erro("garfo " + dir + " usado por dois filosofos ao mesmo tempo!");
        }

        Thread.sleep(Main.TEMPO_COMER_MS);

        comendoAgora.decrementAndGet();
        usoGarfo.decrementAndGet(esq);
        usoGarfo.decrementAndGet(dir);
        refeicoes[i]++;
        progresso.incrementAndGet();
    }

    private void erro(String msg) {
        System.out.println("*** ERRO: " + msg + " ***");
        erros.incrementAndGet();
    }

    public void imprimirResultado(long tempoTotalNs) {
        System.out.println("\n===== RESULTADO FINAL =====");
        System.out.println("Filosofo | Refeicoes | Maior espera (ms)");
        for (int i = 0; i < n; i++) {
            System.out.printf("   %d     |    %2d     |   %8.1f%n", i, refeicoes[i], maiorEsperaNs[i] / 1e6);
        }
        System.out.println("Refeicoes esperadas por filosofo ... " + Main.REFEICOES);
        System.out.println("Max. filosofos comendo juntos ...... " + maxComendoJuntos.get()
                + " (limite possivel: " + n / 2 + ")");
        System.out.println("Erros detectados ................... " + erros.get());
        System.out.printf("Tempo total ........................ %.1f ms%n", tempoTotalNs / 1e6);
    }
}
