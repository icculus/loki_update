
TARGET := loki_update
VERSION := \"1.0b\"

OS := $(shell uname -s)
ARCH := $(shell sh print_arch)

SETUPDB = ../setupdb
SNARF = snarf-7.0
CFLAGS = -g -O2 -Wall
CFLAGS += -I$(SETUPDB) -I$(SNARF) -DVERSION=$(VERSION)
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

CORE_OBJS = loki_update.o gtk_ui.o url_paths.o meta_url.o load_products.o \
			load_patchset.o patchset.o urlset.o update.o gpg_verify.o get_url.o \
			mkdirhier.o text_parse.o log_output.o safe_malloc.o

SNARF_OBJS = $(SNARF)/url.o $(SNARF)/util.o $(SNARF)/llist.o \
             $(SNARF)/file.o $(SNARF)/ftp.o $(SNARF)/gopher.o $(SNARF)/http.o

OBJS = $(CORE_OBJS) $(SNARF_OBJS)

all: $(TARGET)

$(TARGET): $(SNARF)/snarf $(OBJS)
	$(CC) -o $@ $(OBJS) $(LFLAGS)

$(SNARF)/snarf:
	(cd $(SNARF); test -f Makefile || ./configure; make)

distclean: clean
	rm -f $(TARGET)
	-$(MAKE) -C $(SNARF) $@

clean:
	rm -f *.o core
	-$(MAKE) -C $(SNARF) $@

# i18n rules

# This is the list of supported locales
LOCALES = fr

po/loki_update.po: $(SRCS) loki_update.glade
	libglade-xgettext loki_update.glade > po/loki_update.po
	xgettext -p po -j -d loki_update --keyword=_ $(CORE_OBJS:.o=.c)

update-po: po/loki_update.po
	for lang in $(LOCALES); do \
		msgmerge po/$$lang/loki_update.po po/loki_update.po > po/$$lang/tmp; \
		mv po/$$lang/tmp po/$$lang/loki_update.po; \
	done

gettext: po/loki_update.po
	for lang in $(LOCALES); do \
		msgfmt -f po/$$lang/loki_update.po -o mo/$$lang/LC_MESSAGES/loki_update.mo; \
	done