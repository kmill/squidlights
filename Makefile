CC=gcc
LIBS=-lm
CFLAGS=-O3 -I include/ -std=gnu99
TARGETS=

all: candyland chromatest bigchromatest

candyland: $(TARGETS)
	$(CC) $(LIBS) $(TARGETS) -o $@

.o: $*.c
	$(CC) $(LIBS) $(CFLAGS) $< -o $%

chromatest: chromatest.c dmx-eth.o
	$(CC) $(CFLAGS) -c -o chromatest.o chromatest.c
	$(CC) -lm dmx-eth.o chromatest.o -o chromatest

bigchromatest: bigchromatest.c dmx-eth.o
	$(CC) $(CFLAGS) -c -o bigchromatest.o bigchromatest.c
	$(CC) -lm dmx-eth.o bigchromatest.o -o bigchromatest

clean:
	rm build/*.o || true
