
TARGET := loki_update
VERSION := 1.0c
IMAGE   := /loki/patch-tools/setup-image

os := $(shell uname -s)
arch := $(shell sh print_arch)
libc := $(shell sh print_libc)

SETUPDB = ../setupdb
SNARF = snarf-7.0
CFLAGS = -g -O2 -Wall
CFLAGS += -I$(SETUPDB) -I$(SNARF) -DVERSION=\"$(VERSION)\"
CFLAGS += $(shell gtk-config --cflags) $(shell libglade-config --cflags)
CFLAGS += $(shell xml-config --cflags)
LFLAGS += -L$(SETUPDB)/$(arch) -lsetupdb
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

CORE_SRCS = $(CORE_OBJS:.o=.c)

all: $(TARGET)

$(TARGET): $(SNARF)/snarf $(OBJS)
	$(CC) -o $@ $(OBJS) $(LFLAGS)

$(SNARF)/snarf:
	(cd $(SNARF); test -f Makefile || ./configure; make)

distclean: clean
	rm -f $(TARGET)
	-$(MAKE) -C $(SNARF) $@

clean:
	rm -f *.o *.bak core
	-$(MAKE) -C $(SNARF) $@

dist:
	@dist=$(TARGET)-$(VERSION); \
	mkdir ../$$dist; \
	cp -r . ../$$dist; \
	(cd ../$$dist; make distclean; rm -r `find . -name CVS -print`); \
	(cd ..; tar zcvf $$dist.tar.gz $$dist); \
    rm -rf ../$$dist

install-bin: $(TARGET)
	@if [ -d $(IMAGE)/$(TARGET)/bin/$(arch)/$(libc)/ ]; then \
		cp -v $(TARGET) $(IMAGE)/$(TARGET)/bin/$(arch)/$(libc)/; \
		strip $(IMAGE)/$(TARGET)/bin/$(arch)/$(libc)/$(TARGET); \
	else \
		echo No directory to copy the binary files to.; \
	fi

install-data:
	tar zcvf $(IMAGE)/$(TARGET)/data.tar.gz loki_update.glade pixmaps/*.xpm
	tar zcvf $(IMAGE)/$(TARGET)/detect.tar.gz detect/*.txt detect/*.sh detect/*.md5
	for file in `find locale -name $(TARGET).mo -print`; \
        do  path="$(IMAGE)/$(TARGET)/`dirname $$file | sed 's,image/setup.data/,,'`"; \
            mkdirhier $$path; \
            cp -v $$file $$path; \
        done;

# i18n rules

# This is the list of supported locales
LOCALES = fr

po/$(TARGET).po: $(CORE_SRCS) loki_update.glade
	libglade-xgettext loki_update.glade > po/$(TARGET).po
	xgettext -p po -j -d $(TARGET) --keyword=_ $(CORE_SRCS)

update-po: po/$(TARGET).po
	for lang in $(LOCALES); do \
		msgmerge po/$$lang/$(TARGET).po po/$(TARGET).po > po/$$lang/tmp; \
		mv po/$$lang/tmp po/$$lang/$(TARGET).po; \
	done

gettext: po/$(TARGET).po
	for lang in $(LOCALES); do \
		msgfmt -f po/$$lang/$(TARGET).po -o locale/$$lang/LC_MESSAGES/$(TARGET).mo; \
	done