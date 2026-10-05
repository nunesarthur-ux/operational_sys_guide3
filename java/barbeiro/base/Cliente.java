public class Cliente implements Runnable {

    private final Barbearia barbearia;
    private final int id;
    private final Verificador verificador;

    public Cliente(Barbearia barbearia, int id, Verificador verificador) {
        this.barbearia = barbearia;
        this.id = id;
        this.verificador = verificador;
    }

    @Override
    public void run() {
        try {
            if (barbearia.entrar(id)) {
                verificador.atendido(id);
            } else {
                verificador.desistiu(id);
            }
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
    }
}
