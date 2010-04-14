CC=gcc
CFLAGS=-O0 -g

all: fstest

bios.o: bios.c fs.h
	${CC} ${CFLAGS} -c -o bios.o bios.c

fsdriver.o: fsdriver.c fs.h
	${CC} ${CFLAGS} -c -o fsdriver.o fsdriver.c

fstest: bios.o fsdriver.o fstest.c
	${CC} ${CFLAGS} -o fstest fstest.c bios.o fsdriver.o

clean:
	rm -f *.o fstest *~

.PHONY: clean
