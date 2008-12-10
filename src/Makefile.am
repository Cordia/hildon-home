hildondesktopconf_DATA = \
	home.conf		\
	home.plugins		\
	notification.conf	\
	notification-groups.conf

bin_PROGRAMS = hildon-home

hildon_home_CFLAGS = \
	$(HILDON_HOME_CFLAGS)							\
	-DHD_DESKTOP_CONFIG_PATH=\"$(hildondesktopconfdir)\"			\
	-D_XOPEN_SOURCE=500							\
	-DHD_APPLICATIONS_DIR=\"$(datadir)/applications/hildon\"

BUILT_SOURCES =				\
	hd-hildon-home-dbus-glue.h	\
	hd-notification-manager-glue.h	\
	hd-marshal.c			\
	hd-marshal.h

hd-hildon-home-dbus-glue.h: hd-hildon-home-dbus.xml
	$(DBUS_BINDING_TOOL) --prefix=hd_hildon_home_dbus --mode=glib-server $< > xgen-$(@F)		\
	&& ( cmp -s xgen-$(@F) $@ || cp xgen-$(@F) $@ )							\
	&& rm -f xgen-$(@F)

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
	hd-add-applet-dialog.c		\
	hd-add-applet-dialog.h		\
	hd-add-bookmark-dialog.c	\
	hd-add-bookmark-dialog.h	\
	hd-add-task-dialog.c		\
	hd-add-task-dialog.h		\
	hd-applet-manager.c		\
	hd-applet-manager.h		\
	hd-bookmark-shortcut.c		\
	hd-bookmark-shortcut.h		\
	hd-change-background-dialog.c	\
	hd-change-background-dialog.h	\
	hd-hildon-home-dbus.c		\
	hd-hildon-home-dbus.h		\
	hd-incoming-event-window.c	\
	hd-incoming-event-window.h	\
	hd-incoming-events.c		\
	hd-incoming-events.h		\
	hd-notification-manager.c	\
	hd-notification-manager.h	\
	hd-system-notifications.c	\
	hd-system-notifications.h	\
	hd-task-shortcut.c		\
	hd-task-shortcut.h		\
	hd-task-manager.c		\
	hd-task-manager.h		\
	hd-bookmark-manager.c		\
	hd-bookmark-manager.h		\
	hildon-home.c

hildon_home_LDFLAGS = \
	-rdynamic			\
	$(HILDON_HOME_LIBS)

noinst_PROGRAMS = test-incoming-event

test_incoming_event_CFLAGS = \
	$(HILDON_HOME_CFLAGS)

test_incoming_event_LDFLAGS = \
	-rdynamic			\
	$(HILDON_HOME_LIBS)

test_incoming_event_SOURCES = \
	hd-incoming-event-window.c	\
	hd-incoming-event-window.h	\
	test-incoming-event.c

EXTRA_DIST = \
	hd-notification-manager.xml

CLEANFILES = \
	$(BUILT_SOURCES)
