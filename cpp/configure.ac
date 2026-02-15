# vibePDA C port - GNU configure script
# Supports Linux (x86_64, i386) and FreeDOS (DJGPP 32-bit)
AC_INIT([vibePDA], [0.7.1], [], [vibePDA])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([foreign -Wno-portability])

AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

# Optional: SQLite for Linux (use --without-sqlite for DOS or minimal build)
AC_ARG_WITH([sqlite],
  [AS_HELP_STRING([--without-sqlite], [disable SQLite (for DOS or minimal build)])],
  [],
  [with_sqlite=yes])

# Detect host for cross-compilation
AC_CANONICAL_HOST

case "$host_os" in
  *linux*)
    PLATFORM=linux
    if test "$with_sqlite" = yes; then
      AC_CHECK_LIB([sqlite3], [sqlite3_open], [],
        [AC_MSG_WARN([SQLite3 not found; build will use file-based storage])])
    fi
    ;;
  *msdos*|*dos*)
    PLATFORM=dos
    with_sqlite=no
    AC_DEFINE([PLATFORM_DOS], [1], [Building for DOS/FreeDOS])
    ;;
  *)
    PLATFORM=unknown
    ;;
esac

AM_CONDITIONAL([PLATFORM_LINUX], [test "$PLATFORM" = linux])
AM_CONDITIONAL([PLATFORM_DOS], [test "$PLATFORM" = dos])
AM_CONDITIONAL([USE_SQLITE], [test "$with_sqlite" = yes])

AC_SUBST([PLATFORM])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
