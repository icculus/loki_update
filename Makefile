
OS := $(shell uname -s)
ARCH := $(shell sh print_arch)

SETUPDB = ../setupdb
SNARF = snarf-7.0
CFLAGS = -g -Wall
CFLAGS += -I$(SETUPDB) -I$(SNARF)
CFLAGS += $(shell gtk-config --cflags) $(shell libglade-config --cflags)
CFLAGS += $(shell xml-config --cflags)
LFLAGS += -L$(SETUPDB)/$(ARCH) -lsetupdb
LFLAGS += -Wl,-Bstatic
LFLAGS += -L$(shell libglade-config --prefix)
LFLAGS +=  -lglade
LFLAGS += -L$(shell gtk-config --prefix)
LFLAGS +=  -lgtk -lgdk -rdynamic -lgmodule -lglib 
LFLAGS += -L$(shell xml-config --prefix)
LFLAGS += -lxml -lz
LFLAGS += -Wl,-Bdynamic
LFLAGS += -L/usr/X11R6/lib -lXi -lXext -lX11 -lm -ldl
# Used for non-blocking gethostbyname
# You can find Ares at: ftp://athena-dist.mit.edu/pub/ATHENA/ares
LFLAGS += -lares

OBJS = loki_update.o gtk_ui.o load_products.o load_patchset.o \
       patchset.o update.o gpg_verify.o get_url.o mkdirhier.o \
       text_parse.o log_output.o safe_malloc.o \
       $(SNARF_OBJS)

SNARF_OBJS = $(SNARF)/url.o $(SNARF)/util.o $(SNARF)/llist.o \
             $(SNARF)/ftp.o $(SNARF)/gopher.o $(SNARF)/http.o

all: loki_update

loki_update: $(SNARF)/snarf $(OBJS)
	$(CC) -o $@ $(OBJS) $(LFLAGS)

$(SNARF)/snarf:
	(cd $(SNARF); test -f Makefile || ./configure; make)

distclean: clean
	rm -f loki_update
	-$(MAKE) -C $(SNARF) $@

clean:
	rm -f *.o core
	-$(MAKE) -C $(SNARF) $@
