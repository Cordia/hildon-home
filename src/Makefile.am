hildondesktopconf_DATA = \
	home.conf		\
	home.plugins		\
	notification.conf

bin_PROGRAMS = hildon-home

hildon_home_CFLAGS = \
	$(HILDON_CFLAGS)							\
	$(LIBHILDONDESKTOP_CFLAGS)						\
	$(GNOME_VFS_CFLAGS)							\
	$(SQLITE_CFLAGS)							\
	-DHD_DESKTOP_CONFIG_PATH=\"$(hildondesktopconfdir)\"

BUILT_SOURCES =				\
	hd-notification-manager-glue.h	\
	hd-marshal.c			\
	hd-marshal.h

hd-notification-manager-glue.h: hd-notification-manager.xml
	$(DBUS_BINDING_TOOL) --prefix=hd_notification_manager --mode=glib-server $< > xgen-$(@F)	\
	&& ( cmp -s xgen-$(@F) $@ || cp xgen-$(@F) $@ )							\
	&& rm -f xgen-$(@F)

hd-marshal.c: hd-marshal.list
	$(GLIB_GENMARSHAL) --prefix=hd_cclosure_marshal $< --header --body --internal > xgen-$(@F)	\
	&& ( cmp -s xgen-$(@F) $@ || cp xgen-$(@F) $@ )							\
	&& rm -f xgen-$(@F)

hd-marshal.h: hd-marshal.list
	$(GLIB_GENMARSHAL) --prefix=hd_cclosure_marshal $< --header --internal > xgen-$(@F)		\
	&& ( cmp -s xgen-$(@F) $@ || cp xgen-$(@F) $@ )							\
	&& rm -f xgen-$(@F)

hildon_home_SOURCES = \
	$(BUILT_SOURCES)		\
	hd-incoming-event-window.c	\
	hd-incoming-event-window.h	\
	hd-notification-manager.c	\
	hd-notification-manager.h	\
	hd-system-notifications.c	\
	hd-system-notifications.h	\
	hildon-home.c

hildon_home_LDFLAGS = \
	-rdynamic			\
	$(HILDON_LIBS)	    		\
	$(LIBHILDONDESKTOP_LIBS)	\
	$(SQLITE_LIBS)			\
	$(GNOME_VFS_LIBS)

EXTRA_DIST = \
	hd-notification-manager.xml

CLEANFILES = \
	$(BUILT_SOURCES)
