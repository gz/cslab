# (c) ETH Zurich, 2006-2007
# Matteo Corti <matteo.corti@id.ethz.ch>

CC=gcc
CFLAGS=-O0 -g -Wall -DDEBUG
SRC=simulator.c simulator.h scheduler.c scheduler.h

all: schedule

schedule: ${SRC}
	${CC} ${CFLAGS} -o schedule simulator.c scheduler.c

schedlab-handout.tar.gz: ${SRC}
	rm -rf schedlab-handout
	rm -f schedlab-handout.tar.gz
	mkdir schedlab-handout
	cp simulator.c simulator.h scheduler.h Makefile generate.pl schedlab-handout
	cp scheduler-students.c schedlab-handout/scheduler.c
	tar cfz schedlab-handout.tar.gz schedlab-handout

clean:
	rm -f *.o schedule *~

.PHONY: clean
