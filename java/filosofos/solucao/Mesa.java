/*
 * Mesa.java -- SOLUCAO DE REFERENCIA
 * Laboratorio de Problemas Classicos de IPC - Parte 4 (Filosofos com Monitores)
 *
 * Versao com os TODOs do codigo base resolvidos. So consulte este
 * arquivo depois de ter tentado resolver o codigo base sozinho(a) --
 * ver Parte 4 do roteiro (README.md).
 */
public class Mesa {

    static final int PENSANDO = 0;
    static final int COM_FOME = 1;
    static final int COMENDO  = 2;

    private final int n;
    private final int[] estado;

    public Mesa(int n) {
        this.n = n;
        this.estado = new int[n];
    }

    private int esquerdo(int i) { return (i + n - 1) % n; }
    private int direito(int i)  { return (i + 1) % n; }

    /* TODO 1 resolvido: metodo agora e 'synchronized'. */
    public synchronized void pegarGarfos(int i) throws InterruptedException {
        estado[i] = COM_FOME;

        /* TODO 2 resolvido: espera em laco 'while', nao 'if'. */
        while (estado[esquerdo(i)] == COMENDO || estado[direito(i)] == COMENDO) {
            wait();
        }

        estado[i] = COMENDO;
    }

    /* TODO 3 resolvido: metodo agora e 'synchronized'. */
    public synchronized void devolverGarfos(int i) {
        estado[i] = PENSANDO;

        notifyAll(); /* TODO 4 resolvido */
    }

    public String descreverEstado(int i) {
        return new String[] { "PENSANDO", "COM_FOME", "COMENDO" }[estado[i]];
    }
}
