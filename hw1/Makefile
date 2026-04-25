CC = gcc
CFLAGS = -Wall -Wextra -g

all: merger

merger: merger.c merger_parser.c
	$(CC) $(CFLAGS) -o merger merger.c merger_parser.c

clean:
	rm -f merger
