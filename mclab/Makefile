CFLAGS=-O1 -g
HFILES=$(wildcard *.h)
OFILES=main.o dummy_buffer.o buffer.o

all: main

%.o: %.c $(HFILES)
	$(CC) $(CFLAGS) -c -o $@ $<

main: $(OFILES)
	$(CC) $(CFLAGS) -o $@ $(OFILES) -lpthread

clean:
	rm -rf $(OFILES) main *~

