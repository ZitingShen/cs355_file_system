CC=g++
CFLAGS=-std=c++11 -g -Wall
CFLAGS2=-g -Wall

all: main

main: main.o parse.o handle_signal.o evaluate.o joblist.o format.o fs.o fscalls.o
	$(CC) $(CFLAGS) -lreadline -o main main.o parse.o handle_signal.o evaluate.o joblist.o fs.o format.o fscalls.o

main.o: main.cpp
	$(CC) $(CFLAGS) -c main.cpp

parse.o: parse.cpp
	$(CC) $(CFLAGS) -c parse.cpp

handle_signal.o: handle_signal.cpp
	$(CC) $(CFLAGS) -c handle_signal.cpp

evaluate.o: evaluate.cpp
	$(CC) $(CFLAGS) -c evaluate.cpp

joblist.o: joblist.cpp
	$(CC) $(CFLAGS) -c joblist.cpp

fscalls.o: fscalls.cpp
	$(CC) $(CFLAGS) -c fscalls.cpp

format.o: format.c
	gcc $(CFLAGS2) -c format.c -o format.o

fs.o: fs.c
	gcc $(CFLAGS2) -c fs.c -o fs.o

format: format_main.c format.o
	$(CC) $(CFLAGS) format.o $< -o $@

clean:
	rm main parse.o handle_signal.o evaluate.o joblist.o