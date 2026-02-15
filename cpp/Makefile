# vibePDA C port - GNU-style Makefile
# Build: make [TARGET=linux] | make TARGET=linux-ia32 | make TARGET=dos
# Install: make install [DESTDIR=] [PREFIX=/usr/local]

PACKAGE     = vibePDA
VERSION     = 0.7.1
prefix      ?= /usr/local
exec_prefix ?= $(prefix)
bindir      ?= $(exec_prefix)/bin
datarootdir ?= $(prefix)/share
mandir      ?= $(datarootdir)/man

# TARGET: linux (default, native x86_64 or i386), linux-ia32, dos
TARGET ?= linux

CC     ?= gcc
CFLAGS ?= -Wall -Wextra -std=c11 -O2
CPPFLAGS += -I. -Isrc -D_GNU_SOURCE

SRC = src/main.c src/tui.c src/app.c
OBJ = $(SRC:.c=.o)

# Platform and storage
ifeq ($(TARGET),dos)
  CC   = i586-pc-msdosdjgpp-gcc
  SRC += src/storage_file.c
  CPPFLAGS += -DPLATFORM_DOS -DVT102_CONSOLE
  CFLAGS += -march=i386
  LDFLAGS +=
else
  ifeq ($(TARGET),linux-ia32)
    CFLAGS += -m32
    LDFLAGS += -m32
  endif
  SRC += src/storage_file.c
  CPPFLAGS += -DPLATFORM_LINUX -DVT102_CONSOLE
  # Optional SQLite: set USE_SQLITE=1 to link
  ifeq ($(USE_SQLITE),1)
    SRC := $(filter-out src/storage_file.c,$(SRC))
    SRC += src/storage_sqlite.c
    LDFLAGS += -lsqlite3
  endif
endif

OBJ = $(SRC:.c=.o)

.PHONY: all clean install dist distcheck
all: vibePDA

vibePDA: $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJ)

%.o: %.c config.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

config.h: config.h.in
	@echo "Creating config.h from config.h.in"
	sed 's/@PACKAGE@/$(PACKAGE)/g;s/@VERSION@/$(VERSION)/g' config.h.in > config.h

clean:
	rm -f vibePDA $(OBJ) config.h

install: vibePDA
	install -d $(DESTDIR)$(bindir)
	install -m 755 vibePDA $(DESTDIR)$(bindir)/

uninstall:
	rm -f $(DESTDIR)$(bindir)/vibePDA

dist:
	$(MAKE) clean
	tar --transform 's,^,$(PACKAGE)-$(VERSION)/,' -czvf $(PACKAGE)-$(VERSION).tar.gz \
		Makefile config.h.in configure.ac Makefile.am src include README

distcheck: dist
	tar xzf $(PACKAGE)-$(VERSION).tar.gz
	cd $(PACKAGE)-$(VERSION) && $(MAKE) && $(MAKE) clean
	rm -rf $(PACKAGE)-$(VERSION)
