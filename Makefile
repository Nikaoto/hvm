# Makefile
CC:= gcc
CFLAGS:= -Wall -pedantic -std=c99 -O2
G_CFLAGS:= -Wall -pedantic -std=c99 -O1

all: hvm

hvm: hvm.c hvm.h file.c
	$(CC) $(CFLAGS) $^ -o hvm

hvm_old: hvm_old.c hvm_old.h file.c
	$(CC) $(CFLAGS) $^ -o hvm_old

hvm_g: hvm.c file.c
	$(CC) $(G_CFLAGS) $^ -g -o hvm_g

clean:
	rm hvm hvm_g

.PHONY: all clean
