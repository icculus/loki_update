
OS := $(shell uname -s)
ARCH := $(shell sh print_arch)

SETUPDB = ../setupdb
SNARF = snarf-7.0
CFLAGS = -g -Wall
CFLAGS += -I$(SETUPDB) -I$(SNARF)
CFLAGS += $(shell gtk-config --cflags) $(shell libglade-config --cflags)
CFLAGS += $(shell xml-config --cflags)
LFLAGS += -L$(SETUPDB)/$(ARCH) -lsetupdb
LFLAGS += $(shell gtk-config --libs) $(shell libglade-config --libs)
LFLAGS += $(shell xml-config --libs)
# Used for non-blocking gethostbyname
# You can find Ares at: ftp://athena-dist.mit.edu/pub/ATHENA/ares
LFLAGS += -lares

OBJS = loki_update.o gtk_ui.o load_patchset.o patchset.o update.o \
       gpg_verify.o get_url.o text_parse.o mkdirhier.o log_output.o \
       safe_malloc.o \
       $(SNARF_OBJS)

SNARF_OBJS = $(SNARF)/url.o $(SNARF)/util.o $(SNARF)/llist.o \
             $(SNARF)/ftp.o $(SNARF)/gopher.o $(SNARF)/http.o

all: loki_update

loki_update: $(SNARF)/snarf $(OBJS)
	$(CC) -o $@ $(OBJS) $(LFLAGS)

$(SNARF)/snarf:
	(cd $(SNARF); ./configure && make)

distclean: clean
	rm -f loki_update
	-$(MAKE) -C $(SNARF) $@

clean:
	rm -f *.o core
	-$(MAKE) -C $(SNARF) $@
