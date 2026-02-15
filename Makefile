# vibePDA - GNU Makefile (C port) — Alpha
# Targets: Linux (ncurses), FreeDOS (PDCurses), WebAssembly
# Build: make [TARGET=linux] | make TARGET=linux-ia32 | make TARGET=dos | make TARGET=webasm
# Clean: make clean | make clean all | make rebuild
# Install: make install [DESTDIR=] [PREFIX=/usr/local]

PACKAGE     = vibePDA
VERSION     = $(shell cat VERSION 2>/dev/null || echo 0.7.1)
prefix      ?= /usr/local
exec_prefix ?= $(prefix)
bindir      ?= $(exec_prefix)/bin
datarootdir ?= $(prefix)/share
mandir      ?= $(datarootdir)/man

# TARGET: linux (default), linux-ia32, dos, webasm
# Linux targets use ncurses; DOS uses PDCurses
TARGET ?= linux

CC     ?= gcc
CFLAGS ?= -Wall -Wextra -std=c11 -O2
CPPFLAGS += -I. -Isrc -D_GNU_SOURCE

SRC = src/main.c src/tui.c src/app.c src/vibe_config.c
OBJ = $(SRC:.c=.o)

ifeq ($(TARGET),webasm)
  CC     = emcc
  CFLAGS = -Wall -Wextra -std=c11 -O2
  CPPFLAGS += -DPLATFORM_WASM
  SRC   += src/storage_file.c
  LDFLAGS += -s STANDALONE_WASM=0 -s EXPORTED_FUNCTIONS='["_main"]' -s EXPORTED_RUNTIME_METHODS='["cwrap"]' -s ERROR_ON_UNDEFINED_SYMBOLS=0
  WEBASM_OUT = vibePDA.js vibePDA.wasm
else ifeq ($(TARGET),dos)
  CC   = i586-pc-msdosdjgpp-gcc
  SRC += src/storage_file.c
  CPPFLAGS += -DPLATFORM_DOS -DVT102_CONSOLE
  CFLAGS += -march=i386
  LDFLAGS += -lpdcurses
else
  # linux, linux-ia32: ncurses
  ifeq ($(TARGET),linux-ia32)
    CFLAGS += -m32
    LDFLAGS += -m32
  endif
  SRC += src/storage_file.c
  CPPFLAGS += -DPLATFORM_LINUX -DVT102_CONSOLE
  LDFLAGS += -lncurses
endif

OBJ = $(SRC:.c=.o)

.PHONY: all clean install uninstall dist distcheck test webasm rebuild

# Default goal (make / make all): build program for current TARGET
all: $(if $(filter webasm,$(TARGET)),vibePDA.js,vibePDA)

# Clean rebuild: remove all build artifacts, then build
rebuild: clean all

ifeq ($(TARGET),webasm)
vibePDA.js: config.h $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o vibePDA.js $(OBJ)
else
vibePDA: config.h $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)
endif

%.o: %.c config.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

config.h: config.h.in VERSION
	@echo "Creating config.h from config.h.in"
	sed 's/@PACKAGE@/$(PACKAGE)/g;s/@VERSION@/$(VERSION)/g' config.h.in > config.h

run_tests: config.h
	$(CC) $(CPPFLAGS) -Itests $(CFLAGS) -o run_tests tests/run_tests.c tests/test_app.c tests/test_storage.c tests/fixture_parks.c src/app.c src/tui.c src/storage_file.c -lncurses
test: run_tests vibePDA
	./run_tests
	@./vibePDA --foo 2>&1 | grep -q "unknown argument" || (echo "FAIL: unknown argument not reported"; exit 1)
	@./vibePDA --foo >/dev/null 2>/dev/null; test $$? -eq 1 || (echo "FAIL: unknown argument should exit 1"; exit 1)

clean:
	rm -f vibePDA vibePDA.js vibePDA.wasm run_tests config.h
	rm -f $(OBJ) src/*.o

install: all
	@if [ "$(TARGET)" = webasm ]; then \
		install -d $(DESTDIR)$(datarootdir)/vibePDA; \
		test -f vibePDA.js && install -m 644 vibePDA.js vibePDA.wasm $(DESTDIR)$(datarootdir)/vibePDA/; \
	else \
		install -d $(DESTDIR)$(bindir); \
		install -m 755 vibePDA $(DESTDIR)$(bindir)/; \
	fi

uninstall:
	rm -f $(DESTDIR)$(bindir)/vibePDA
	rm -rf $(DESTDIR)$(datarootdir)/vibePDA

dist:
	$(MAKE) clean
	tar --transform 's,^,$(PACKAGE)-$(VERSION)/,' -czvf $(PACKAGE)-$(VERSION).tar.gz \
		Makefile config.h.in configure.ac Makefile.am src tests README.md PLAN.md CHANGELOG.md VERSION docs

distcheck: dist
	tar xzf $(PACKAGE)-$(VERSION).tar.gz
	cd $(PACKAGE)-$(VERSION) && $(MAKE) && $(MAKE) clean
	rm -rf $(PACKAGE)-$(VERSION)
