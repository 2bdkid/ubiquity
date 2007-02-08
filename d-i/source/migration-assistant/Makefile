ARGS=-g -Wall -D_GNU_SOURCE
LIBXML_LIBS := $(shell xml2-config --libs)
LIBXML_CFLAGS := $(shell xml2-config --cflags)
BIN=$(DESTDIR)/usr/bin

all: ma-import ma-search-items

registry.o : registry.h registry.c
	$(CC) $(ARGS) -c registry.c

ma-search-items : registry.o search-items.c
	$(CC) $(ARGS) -c search-items.c
	$(CC) $(ARGS) -o ma-search-items search-items.o registry.o

ma-import : gaim-import.o windows-import.o firefox-import.o import.o utils.o
	$(CC) $(ARGS) -c import.c
	$(CC) $(ARGS) $(LIBXML_LIBS) -o ma-import gaim-import.o windows-import.o firefox-import.o registry.o import.o utils.o

gaim-import.o : registry.o utils.o gaim-import.c
	$(CC) $(ARGS) $(LIBXML_CFLAGS) -c gaim-import.c

utils.o: utils.c utils.h
	$(CC) $(ARGS) $(LIBXML_CFLAGS) -c utils.c

firefox-import.o: firefox-import.c
	$(CC) $(ARGS) -c firefox-import.c

windows-import.o: windows-import.c utils.o
	$(CC) $(ARGS) -c windows-import.c

strip: ma-import ma-search-items
	strip --remove-section=.comment --remove-section=.note ma-import ma-search-items
clean :
	-rm -f *.o ma-import ma-search-items

install:
	install -d $(BIN)
	cp ma-import $(BIN)
	cp ma-search-items $(BIN)

.PHONY: clean install strip all
