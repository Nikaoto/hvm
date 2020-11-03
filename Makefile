# Makefile
CC:= gcc
CFLAGS:= -Wall -pedantic -std=c99 -O2

all: hvm

hvm: hvm.c file.c
	$(CC) $(CFLAGS) $^ -o hvm

hvm_g: hvm.c file.c
	$(CC) $(CFLAGS) $^ -g -o hvm_g

.PHONY: all
