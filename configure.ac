# vibePDA - GNU Autoconf/Automake
# Build targets: 1) Linux  2) FreeDOS (DJGPP)  3) WebAssembly (Emscripten)
# ./configure --help and ./configure --version are provided by Autoconf.
AC_INIT([vibePDA], [0.7.3], [], [vibePDA])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([foreign -Wno-portability])

AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

AC_CANONICAL_HOST

case "$host_os" in
  *linux*)   PLATFORM=linux ;;
  *msdos*|*dos*) PLATFORM=dos ;;
  *emscripten*) PLATFORM=webasm ;;
  *)         PLATFORM=unknown ;;
esac

AM_CONDITIONAL([PLATFORM_LINUX], [test "$PLATFORM" = linux])
AM_CONDITIONAL([PLATFORM_DOS], [test "$PLATFORM" = dos])
AM_CONDITIONAL([PLATFORM_WASM], [test "$PLATFORM" = webasm])
AC_SUBST([PLATFORM])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
