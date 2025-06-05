SRCDIR = src
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(SOURCES:.c=.o)
CFLAGS = -Wall -Wextra -std=c99 -g

netpulse: $(OBJECTS)
	gcc $(OBJECTS) -o netpulse

%.o : %.c
	gcc $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) netpulse

.PHONY: clean