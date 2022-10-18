CC = gcc-11

all: httpserver

httpserver:
	$(CC) -o httpserver rio.h rio.c in.h in.c kv.h kv.c httpserver.h httpserver.c main.c

debug:
	$(CC) -g rio.h rio.c in.h in.c kv.h kv.c httpserver.h httpserver.c main.c -o debug
	#gdb debug

kvtest:
	$(CC) -o kvtest kv.h kv.c kv_test.c
	

.PHONY: runkvtest
runkvtest: kvtest
	./kvtest

clean:
	rm -f *.out *.o *.gch httpserver kvtest debug