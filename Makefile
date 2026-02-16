# vibePDA - GNU Makefile (C port) — Alpha
# Targets: Linux (ncurses), FreeDOS (PDCurses), WebAssembly
# Build: make [TARGET=linux] | make TARGET=linux-ia32 | make TARGET=dos | make TARGET=webasm
# Clean: make clean | make clean all | make rebuild
# Install: make install [DESTDIR=] [PREFIX=/usr/local]

PACKAGE     = vibePDA
VERSION     = $(shell cat VERSION 2>/dev/null || echo 0.7.1)
BUILD_KEEP  ?= 5   # Number of archived builds to keep (make BUILD_KEEP=10)
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
  # linux, linux-ia32: ncursesw for UTF-8 box-drawing support
  ifeq ($(TARGET),linux-ia32)
    CFLAGS += -m32
    LDFLAGS += -m32
  endif
  SRC += src/storage_file.c
  CPPFLAGS += -DPLATFORM_LINUX -DVT102_CONSOLE
  ifeq ($(shell pkg-config --exists ncursesw 2>/dev/null && echo 1),1)
    CPPFLAGS += $(shell pkg-config --cflags ncursesw)
    LDFLAGS += $(shell pkg-config --libs ncursesw)
  else
    LDFLAGS += -lncursesw -ltinfo
  endif
endif

OBJ = $(SRC:.c=.o)

.PHONY: all clean install uninstall dist distcheck test test_terminal webasm rebuild demo configure-terminal

# Default goal (make / make all): build program for current TARGET
# For Linux/DOS: archives previous vibePDA as vibePDA_YYYYMMDDHHMMSS, keeps last BUILD_KEEP
all:
	$(MAKE) $(if $(filter webasm,$(TARGET)),vibePDA.js,vibePDA)

# Clean rebuild: remove all build artifacts, then build
rebuild: clean all

ifeq ($(TARGET),webasm)
vibePDA.js: config.h $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o vibePDA.js $(OBJ)
else
vibePDA: config.h $(OBJ)
	@if [ -f vibePDA ]; then \
		ts=$$(date +%Y%m%d%H%M%S); \
		mv vibePDA vibePDA_$$ts; \
		echo "Archived: vibePDA -> vibePDA_$$ts"; \
		kept=0; \
		for f in $$(ls -t vibePDA_[0-9]* 2>/dev/null); do \
			kept=$$((kept+1)); \
			if [ $$kept -gt $(BUILD_KEEP) ]; then rm -f "$$f"; echo "Removed old: $$f"; fi; \
		done; \
	fi
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)
endif

%.o: %.c config.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

config.h: config.h.in VERSION
	@echo "Creating config.h from config.h.in"
	sed 's/@PACKAGE@/$(PACKAGE)/g;s/@VERSION@/$(VERSION)/g' config.h.in > config.h

run_tests: config.h
	$(CC) $(CPPFLAGS) -Itests $(CFLAGS) -o run_tests tests/run_tests.c tests/test_app.c tests/test_storage.c tests/test_fuzz.c tests/test_config.c tests/fixture_parks.c src/app.c src/tui.c src/vibe_config.c src/storage_file.c $(shell pkg-config --exists ncursesw 2>/dev/null && pkg-config --libs ncursesw || echo "-lncursesw -ltinfo")

test_tui: config.h
	$(CC) $(CPPFLAGS) -Itests -Isrc $(CFLAGS) -o test_tui tests/test_tui.c src/app.c src/tui.c src/vibe_config.c src/storage_file.c $(shell pkg-config --exists ncursesw 2>/dev/null && pkg-config --libs ncursesw || echo "-lncursesw -ltinfo")

demo/demo: config.h
	$(CC) $(CPPFLAGS) -Itests -Isrc $(CFLAGS) -o demo/demo demo/demo.c tests/fixture_parks.c src/vibe_config.c src/storage_file.c

demo: demo/demo
	@./demo/demo

test: run_tests vibePDA
	./run_tests
	@./vibePDA --foo 2>&1 | grep -q "unknown argument" || (echo "FAIL: unknown argument not reported"; exit 1)
	@./vibePDA --foo >/dev/null 2>/dev/null; test $$? -eq 1 || (echo "FAIL: unknown argument should exit 1"; exit 1)

test_terminal: vibePDA
	@python3 scripts/terminal_test.py --ascii-fallback

configure-terminal:
	@python3 scripts/configure_terminal.py

clean:
	rm -f vibePDA vibePDA.js vibePDA.wasm run_tests test_tui demo/demo config.h
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
