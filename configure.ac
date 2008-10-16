AC_INIT([hildon-home], [0.1.2])
AC_CANONICAL_SYSTEM

AM_CONFIG_HEADER(config.h)
AC_CONFIG_SRCDIR([src/hildon-home.c])

AM_MAINTAINER_MODE

AM_INIT_AUTOMAKE

AC_PROG_CC
AC_PROG_INSTALL
AM_PROG_LIBTOOL

AC_HEADER_STDC

AC_PATH_X
AC_PATH_XTRA
AC_SUBST(X_CFLAGS)

#+++++++++++++++
# Misc programs 
#+++++++++++++++

AC_PATH_PROG(GLIB_GENMARSHAL, glib-genmarshal)
AC_PATH_PROG(DBUS_BINDING_TOOL, dbus-binding-tool)

#++++++++++++++++++
# Misc build flags
#++++++++++++++++++

AC_ARG_ENABLE(debug, 
	      [AC_HELP_STRING([--enable-debug],[Debug (default=no)])],
              [hildon_use_debug=yes],[hildon_use_debug=no])

AC_ARG_ENABLE(timestamping,
	      [AC_HELP_STRING([--enable-timestamping],[Define HILDON_USE_TIMESTAMPING (default=no)])],
	      [hildon_use_timestamping=yes],[hildon_use_timestamping=no])

AC_ARG_ENABLE(instrumenting,
	      [AC_HELP_STRING([--enable-instrumenting],[Compile with instrumentation flags (default=no)])],
	      [hildon_use_instrumenting=yes],[hildon_use_instrumenting=no])

if test "x${hildon_use_timestamping}" = "xyes"
then
    CFLAGS="$CFLAGS -DHILDON_USE_TIMESTAMPING"
fi

if test "x${hildon_use_instrumenting}" = "xyes"
then
    CFLAGS="$CFLAGS -Wall -Wmissing-prototypes -Wmissing-declarations -Werror -Wno-format-extra-args -g -finstrument-functions"
else
    CFLAGS="$CFLAGS -Wall -Wmissing-prototypes -Wmissing-declarations -Werror -Wno-format-extra-args"
fi

AC_SUBST(CFLAGS)

#++++++++++++
# i18n setup 
#++++++++++++

ALL_LINGUAS="en_GB"
AC_SUBST(ALL_LINGUAS)
AM_GLIB_GNU_GETTEXT
AC_DEFINE(GETTEXT_PACKAGE, ["maemo-af-desktop"], [Localisation domain])

PKG_CHECK_MODULES(HILDON,
                  [hildon-1], 
                  [AC_DEFINE(HAVE_LIBHILDON, [], [Whether libhildon-1 is present on the system])], 
                  [PKG_CHECK_MODULES(HILDON, hildon-1 >= 2.1.4)])

AC_SUBST(HILDON_LIBS)
AC_SUBST(HILDON_CFLAGS)

PKG_CHECK_MODULES(LIBHILDONDESKTOP,
                  [libhildondesktop-1 >= 2.1.4],
                  [AC_DEFINE(HAVE_LIBHILDONDESKTOP, [], [Whether libhildondesktop-1 is present on the system])], 
                  [])

AC_SUBST(LIBHILDONDESKTOP_LIBS)
AC_SUBST(LIBHILDONDESKTOP_CFLAGS)

PKG_CHECK_MODULES(GNOME_VFS, gnome-vfs-2.0 >= 2.8.3)
AC_SUBST(GNOME_VFS_CFLAGS)
AC_SUBST(GNOME_VFS_LIBS)

PKG_CHECK_MODULES(SQLITE, sqlite3)
AC_SUBST(SQLITE_CFLAGS)
AC_SUBST(SQLITE_LIBS)


#+++++++++++++++++++
# Directories setup
#+++++++++++++++++++

hildondesktopconfdir=${sysconfdir}/hildon-desktop
AC_SUBST(hildondesktopconfdir)

hildonhomedesktopentrydir=`pkg-config libhildondesktop-1 --variable=hildonhomedesktopentrydir`
AC_SUBST(hildonhomedesktopentrydir)
hildonnotificationdesktopentrydir=`pkg-config libhildondesktop-1 --variable=hildonnotificationdesktopentrydir`
AC_SUBST(hildonnotificationdesktopentrydir)

hildondesktoplibdir=`pkg-config libhildondesktop-1 --variable=hildondesktoplibdir`
AC_SUBST(hildondesktoplibdir)

AC_OUTPUT([
Makefile
src/Makefile
src/home.conf
src/home.plugins
src/notification.conf
])
