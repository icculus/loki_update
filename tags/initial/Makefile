
SETUPDB = ../setupdb
CFLAGS = -g -Wall
CFLAGS += -I$(SETUPDB)
CFLAGS += $(shell xml-config --cflags)
LFLAGS += -L$(SETUPDB) -lsetupdb
LFLAGS += $(shell xml-config --libs) -static

OS := $(shell uname -s)

OBJS = loki_update.o load_patchset.o update.o patchset.o \
       download.o gpg_verify.o get_url.o text_parse.o \
       mkdirhier.o log_output.o

all: loki_update

loki_update: $(OBJS)
	$(CC) -o $@ $^ $(LFLAGS) -static

distclean: clean
	rm -f loki_update

clean:
	rm -f *.o core
