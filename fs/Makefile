CC=gcc
CFLAGS=-g -Wall

all:

format: format_main.c format.o
	$(CC) $(CFLAGS) format.o $< -o $@

format.o: format.c
	$(CC) $(CFLAGS) $< -c

fs.o: fs.c
	$(CC) $(CFLAGS) $< -c

tests: tests.c fs.o format.o
	$(CC) $(CFLAGS) $< fs.o format.o -o $@

clean:
	rm format format.o fs.o