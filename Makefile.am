SUBDIRS = src

schemadir = $(datadir)/gconf/schemas
schema_DATA = hildon-home.schemas

install-data-local:
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(srcdir)/$(schema_DATA)

