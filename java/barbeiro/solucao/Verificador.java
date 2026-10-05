import java.util.concurrent.atomic.AtomicInteger;

/*
 * Verificador.java -- Infraestrutura auxiliar de log/verificacao.
 * NAO faz parte do exercicio de sincronizacao: serve apenas para voce
 * enxergar o resultado (correto ou com erro) ao final da execucao.
 */
public class Verificador {

    final AtomicInteger atendidos = new AtomicInteger();
    final AtomicInteger desistentes = new AtomicInteger();
    final AtomicInteger cortes = new AtomicInteger();
    final AtomicInteger erros = new AtomicInteger();
    final AtomicInteger progresso = new AtomicInteger();

    public void sentou(int id, int esperando, int cadeiras) {
        System.out.printf("[Cliente %2d] sentou na sala de espera (esperando=%d)%n", id, esperando);
        verificarSala(esperando, cadeiras);
        progresso.incrementAndGet();
    }

    public void verificarSala(int esperando, int cadeiras) {
        if (esperando < 0 || esperando > cadeiras) {
            erro("esperando=" + esperando + " fora de [0," + cadeiras + "]");
        }
    }

    public void chamado(int id, int minhaSenha, int chamados) {
        System.out.printf("[Cliente %2d] foi chamado para a cadeira do barbeiro (senha %d)%n", id, minhaSenha);
        if (chamados <= minhaSenha) {
            erro("cliente " + id + " sentou na cadeira sem ser chamado pelo barbeiro! (senha "
                    + minhaSenha + ", chamados=" + chamados + ")");
        }
    }

    public void atendido(int id) {
        atendidos.incrementAndGet();
        progresso.incrementAndGet();
    }

    public void desistiu(int id) {
        System.out.printf("[Cliente %2d] sala de espera cheia -- foi embora%n", id);
        desistentes.incrementAndGet();
        progresso.incrementAndGet();
    }

    public void cortarCabelo() throws InterruptedException {
        System.out.printf("[Barbeiro]   cortando cabelo (corte %d)%n", cortes.incrementAndGet());
        Thread.sleep(Main.TEMPO_CORTE_MS);
        progresso.incrementAndGet();
    }

    private void erro(String msg) {
        System.out.println("*** ERRO: " + msg + " ***");
        erros.incrementAndGet();
    }

    public void imprimirResultado(Barbearia barbearia, long tempoTotalNs) {
        System.out.println("\n===== RESULTADO FINAL =====");
        System.out.println("Clientes que chegaram .... " + Main.N_CLIENTES);
        System.out.println("Atendidos ................ " + atendidos.get());
        System.out.println("Desistiram ............... " + desistentes.get());
        System.out.println("Cortes feitos ............ " + cortes.get());
        System.out.println("esperando final .......... " + barbearia.getEsperando() + " (deveria ser 0)");
        if (atendidos.get() + desistentes.get() != Main.N_CLIENTES) {
            erro("atendidos + desistentes != clientes que chegaram");
        }
        if (cortes.get() != atendidos.get()) {
            erro("o barbeiro fez " + cortes.get() + " cortes, mas " + atendidos.get()
                    + " clientes foram atendidos");
        }
        if (barbearia.getEsperando() != 0) erros.incrementAndGet();
        System.out.println("Erros detectados ......... " + erros.get());
        System.out.printf("Tempo total .............. %.1f ms%n", tempoTotalNs / 1e6);
    }
}
