CC=g++
CFLAGS=-std=c++11 -g -Wall

all: main

main: main.cpp parse.o handle_signal.o evaluate.o joblist.o
	$(CC) $(CFLAGS) -lreadline -o main main.cpp parse.o handle_signal.o evaluate.o joblist.o

parse.o: parse.cpp
	$(CC) $(CFLAGS) -c parse.cpp

handle_signal.o: handle_signal.cpp
	$(CC) $(CFLAGS) -c handle_signal.cpp

evaluate.o: evaluate.cpp
	$(CC) $(CFLAGS) -c evaluate.cpp

joblist.o: joblist.cpp
	$(CC) $(CFLAGS) -c joblist.cpp

clean:
	rm main parse.o handle_signal.o evaluate.o joblist.o