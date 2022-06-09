CC = gcc
CFLAGS = -Wall -Ofast -march=native

cmp:
	$(CC) $(CFLAGS) -DN=$(N) -o gen gen.c
	$(CC) $(CFLAGS) -o prog run.c -lm -lncurses
