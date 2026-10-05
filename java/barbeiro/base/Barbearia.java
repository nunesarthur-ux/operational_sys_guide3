/*
 * Barbearia.java -- CODIGO BASE (sem sincronizacao)
 * Laboratorio de Problemas Classicos de IPC - Parte 6 (Barbeiro com Monitores)
 *
 * Esta classe guarda o estado da barbearia: quantos clientes estao
 * sentados na sala de espera e uma "senha" para cada um, para que o
 * barbeiro chame os clientes na ordem de chegada. Ela AINDA NAO E UM
 * MONITOR: o barbeiro nao dorme quando nao ha clientes e os clientes
 * nao esperam ser chamados.
 *
 * Sua tarefa: seguir os TODOs numerados (TODO 1 a TODO 7) e transformar
 * esta classe em um monitor correto, usando 'synchronized', 'wait()' e
 * 'notifyAll()'. Nao altere a logica de senhas/contadores.
 */
public class Barbearia {

    private final int cadeiras;
    private final Verificador verificador;

    private int esperando = 0;      // clientes sentados na sala de espera
    private int proximaSenha = 0;   // senha que sera entregue ao proximo cliente que sentar
    private int chamados = 0;       // quantas senhas o barbeiro ja chamou (0, 1, ..., chamados-1)
    private volatile boolean fechada = false;

    public Barbearia(int cadeiras, Verificador verificador) {
        this.cadeiras = cadeiras;
        this.verificador = verificador;
    }

    /**
     * Chamado pela thread do cliente quando ele chega.
     * Retorna true se o cliente foi atendido, false se desistiu.
     */
    /* TODO 1: adicione o modificador 'synchronized' a este metodo. */
    public boolean entrar(int id) throws InterruptedException {
        if (esperando == cadeiras) {
            return false;                    // sala de espera cheia: vai embora
        }

        int minhaSenha = proximaSenha++;
        esperando++;                         // senta na sala de espera
        verificador.sentou(id, esperando, cadeiras);

        /* TODO 2: avise o barbeiro (que pode estar dormindo) que chegou
         *         um cliente:
         *
         *   notifyAll();
         */

        /* TODO 3: espere ate o barbeiro chamar a sua senha. Use um laco
         *         'while' (nao 'if'!):
         *
         *   while (chamados <= minhaSenha) {
         *       wait();
         *   }
         */

        verificador.chamado(id, minhaSenha, chamados);
        return true;
    }

    /**
     * Chamado pela thread do barbeiro para pegar o proximo cliente.
     * Retorna false quando a barbearia fechou e nao ha mais ninguem.
     */
    /* TODO 4: adicione o modificador 'synchronized' a este metodo. */
    public boolean chamarProximo() throws InterruptedException {

        /* TODO 5: enquanto nao houver clientes (e a barbearia estiver
         *         aberta), o barbeiro dorme:
         *
         *   while (esperando == 0 && !fechada) {
         *       wait();
         *   }
         */

        if (fechada && esperando == 0) {
            return false;                    // fim do expediente
        }

        esperando--;                         // um cliente sai da sala de espera...
        chamados++;                          // ...e a senha dele e chamada
        verificador.verificarSala(esperando, cadeiras);

        /* TODO 6: avise os clientes que uma nova senha foi chamada:
         *
         *   notifyAll();
         */

        return true;
    }

    /** Chamado pelo main no fim do expediente. */
    public synchronized void fechar() {
        fechada = true;

        /* TODO 7: o barbeiro pode estar dormindo (wait() do TODO 5).
         *         Acorde-o para que ele perceba que a barbearia fechou:
         *
         *   notifyAll();
         */
    }

    /* Infraestrutura auxiliar (usada pelo watchdog e pelo resultado final). */
    public String descrever() {
        return "esperando=" + esperando + ", proximaSenha=" + proximaSenha
                + ", chamados=" + chamados + ", fechada=" + fechada;
    }

    public int getEsperando() {
        return esperando;
    }
}
