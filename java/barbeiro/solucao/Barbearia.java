/*
 * Barbearia.java -- SOLUCAO DE REFERENCIA
 * Laboratorio de Problemas Classicos de IPC - Parte 6 (Barbeiro com Monitores)
 *
 * Versao com os TODOs do codigo base resolvidos. So consulte este
 * arquivo depois de ter tentado resolver o codigo base sozinho(a) --
 * ver Parte 6 do roteiro (README.md).
 */
public class Barbearia {

    private final int cadeiras;
    private final Verificador verificador;

    private int esperando = 0;
    private int proximaSenha = 0;
    private int chamados = 0;
    private volatile boolean fechada = false;

    public Barbearia(int cadeiras, Verificador verificador) {
        this.cadeiras = cadeiras;
        this.verificador = verificador;
    }

    /* TODO 1 resolvido: metodo agora e 'synchronized'. */
    public synchronized boolean entrar(int id) throws InterruptedException {
        if (esperando == cadeiras) {
            return false;
        }

        int minhaSenha = proximaSenha++;
        esperando++;
        verificador.sentou(id, esperando, cadeiras);

        notifyAll(); /* TODO 2 resolvido */

        /* TODO 3 resolvido: espera em laco 'while', nao 'if'. */
        while (chamados <= minhaSenha) {
            wait();
        }

        verificador.chamado(id, minhaSenha, chamados);
        return true;
    }

    /* TODO 4 resolvido: metodo agora e 'synchronized'. */
    public synchronized boolean chamarProximo() throws InterruptedException {

        /* TODO 5 resolvido: o barbeiro dorme enquanto nao ha clientes. */
        while (esperando == 0 && !fechada) {
            wait();
        }

        if (fechada && esperando == 0) {
            return false;
        }

        esperando--;
        chamados++;
        verificador.verificarSala(esperando, cadeiras);

        notifyAll(); /* TODO 6 resolvido */

        return true;
    }

    public synchronized void fechar() {
        fechada = true;

        notifyAll(); /* TODO 7 resolvido */
    }

    public String descrever() {
        return "esperando=" + esperando + ", proximaSenha=" + proximaSenha
                + ", chamados=" + chamados + ", fechada=" + fechada;
    }

    public int getEsperando() {
        return esperando;
    }
}
