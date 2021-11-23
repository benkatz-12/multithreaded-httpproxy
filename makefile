CC=gcc
CFLAGS=-I.
DEPS = webproxy.h

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

webproxymake: webproxy.o webproxy_f.o
	$(CC) -pthread -o webproxy webproxy.o webproxy_f.o

clean:
	rm ./cache/*
	rm webproxy webproxy_f.o webproxy.o hostname_ip.txt