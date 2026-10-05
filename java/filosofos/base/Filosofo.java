import java.util.concurrent.ThreadLocalRandom;

public class Filosofo implements Runnable {

    private final Mesa mesa;
    private final int id;
    private final Verificador verificador;

    public Filosofo(Mesa mesa, int id, Verificador verificador) {
        this.mesa = mesa;
        this.id = id;
        this.verificador = verificador;
    }

    @Override
    public void run() {
        try {
            for (int r = 0; r < Main.REFEICOES; r++) {
                Thread.sleep(ThreadLocalRandom.current().nextInt(Main.TEMPO_PENSAR_MS + 1)); // pensar

                long inicioFome = System.nanoTime();
                mesa.pegarGarfos(id);
                verificador.comer(id, inicioFome);   // dura TEMPO_COMER_MS
                mesa.devolverGarfos(id);
            }
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
    }
}
