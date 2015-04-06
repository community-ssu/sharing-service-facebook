AC_INIT([sharing-service-facebook], [0.6.0], [])
AM_INIT_AUTOMAKE([foreign])

# AC_CONFIG_HEADERS([config.h])

AC_DEFINE(_GNU_SOURCE)

# Checks for programs.
AC_ENABLE_SHARED(yes)
AC_ENABLE_STATIC(no)
AC_ISC_POSIX
AC_PROG_CC
AC_HEADER_STDC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL

# Specify dir for local macros
# AC_CONFIG_MACRO_DIR([m4])

PKG_CHECK_MODULES(DEPS, [glib-2.0 gconf-2.0 >= 2.16 hildon-1 libosso libsharing facebookcommon tablet-browser-interface json-glib-1.0 exempi-2.0 gstreamer-0.10])

AC_SUBST(DEPS_CFLAGS)
AC_SUBST(DEPS_LIBS)

pluginsdir=`$PKG_CONFIG libsharing --variable=pluginsdir`
AC_SUBST(pluginsdir)
servicesdir=`$PKG_CONFIG libsharing --variable=servicesdir`
AC_SUBST(servicesdir)

AC_ARG_ENABLE(debug, AC_HELP_STRING([--enable-debug], [Turn on debug messages]),
        enable_debug=yes, enable_debug=no)

if test x$enable_debug = xyes; then
  DEBUG_CFLAGS="-DOSSOLOG_COMPILE"
  AC_SUBST(DEBUG_CFLAGS)
fi

AC_CONFIG_FILES([
Makefile
data/Makefile
data/facebook.service.xml
src/Makefile])

AC_OUTPUT
