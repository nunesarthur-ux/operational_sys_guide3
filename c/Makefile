CC=gcc
CFLAGS=-Wall -Wextra -pthread

PROGS=filosofos_garfos filosofos_estados barbeiro

all: base solucao

base:
	for p in $(PROGS); do $(CC) $(CFLAGS) base/$$p.c -o base/$$p || exit 1; done

solucao:
	for p in $(PROGS); do $(CC) $(CFLAGS) solucao/$$p.c -o solucao/$$p || exit 1; done

clean:
	rm -f $(addprefix base/,$(PROGS)) $(addprefix solucao/,$(PROGS))

.PHONY: all base solucao clean
