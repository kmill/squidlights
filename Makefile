CC=gcc
LIBS=-lm
CFLAGS=-O3 -Wall -I include -std=gnu99
TARGETS=

all: lights clients server pd_client

server: src/lights.o src/server.o
	$(CC) $(LIBS) src/lights.o src/server.o -o build/server

lights: src/lights.o testlight yeoldelights

testlight: src/lights/testlight.o
	$(CC) $(LIBS) src/lights.o src/lights/testlight.o -o build/lights/testlight

yeoldelights: src/lights/yeoldelights.o src/lights/yeoldelights.conf
	cp src/lights/yeoldelights.conf build/lights/yeoldelights.conf
	$(CC) $(LIBS) src/lights.o src/lights/yeoldelights.o -o build/lights/yeoldelights

clients: src/clients.o testclient

testclient: src/clients/testclient.o
	$(CC) $(LIBS) src/clients.o src/clients/testclient.o -o build/clients/testclient

.o: $*.c
	$(CC) $(LIBS) $(CFLAGS) $< -o $%

clean:
	rm build/*.o || true

pd_client: src/pd_client.c src/lights.o src/clients.o
	$(CC) $(LIBS) $(CFLAGS) -DPD -W -Wshadow -Wstrict-prototypes -Wno-unused -Wno-parentheses -Wno-switch -o src/pd_client.o -c src/pd_client.c
	$(CC) $(LIBS) -bundle -undefined suppress -flat_namespace -o build/sqlight.pd_darwin src/pd_client.o src/lights.o src/clients.o

install: pd_client
	cp build/sqlight.pd_darwin ~/Library/Pd