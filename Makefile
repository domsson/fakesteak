CC := cc
CFLAGS += -Wall -O3
LDLIBS := -lm
PREFIX := /usr/local/
NAME := fakesteak


all bin/$(NAME):
	mkdir -p bin
	$(CC) $(CFLAGS) -o bin/$(NAME) src/$(NAME).c $(LDLIBS)

debug: CFLAGS += -g
debug: all


install: bin/$(NAME)
	mkdir -p $(PREFIX)/bin
	cp bin/$(NAME) $(PREFIX)/bin
	chmod +x $(PREFIX)/bin/$(NAME)

uninstall:
	rm $(PREFIX)/bin/$(NAME)     

clean:
	rm -f bin/$(NAME)

.PHONY = all debug install uninstall clean
