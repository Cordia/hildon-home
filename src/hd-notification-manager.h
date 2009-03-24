/*
 * This file is part of hildon-home
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Based on hildon-desktop-notification-manager.h (hildon-desktop)
 *   Copyright (C) 2007 Nokia Corporation.
 *
 *   Author:  Lucas Rocha <lucas.rocha@nokia.com>
 *            Moises Martinez <moises.martinez@nokia.com>
 *   Contact: Karoliina Salminen <karoliina.t.salminen@nokia.com>
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

#ifndef __HD_NOTIFICATION_MANAGER_H__
#define __HD_NOTIFICATION_MANAGER_H__

#include <glib.h>
#include <glib-object.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib-bindings.h>
#include <libhildondesktop/libhildondesktop.h>

G_BEGIN_DECLS

typedef struct _HDNotificationManager        HDNotificationManager;
typedef struct _HDNotificationManagerClass   HDNotificationManagerClass;
typedef struct _HDNotificationManagerPrivate HDNotificationManagerPrivate;

#define HD_TYPE_NOTIFICATION_MANAGER            (hd_notification_manager_get_type ())
#define HD_NOTIFICATION_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_NOTIFICATION_MANAGER, HDNotificationManager))
#define HD_NOTIFICATION_MANAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  HD_TYPE_NOTIFICATION_MANAGER, HDNotificationManagerClass))
#define HD_IS_NOTIFICATION_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_NOTIFICATION_MANAGER))
#define HD_IS_NOTIFICATION_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  HD_TYPE_NOTIFICATION_MANAGER))
#define HD_NOTIFICATION_MANAGER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  HD_TYPE_NOTIFICATION_MANAGER, HDNotificationManagerClass))

struct _HDNotificationManager 
{
  GObject parent;

  HDNotificationManagerPrivate *priv;
};

struct _HDNotificationManagerClass 
{
  GObjectClass parent_class;

  void (*notified)    (HDNotificationManager *nm,
                       HDNotification        *notification);
};

GType                  hd_notification_manager_get_type              (void);

HDNotificationManager *hd_notification_manager_get                   (void);

void                  hd_notification_manager_db_load                (HDNotificationManager *nm);

gboolean               hd_notification_manager_notify                (HDNotificationManager *nm,
                                                                      const gchar           *app_name,
                                                                      guint                  id,
                                                                      const gchar           *icon,
                                                                      const gchar           *summary,
                                                                      const gchar           *body,
                                                                      gchar                **actions,
                                                                      GHashTable            *hints,
                                                                      gint                   timeout, 
                                                                      DBusGMethodInvocation *context);

gboolean               hd_notification_manager_system_note_infoprint (HDNotificationManager *nm,
                                                                      const gchar           *message,
                                                                      DBusGMethodInvocation *context);

gboolean               hd_notification_manager_system_note_dialog    (HDNotificationManager *nm,
                                                                      const gchar           *message,
                                                                      guint                  type,
                                                                      const gchar           *label,
                                                                      DBusGMethodInvocation *context);

gboolean               hd_notification_manager_get_capabilities      (HDNotificationManager *nm, 
                                                                      gchar               ***caps);

gboolean               hd_notification_manager_get_server_info       (HDNotificationManager *nm,
                                                                      gchar                **out_name,
                                                                      gchar                **out_vendor,
                                                                      gchar                **out_version);

gboolean               hd_notification_manager_close_notification    (HDNotificationManager *nm,
                                                                      guint id, 
                                                                      GError **error);

void                   hd_notification_manager_close_all             (HDNotificationManager *nm);

void                   hd_notification_manager_call_action           (HDNotificationManager *nm,
                                                                      HDNotification        *notification,
                                                                      const gchar           *action_id);

void                   hd_notification_manager_call_dbus_callback    (HDNotificationManager *nm,
                                                                      const gchar           *dbus_call);
void                   hd_notification_manager_call_message          (HDNotificationManager *nm,
                                                                      DBusMessage           *message);

G_END_DECLS

#endif /* __HD_NOTIFICATION_MANAGER_H__ */
