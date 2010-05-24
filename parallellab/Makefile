CC = gcc-4
CFLAGS = -Wall -g -O3 -fopenmp
LDFLAGS = -fopenmp -lgomp -lm

OBJS = parallel.o matrix.o

all: parallel

parallel: $(OBJS)

parallel.o: parallel.c
	$(CC) $(CFLAGS) -c parallel.c

matrix.o: matrix.c
	$(CC) $(CFLAGS) -c matrix.c

clean:
	rm -f *~ *.o parallel core

