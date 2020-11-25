CFLAGS += -Wall -O3
LDLIBS := -lm
PREFIX := /usr/local
BINDIR := $(PREFIX)/bin
NAME := fakesteak

all: bin/$(NAME)

bin/$(NAME): src/$(NAME).c 
	mkdir -p bin
	$(CC) $(CFLAGS) -o bin/$(NAME) src/$(NAME).c $(LDLIBS)

debug: CFLAGS += -g
debug: bin/$(NAME)

install: bin/$(NAME)
	mkdir -p $(BINDIR)
	cp bin/$(NAME) $(BINDIR)
	chmod +x $(BINDIR)/$(NAME)

install-strip: install
	strip $(BINDIR)/$(NAME)

uninstall:
	rm $(BINDIR)/$(NAME)     

clean:
	rm -f bin/$(NAME)

.PHONY = all debug install install-strip uninstall clean
