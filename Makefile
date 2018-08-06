CFLAGS=-g -Wall -Wextra

default: all
all:
	gcc $(CFLAGS) bytediff.c -o bytediff
install:
	install bytediff /usr/local/bin
