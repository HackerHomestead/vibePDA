# GNU Makefile for vibePDA C port
# Targets: VT102 terminal (Linux) and DOS console (FreeDOS)

bin_PROGRAMS = vibePDA
vibePDA_SOURCES = \
	src/main.c \
	src/tui.c \
	src/app.c

vibePDA_CPPFLAGS = -I$(srcdir)/src -I$(builddir)
vibePDA_CFLAGS = -Wall -Wextra -std=c11

if PLATFORM_DOS
vibePDA_SOURCES += src/storage_file.c
vibePDA_CPPFLAGS += -DPLATFORM_DOS -DVT102_CONSOLE
else
if USE_SQLITE
vibePDA_SOURCES += src/storage_sqlite.c
vibePDA_LDADD = -lsqlite3
else
vibePDA_SOURCES += src/storage_file.c
endif
vibePDA_CPPFLAGS += -DPLATFORM_LINUX -DVT102_CONSOLE
endif

# Install docs per GNU standards
dist_doc_DATA = README

# Clean
CLEANFILES = config.h config.h.in config.log config.status Makefile
