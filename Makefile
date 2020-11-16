CC = cc
SHELL = /bin/sh
CFLAGS += -Wall -O3
LDFLAGS += -lm
PREFIX = /usr/local

NAME = fakesteak

SRCS:= $(shell find src -name "*.c")

OBJS = $(SRCS:.c=.o)

all: $(NAME)

$(NAME): $(OBJS)
	mkdir -p bin
	$(CC) $(CFLAGS) -o bin/$(NAME) $(OBJS) $(LDFLAGS)

.o: .c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	find src -name "*.o" -delete -print
	rm -f bin/$(NAME)

install: $(NAME)
	cp bin/$(NAME) $(PREFIX)/bin
	chmod +x $(PREFIX)/bin/$(NAME)

uninstall:
	rm $(PREFIX)/bin/$(NAME)

debug: CFLAGS += -g
debug: clean $(NAME)

.PHONY: all clean install uninstall debug
