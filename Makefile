CC=gcc
LIBS=-lm
CFLAGS=-O3 -I include -std=gnu99
TARGETS=

all: lights server

server: src/lights.o src/server.o
	$(CC) $(LIBS) src/lights.o src/server.o -o build/server

lights: src/lights.o testlight

testlight: src/lights/testlight.o
	$(CC) $(LIBS) src/lights.o src/lights/testlight.o -o build/lights/testlight

.o: $*.c
	$(CC) $(LIBS) $(CFLAGS) $< -o $%

clean:
	rm build/*.o || true
