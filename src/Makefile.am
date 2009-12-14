hildondesktopconf_DATA = \
	home.conf		\
	home.plugins		\
	home.safe-set		\
	notification.conf	\
	notification.safe-set   \
	notification-groups.conf

bin_PROGRAMS = hildon-home hildon-sv-notification-daemon

hildon_home_CFLAGS = \
	$(HILDON_HOME_CFLAGS)							\
	-DHD_DESKTOP_CONFIG_PATH=\"$(hildondesktopconfdir)\"			\
	-D_XOPEN_SOURCE=500							\
	-DHD_APPLICATIONS_DIR=\"$(datadir)/applications/hildon\"		\
	$(MAEMO_LAUNCHER_CFLAGS)

BUILT_SOURCES =				 \
	hd-hildon-home-dbus-glue.h	 \
	hd-notification-manager-glue.h	 \
	hd-sv-notification-daemon-glue.h \
	hd-marshal.c			 \
	hd-marshal.h

hd-hildon-home-dbus-glue.h: hd-hildon-home-dbus.xml
	$(DBUS_BINDING_TOOL) --prefix=hd_hildon_home_dbus --mode=glib-server $< > xgen-$(@F)		\
	&& ( cmp -s xgen-$(@F) $@ || cp xgen-$(@F) $@ )							\
	&& rm -f xgen-$(@F)

hd-notification-manager-glue.h: hd-notification-manager.xml
	$(DBUS_BINDING_TOOL) --prefix=hd_notification_manager --mode=glib-server $< > xgen-$(@F)	\
	&& ( cmp -s xgen-$(@F) $@ || cp xgen-$(@F) $@ )							\
	&& rm -f xgen-$(@F)

hd-sv-notification-daemon-glue.h: hd-sv-notification-daemon.xml
	$(DBUS_BINDING_TOOL) --prefix=hd_sv_notification_daemon --mode=glib-server $< > xgen-$(@F)	\
	&& ( cmp -s xgen-$(@F) $@ || cp xgen-$(@F) $@ )							\
	&& rm -f xgen-$(@F)

hd-sv-notification-daemon-client.h: hd-sv-notification-daemon.xml
	$(DBUS_BINDING_TOOL) --prefix=hd_sv_notification_daemon --mode=glib-client $< > xgen-$(@F)	\
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
	hd-activate-views-dialog.c	\
	hd-activate-views-dialog.h	\
	hd-applet-manager.c		\
	hd-applet-manager.h		\
	hd-backgrounds.c		\
	hd-backgrounds.h		\
	hd-bookmark-shortcut.c		\
	hd-bookmark-shortcut.h		\
	hd-cairo-surface-cache.c	\
	hd-cairo-surface-cache.h	\
	hd-change-background-dialog.c	\
	hd-change-background-dialog.h	\
	hd-edit-mode-menu.c		\
	hd-edit-mode-menu.h		\
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
	hd-shortcut-widgets.c		\
	hd-shortcut-widgets.h		\
	hd-bookmark-widgets.c		\
	hd-bookmark-widgets.h		\
	hd-led-pattern.c                \
	hd-led-pattern.h		\
	hd-multi-map.c			\
	hd-multi-map.h			\
	hd-widgets.c			\
	hd-widgets.h			\
	hd-install-widgets-dialog.c	\
	hd-install-widgets-dialog.h	\
	hd-time-difference.c		\
	hd-time-difference.h		\
	hd-command-thread-pool.c	\
	hd-command-thread-pool.h	\
	hd-dbus-utils.c			\
	hd-dbus-utils.h			\
	hd-search-service.c		\
	hd-search-service.h		\
	hd-background.c			\
	hd-background.h			\
	hd-file-background.c		\
	hd-file-background.h		\
	hd-imageset-background.c	\
	hd-imageset-background.h	\
	hd-theme-background.c		\
	hd-theme-background.h		\
	hd-wallpaper-background.c	\
	hd-wallpaper-background.h	\
	hd-available-backgrounds.c	\
	hd-available-backgrounds.h	\
	hd-pixbuf-utils.c		\
	hd-pixbuf-utils.h		\
	hd-object-vector.c		\
	hd-object-vector.h		\
	hildon-home.c

nodist_hildon_home_SOURCES = \
	hd-hildon-home-dbus-glue.h	\
	hd-notification-manager-glue.h	\
	hd-marshal.c			\
	hd-marshal.h

hildon_home_LDFLAGS = \
	$(HILDON_HOME_LIBS)		\
	$(MAEMO_LAUNCHER_LIBS)

hildon_sv_notification_daemon_CFLAGS = \
	$(HILDON_SV_NOTIFICATION_DAEMON_CFLAGS)

hildon_sv_notification_daemon_LDFLAGS = \
	$(HILDON_SV_NOTIFICATION_DAEMON_LIBS)

hildon_sv_notification_daemon_SOURCES = \
	hd-sv-notification-daemon.h		\
	hd-sv-notification-daemon.c

nodist_hildon_sv_notification_daemon_SOURCES = \
	hd-sv-notification-daemon-glue.h

EXTRA_DIST = \
	hd-notification-manager.xml \
	hd-hildon-home-dbus.xml \
	hildon-sv-notification-daemon.xml

CLEANFILES = \
	$(BUILT_SOURCES)
