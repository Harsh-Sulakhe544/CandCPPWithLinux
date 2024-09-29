opt=-O3 -w -Wall -std=c2x

all: clean altermac

altermac: altermac.o 
	cc ${opt} $^ -o $@

altermac.o: altermac.c
	cc ${opt} -c $^

clean:
	rm -f altermac *.o
