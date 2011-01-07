CC=gcc
LIBS=-lm -llo
CFLAGS=-O3 -Wall -I include -std=gnu99
TARGETS=

all: lights clients server pd_client

server: src/lights.o src/server.o
	$(CC) $(LIBS) src/lights.o src/server.o -o build/server

lights: src/lights.o testlight yeoldelights elmolights

testlight: src/lights/testlight.o
	$(CC) $(LIBS) src/lights.o src/lights/testlight.o -o build/lights/testlight

yeoldelights: src/lights/yeoldelights.o src/lights/yeoldelights.conf
	cp src/lights/yeoldelights.conf build/lights/yeoldelights.conf
	$(CC) $(LIBS) src/lights.o src/lights/yeoldelights.o -o build/lights/yeoldelights

elmolights: src/lights/elmolights.o
	$(CC) $(LIBS) src/lights.o src/lights/elmolights.o -o build/lights/elmolights

clients: src/clients.o testclient sqlights

testclient: src/clients/testclient.o
	$(CC) $(LIBS) src/clients.o src/clients/testclient.o -o build/clients/testclient

sqlights: src/clients/sqlights.o
	$(CC) $(LIBS) src/clients.o src/clients/sqlights.o -o build/clients/sqlights

.o: $*.c
	$(CC) $(LIBS) $(CFLAGS) $< -o $%

clean:
	rm build/*.o || true

pd_client: src/pd_client.c src/lights.o src/clients.o
	$(CC) $(LIBS) $(CFLAGS) -DPD -W -Wshadow -Wstrict-prototypes -Wno-unused -Wno-parentheses -Wno-switch -o src/pd_client.o -c src/pd_client.c
	$(CC) $(LIBS) -bundle -undefined suppress -flat_namespace -o build/sqlight.pd_darwin src/pd_client.o src/lights.o src/clients.o

install: pd_client
	cp build/sqlight.pd_darwin ~/Library/Pd

fullinstall: sqlights install server yeoldelights
	echo "Need to be root to do this (copying to /usr/bin)"
	sudo cp build/clients/sqlights /usr/bin/
	sudo cp build/server /usr/bin/sqserver
	sudo cp build/lights/yeoldelights /usr/bin/sqyelights