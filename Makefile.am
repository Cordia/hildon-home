SUBDIRS = src

schemadir = $(datadir)/gconf/schemas
schema_DATA = hildon-home.schemas

servicesdir = $(datadir)/dbus-1/services/
services_DATA = \
		com.nokia.HildonHome.service		\
		org.freedesktop.Notifications.service

install-data-local:
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(srcdir)/$(schema_DATA)

