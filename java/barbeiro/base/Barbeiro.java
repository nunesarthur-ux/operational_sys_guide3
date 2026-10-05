public class Barbeiro implements Runnable {

    private final Barbearia barbearia;
    private final Verificador verificador;

    public Barbeiro(Barbearia barbearia, Verificador verificador) {
        this.barbearia = barbearia;
        this.verificador = verificador;
    }

    @Override
    public void run() {
        try {
            while (barbearia.chamarProximo()) {
                verificador.cortarCabelo();   // dura TEMPO_CORTE_MS
            }
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
    }
}
