# GNU Makefile.am for vibePDA (Automake)
# Targets: Linux, FreeDOS (DJGPP), WebAssembly (Emscripten)
# Use ./configure --help and ./configure --version.
bin_PROGRAMS = vibePDA
vibePDA_SOURCES = src/main.c src/tui.c src/app.c src/vibe_config.c
vibePDA_CPPFLAGS = -I$(srcdir)/src -I$(builddir)
vibePDA_CFLAGS = -Wall -Wextra -std=c11

vibePDA_SOURCES += src/storage_file.c
if PLATFORM_DOS
vibePDA_CPPFLAGS += -DPLATFORM_DOS -DVT102_CONSOLE
else
if PLATFORM_WASM
vibePDA_CPPFLAGS += -DPLATFORM_WASM
vibePDA_LDFLAGS = -s STANDALONE_WASM=0 -s EXPORTED_FUNCTIONS='["_main"]'
else
vibePDA_CPPFLAGS += -DPLATFORM_LINUX -DVT102_CONSOLE
endif
endif

CLEANFILES = config.h config.log config.status
