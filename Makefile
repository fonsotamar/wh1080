CC=gcc

all: capture.c
	$(CC) -o capturep capture.c -lwiringPi

