/*
 * This file is part of hildon-home
 *
 * Copyright (C) 2009 Nokia Corporation.
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

#ifndef __HD_SV_NOTIFICATION_DAEMON_H__
#define __HD_SV_NOTIFICATION_DAEMON_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _HDSVNotificationDaemon        HDSVNotificationDaemon;
typedef struct _HDSVNotificationDaemonClass   HDSVNotificationDaemonClass;
typedef struct _HDSVNotificationDaemonPrivate HDSVNotificationDaemonPrivate;

#define HD_TYPE_SV_NOTIFICATION_DAEMON                (hd_sv_notification_daemon_get_type ())
#define HD_SV_NOTIFICATION_DAEMON(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_SV_NOTIFICATION_DAEMON, HDSVNotificationDaemon))
#define HD_SV_NOTIFICATION_DAEMON_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass),  HD_TYPE_SV_NOTIFICATION_DAEMON, HDSVNotificationDaemonClass))
#define HD_IS_SV_NOTIFICATION_DAEMON(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_SV_NOTIFICATION_DAEMON))
#define HIDOLN_IS_SV_NOTIFICATION_DAEMON_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  HD_TYPE_SV_NOTIFICATION_DAEMON))
#define HD_SV_NOTIFICATION_DAEMON_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj),  HD_TYPE_SV_NOTIFICATION_DAEMON, HDSVNotificationDaemonClass))

struct _HDSVNotificationDaemon 
{
  GObject parent;

  HDSVNotificationDaemonPrivate *priv;
};

struct _HDSVNotificationDaemonClass 
{
  GObjectClass parent_class;
};

GType                   hd_sv_notification_daemon_get_type   (void);

gboolean                hd_sv_notification_daemon_play_event (HDSVNotificationDaemon *nd,
                                                              GHashTable             *hints,
                                                              const gchar            *notification_sender,
                                                              DBusGMethodInvocation  *context);

gboolean               hd_sv_notification_daemon_stop_event  (HDSVNotificationDaemon *nd,
                                                              gint                    id,
                                                              DBusGMethodInvocation  *context);

G_END_DECLS

#endif
