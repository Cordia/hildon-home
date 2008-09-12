hildondesktopconf_DATA = \
	notification.conf

bin_PROGRAMS = hildon-home

hildon_home_CFLAGS = \
	$(HILDON_CFLAGS)							\
	$(LIBHILDONDESKTOP_CFLAGS)						\
	$(GNOME_VFS_CFLAGS)							\
	$(SQLITE_CFLAGS)							\
	-DHD_DESKTOP_CONFIG_PATH=\"$(hildondesktopconfdir)\"

BUILT_SOURCES = \
	hd-notification-service.h 

hd-notification-service.h: notification-manager.xml
	dbus-binding-tool --prefix=hd_notification_service --mode=glib-server --output=hd-notification-service.h $<

hildon_home_SOURCES = \
	$(BUILT_SOURCES)		\
	hd-notification-manager.c	\
	hd-notification-manager.h	\
	hildon-home.c								

hildon_home_LDFLAGS = \
	-rdynamic           							\
	$(HILDON_LIBS)	    							\
	$(LIBHILDONDESKTOP_LIBS)						\
	$(SQLITE_LIBS)							\
	$(GNOME_VFS_LIBS)
