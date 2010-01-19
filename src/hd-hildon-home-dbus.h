/*
 * This file is part of hildon-home
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef __HD_HILDON_HOME_DBUS_H__
#define __HD_HILDON_HOME_DBUS_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _HDHildonHomeDBus        HDHildonHomeDBus;
typedef struct _HDHildonHomeDBusClass   HDHildonHomeDBusClass;
typedef struct _HDHildonHomeDBusPrivate HDHildonHomeDBusPrivate;

#define HD_TYPE_HILDON_HOME_DBUS            (hd_hildon_home_dbus_get_type ())
#define HD_HILDON_HOME_DBUS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_HILDON_HOME_DBUS, HDHildonHomeDBus))
#define HD_HILDON_HOME_DBUS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  HD_TYPE_HILDON_HOME_DBUS, HDHildonHomeDBusClass))
#define HD_IS_HILDON_HOME_DBUS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_HILDON_HOME_DBUS))
#define HD_IS_HILDON_HOME_DBUS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  HD_TYPE_HILDON_HOME_DBUS))
#define HD_HILDON_HOME_DBUS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  HD_TYPE_HILDON_HOME_DBUS, HDHildonHomeDBusClass))

struct _HDHildonHomeDBus 
{
  GObject parent;

  HDHildonHomeDBusPrivate *priv;
};

struct _HDHildonHomeDBusClass 
{
  GObjectClass parent_class;
};

GType             hd_hildon_home_dbus_get_type       (void);

HDHildonHomeDBus *hd_hildon_home_dbus_get            (void);

void              hd_hildon_home_dbus_show_edit_menu (HDHildonHomeDBus      *home,
                                                      guint                  current_view,
                                                      DBusGMethodInvocation *context);
void              hd_hildon_home_dbus_set_background_image (HDHildonHomeDBus      *dbus,
                                                            const char            *uri,
                                                            DBusGMethodInvocation *context);

G_END_DECLS

#endif /* __HD_HILDON_HOME_DBUS_H__ */
