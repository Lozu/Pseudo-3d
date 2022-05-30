cmp:
	gcc -Wall -Ofast -march=native -DN=$(N) -o prog gen.c
