/*
 * This file is part of hildon-home
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>

#include <hildon/hildon.h>

#include "hd-incoming-event-window.h"
#include "hd-notification-manager.h"

#include "hd-incoming-events.h"

#define HD_INCOMING_EVENTS_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_INCOMING_EVENTS, HDIncomingEventsPrivate))

typedef struct
{
  GtkWidget *switcher_window;
  GArray    *notification_ids;
} HDIncomingEventCategoryInfo;

typedef struct
{
  gchar *icon;
  gchar *summary;
  gchar *body;
  gchar *category;
} HDIncomingEventNotificationInfo;

struct _HDIncomingEventsPrivate
{
  HDNotificationManager *nm;
  GQueue                *preview_queue;
  GHashTable            *notifications;
  GHashTable            *categories;
};

G_DEFINE_TYPE (HDIncomingEvents, hd_incoming_events, G_TYPE_OBJECT);

static void
notification_closed (HDNotificationManager *nm,
                     guint id,
                     HDIncomingEvents *sn)
{
  gpointer widget;

  widget = g_hash_table_lookup (sn->priv->notifications, GINT_TO_POINTER (id));

  g_hash_table_remove (sn->priv->notifications, GINT_TO_POINTER (id));

  if (GTK_IS_WIDGET (widget))
    {
      gtk_widget_destroy (GTK_WIDGET (widget));
    }
}

static void
preview_window_response (HDIncomingEventWindow *window,
                         gint                   response_id,
                         guint                  notification_id)
{
  if (response_id == GTK_RESPONSE_OK)
    {
      hd_notification_manager_call_action (hd_notification_manager_get (),
                                           notification_id,
                                           "default");
    }
  else if (response_id == GTK_RESPONSE_DELETE_EVENT)
    {
#if 0
      HDIncomingEventNotificationInfo *info;

      /* FIXME: grouping etc */
  g_hash_table_insert (priv->notifications,
                       GUINT_TO_POINTER (id),
                       info);

  /* FIXME display preview */
  preview_window = hd_incoming_event_window_new (FALSE, summary, body,
                                                 NULL, icon); /* FIXME time */
#endif
    }

  gtk_widget_destroy (GTK_WIDGET (window));
}

static void
preview_window_destroy (GtkWidget        *widget,
                        HDIncomingEvents *ie)
{
  HDIncomingEventsPrivate *priv = ie->priv;

  /* Pop the destroyed preview window */
  g_queue_pop_head (priv->preview_queue);

  /* Show the next avialable preview window */
  while (!g_queue_is_empty (priv->preview_queue))
    {
      GtkWidget *next_preview = g_queue_peek_head (priv->preview_queue);

      if (GTK_IS_WIDGET (next_preview))
        {
          gtk_widget_show (next_preview);
          break;
        }
      else
        g_queue_pop_head (priv->preview_queue);
    }
}                       

static void
notification_sent (HDNotificationManager  *nm,
                   const gchar            *app_name,
                   guint                   id,
                   const gchar            *icon,
                   const gchar            *summary,
                   const gchar            *body,
                   gchar                 **actions,
                   GHashTable             *hints,
                   gint                    timeout,
                   HDIncomingEvents       *ie)
{
  HDIncomingEventsPrivate *priv = ie->priv;
  HDIncomingEventNotificationInfo *info;
  const gchar *category;
  GtkWidget *preview_window;

  g_return_if_fail (HD_IS_INCOMING_EVENTS (ie));

  /* Get category string */
  category = g_value_get_string (g_hash_table_lookup (hints, "category"));

  if (g_str_has_prefix (category, "system.note."))
    {
      return;
    }

  info = g_new0 (HDIncomingEventNotificationInfo, 1);
  info->icon = g_strdup (icon);
  info->summary = g_strdup (summary);
  info->body = g_strdup (body);
  info->category = g_strdup (category);

  g_hash_table_insert (priv->notifications,
                       GUINT_TO_POINTER (id),
                       info);

  /* FIXME display preview */
  preview_window = hd_incoming_event_window_new (TRUE, summary, body,
                                                 NULL, icon); /* FIXME time */

  g_signal_connect (preview_window, "response",
                    G_CALLBACK (preview_window_response),
                    GUINT_TO_POINTER (id));
  g_signal_connect (preview_window, "destroy",
                    G_CALLBACK (preview_window_destroy), ie);

  /* Show window when queue is empty */
  if (g_queue_is_empty (priv->preview_queue))
    gtk_widget_show (preview_window); 

  /* Push window on queue */
  g_queue_push_tail (priv->preview_queue, preview_window);
}

static void
hd_incoming_events_class_init (HDIncomingEventsClass *klass)
{
  g_type_class_add_private (klass, sizeof (HDIncomingEventsPrivate));
}

static void
hd_incoming_events_init (HDIncomingEvents *sn)
{
  sn->priv = HD_INCOMING_EVENTS_GET_PRIVATE (sn);

  sn->priv->notifications = g_hash_table_new_full (g_direct_hash, 
                                                   g_direct_equal,
                                                   NULL,
                                                   NULL);

  sn->priv->preview_queue = g_queue_new ();
}

HDIncomingEvents *
hd_incoming_events_get ()
{
  HDIncomingEvents *sn = g_object_new (HD_TYPE_INCOMING_EVENTS, NULL);

  sn->priv->nm = hd_notification_manager_get ();
  g_signal_connect (sn->priv->nm, "notification-sent",
                    G_CALLBACK (notification_sent), sn);
  g_signal_connect (sn->priv->nm, "notification-closed",
                    G_CALLBACK (notification_closed), sn);

  return sn;
}
