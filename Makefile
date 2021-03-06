
TARGET := loki_update
VERSION := 1.0.13
IMAGE   := /loki/patch-tools/setup-image
UPDATES := /loki/updates/loki_update
UI_LIBDIR := .
DATADIR := .
LOCALEDIR := $(DATADIR)/locale

os := $(shell uname -s)
arch := $(shell sh print_arch)
libc := $(shell sh print_libc)

SETUPDB = ../loki_setupdb
SNARF = snarf-7.0
OPTFLAGS = -g -O2 -Wall
CFLAGS = $(OPTFLAGS)
ifeq ($(arch),alpha)
CFLAGS += -mcpu=ev4 -Wa,-mall
endif
ifdef MD5SUM
CFLAGS += -DMD5SUM=\"$(MD5SUM)\"
endif
CFLAGS += -I$(SETUPDB) -I$(SNARF) -DVERSION=\"$(VERSION)\" -DDYNAMIC_UI
CFLAGS += -DPROTOTYPES # for snarf
CFLAGS += -DUI_LIBDIR=\"$(UI_LIBDIR)\" -DDATADIR=\"$(DATADIR)\" -DLOCALEDIR=\"$(LOCALEDIR)\"
CFLAGS += $(shell gtk-config --cflags) $(shell libglade-config --cflags)
CFLAGS += $(shell xml-config --cflags)
LFLAGS = -rdynamic
LFLAGS += -Wl,-Bstatic
# Used for non-blocking gethostbyname
# You can find Ares at: ftp://athena-dist.mit.edu/pub/ATHENA/ares
LFLAGS += -L./libs/ares-1.1.0 -lares
LFLAGS += -L$(SETUPDB)/$(arch) -lsetupdb
LFLAGS += $(shell xml-config --libs)
LFLAGS += -lxml -lz
LFLAGS += -Wl,-Bdynamic
LFLAGS += -lm -ldl

TTY_LFLAGS =

GTK_ST_LFLAGS = -Wl,-Bstatic
GTK_ST_LFLAGS += $(shell libglade-config --libs)
GTK_ST_LFLAGS += $(shell gtk-config --libs)
GTK_ST_LFLAGS += -Wl,-Bdynamic

GTK_SH_LFLAGS = -Wl,-Bstatic
GTK_SH_LFLAGS += $(shell libglade-config --libs)
GTK_SH_LFLAGS += -Wl,-Bdynamic
GTK_SH_LFLAGS += $(shell gtk-config --libs)

CORE_OBJS = loki_update.o prefpath.o url_paths.o meta_url.o \
            load_products.o load_patchset.o patchset.o urlset.o \
            update.o gpg_verify.o get_url.o \
            mkdirhier.o text_parse.o log_output.o safe_malloc.o

SNARF_OBJS = $(SNARF)/url.o $(SNARF)/util.o $(SNARF)/llist.o \
             $(SNARF)/file.o $(SNARF)/ftp.o $(SNARF)/gopher.o $(SNARF)/http.o

OBJS = $(CORE_OBJS) $(SNARF_OBJS)

CORE_SRCS = $(CORE_OBJS:.o=.c)

all: $(TARGET) tty_ui.so gtk_st_ui.so gtk_sh_ui.so

gtk_sh_ui.so: gtk_ui.o
	$(CC) -o $@ -shared $^ $(GTK_SH_LFLAGS)

gtk_st_ui.so: gtk_ui.o
	$(CC) -o $@ -shared $^ $(GTK_ST_LFLAGS)

tty_ui.so: tty_ui.o
	$(CC) -o $@ -shared $^ $(TTY_LFLAGS)

$(TARGET): $(SNARF)/snarf $(OBJS)
	$(CC) -o $@ $(OBJS) $(LFLAGS)

$(SNARF)/snarf:
	(cd $(SNARF); test -f Makefile || ./configure; make)

distclean: clean
	rm -f $(TARGET) *.so
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

install-bin: all
	@if [ -d $(IMAGE)/$(TARGET)/bin/$(arch)/$(libc)/ ]; then \
		cp -v $(TARGET) *.so $(IMAGE)/$(TARGET)/bin/$(arch)/$(libc)/; \
		strip $(IMAGE)/$(TARGET)/bin/$(arch)/$(libc)/*; \
	else \
		echo No directory to copy the binary files to.; \
	fi
	@if [ -d $(UPDATES) ]; then \
		rm -rf $(UPDATES)/bin-$(arch)-$(VERSION); \
		mkdir $(UPDATES)/bin-$(arch)-$(VERSION); \
		cp -v $(TARGET) *.so $(UPDATES)/bin-$(arch)-$(VERSION)/; \
		strip $(UPDATES)/bin-$(arch)-$(VERSION)/*; \
	fi
	@echo "Don't forget to update the version in setup.xml to $(VERSION)"

install-data:
	cp -av README icon.xpm $(IMAGE)/$(TARGET)/
	tar zcvf $(IMAGE)/$(TARGET)/data.tar.gz loki_update.glade pixmaps/*.xpm
	tar zcvf $(IMAGE)/$(TARGET)/detect.tar.gz detect/*.txt detect/*.sh detect/*.md5
	for file in `find locale -name $(TARGET).mo -print`; \
        do  path="$(IMAGE)/$(TARGET)/`dirname $$file | sed 's,image/setup.data/,,'`"; \
            mkdirhier $$path; \
            cp -v $$file $$path; \
        done;
	if [ -d $(UPDATES) ]; then \
	        rm -rf $(UPDATES)/data-$(VERSION); \
                mkdir $(UPDATES)/data-$(VERSION); \
	        cp -av README icon.xpm $(UPDATES)/data-$(VERSION); \
	        tar cf - loki_update.glade pixmaps/*.xpm | (cd $(UPDATES)/data-$(VERSION); tar xvf -); \
	        tar cf - detect/*.txt detect/*.sh detect/*.md5 | (cd $(UPDATES)/data-$(VERSION); tar xvf -); \
	        for file in `find locale -name $(TARGET).mo -print`; \
                do  path="$(UPDATES)/data-$(VERSION)/`dirname $$file | sed 's,image/setup.data/,,'`"; \
                    mkdirhier $$path; \
                    cp -v $$file $$path; \
                done; \
	fi

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
