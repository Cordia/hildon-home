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

#include <time.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <hildon/hildon.h>

#include "hd-incoming-event-window.h"
#include "hd-notification-manager.h"

#include "hd-incoming-events.h"

#define HD_INCOMING_EVENTS_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_INCOMING_EVENTS, HDIncomingEventsPrivate))

#define NOTIFICATION_GROUP_KEY_DESTINATION "Destination"
#define NOTIFICATION_GROUP_KEY_SUMMARY "Summary"
#define NOTIFICATION_GROUP_KEY_BODY "Body"
#define NOTIFICATION_GROUP_KEY_ICON "Icon"
#define NOTIFICATION_GROUP_KEY_DBUS_CALL "D-Bus-Call"
#define NOTIFICATION_GROUP_KEY_TEXT_DOMAIN "Text-Domain"

typedef struct
{
  gchar *destination;
  gchar *summary;
  gchar *body;
  gchar *icon;
  gchar *dbus_callback;
  gchar *text_domain;
  GtkWidget *switcher_window;
  GPtrArray *notifications;
} HDIncomingEventGroup;

struct _HDIncomingEventsPrivate
{
  GQueue                *preview_queue;
  GHashTable            *groups;

  GPtrArray             *plugins;
};

G_DEFINE_TYPE (HDIncomingEvents, hd_incoming_events, G_TYPE_OBJECT);

static void
group_free (HDIncomingEventGroup *group)
{
  g_free (group->destination);
  g_free (group->summary);
  g_free (group->body);
  g_free (group->icon);
  g_free (group->dbus_callback);
  g_free (group->text_domain);
  if (GTK_IS_WIDGET (group->switcher_window))
    gtk_widget_destroy (group->switcher_window);
  g_ptr_array_free (group->notifications, TRUE);
  g_free (group);
}

static void group_update (HDIncomingEventGroup *group);

static void
group_notification_closed (HDIncomingEventGroup *group,
                           HDNotification       *notification)
{
  g_ptr_array_remove_fast (group->notifications, notification);
  group_update (group);
}

static void
group_window_response (HDIncomingEventWindow *window,
                       gint                   response_id,
                       HDIncomingEventGroup  *group)
{
  guint i;

  if (response_id == GTK_RESPONSE_OK)
    {
      hd_notification_manager_call_dbus_callback (hd_notification_manager_get (),
                                                  group->dbus_callback);
    }

  for (i = 0; i < group->notifications->len; i++)
    {
      HDNotification *notification;

      notification = g_ptr_array_index (group->notifications,
                                        i);

      g_signal_handlers_disconnect_by_func (notification, group_notification_closed, group);

      hd_notification_manager_close_notification (hd_notification_manager_get (),
                                                  hd_notification_get_id (notification),
                                                  NULL);
    }

  g_ptr_array_remove_range (group->notifications,
                            0,
                            group->notifications->len);
  group_update (group);
}

static void
group_update (HDIncomingEventGroup *group)
{
  g_debug ("update group");

  if (!group->notifications->len)
    {
      if (GTK_IS_WIDGET (group->switcher_window))
        {
          gtk_widget_destroy (group->switcher_window);
          group->switcher_window = NULL;
        }
    }
  else
    {
      if (!GTK_IS_WIDGET (group->switcher_window))
        {
          group->switcher_window = hd_incoming_event_window_new (FALSE,
                                                                 group->destination,
                                                                 NULL, NULL, -1, NULL);
          g_signal_connect (group->switcher_window, "response",
                            G_CALLBACK (group_window_response),
                            group);
        }

      if (group->notifications->len > 1)
        {
          guint i;
          time_t max_time = -2;
          const gchar *latest_summary = "";
          gchar *text_domain;
          gchar *summary;
          gchar *body;

          for (i = 0; i < group->notifications->len; i++)
            {
              HDNotification *notification;
              time_t time;

              notification = g_ptr_array_index (group->notifications,
                                                i);

              time = hd_notification_get_time (notification);
              if (time > max_time)
                {
                  max_time = time;
                  latest_summary = hd_notification_get_summary (notification);
                }
            }

          /* Use default domain if no text domain is set */
          text_domain = group->text_domain ? group->text_domain : GETTEXT_PACKAGE;

          /* Create translated summary and body texts */
          summary = g_strdup_printf (dgettext (text_domain,
                                               group->summary),
                                     group->notifications->len);
          body = g_strdup_printf (dgettext (text_domain,
                                            group->body),
                                  latest_summary);

          g_object_set (group->switcher_window,
                        "title", summary,
                        "message", body,
                        "icon", group->icon,
                        "time", (gint64) max_time,
                        NULL);

          g_free (summary);
          g_free (body);
        }
      else
        {
          HDNotification *notification = g_ptr_array_index (group->notifications,
                                                            0);

          g_object_set (group->switcher_window,
                        "title", hd_notification_get_summary (notification),
                        "message", hd_notification_get_body (notification),
                        "icon", hd_notification_get_icon (notification),
                        "time", hd_notification_get_time (notification),
                        NULL);
        }

      gtk_widget_show (group->switcher_window);
    }
}

static void
switcher_window_response (HDIncomingEventWindow     *window,
                          gint                       response_id,
                          HDNotification            *notification)
{
  if (response_id == GTK_RESPONSE_OK)
    {
      hd_notification_manager_call_action (hd_notification_manager_get (),
                                           notification,
                                           "default");
      hd_notification_manager_close_notification (hd_notification_manager_get (),
                                                  hd_notification_get_id (notification),
                                                  NULL);
    }
  else
    {
      hd_notification_manager_close_notification (hd_notification_manager_get (),
                                                  hd_notification_get_id (notification),
                                                  NULL);
    }
}

typedef struct
{
  HDIncomingEvents *ie;
  HDNotification   *notification;
} PreviewWindowResponseInfo;

static void
preview_window_response (HDIncomingEventWindow     *window,
                         gint                       response_id,
                         PreviewWindowResponseInfo *info)
{
  HDIncomingEvents *ie = info->ie;
  HDNotification *notification = info->notification;

  g_debug ("preview_window_response response_id:%d", response_id);

  g_free (info);

  if (response_id == GTK_RESPONSE_OK)
    {
      hd_notification_manager_call_action (hd_notification_manager_get (),
                                           notification,
                                           "default");
      hd_notification_manager_close_notification (hd_notification_manager_get (),
                                                  hd_notification_get_id (notification),
                                                  NULL);
    }
  else if (response_id == GTK_RESPONSE_DELETE_EVENT)
    {
      HDIncomingEventGroup *group;

      group = g_hash_table_lookup (ie->priv->groups,
                                   hd_notification_get_category (notification));
      g_debug ("group %p, for category %s", group, hd_notification_get_category (notification));

      if (group)
        {
          g_ptr_array_add (group->notifications,
                           notification);
          group_update (group);
          g_signal_connect_swapped (notification, "closed",
                                    G_CALLBACK (group_notification_closed), group);
        }
      else
        {
          GtkWidget *switcher_window;

          switcher_window = hd_incoming_event_window_new (FALSE,
                                                          NULL,
                                                          hd_notification_get_summary (notification),
                                                          hd_notification_get_body (notification),
                                                          hd_notification_get_time (notification),
                                                          hd_notification_get_icon (notification));

          g_signal_connect (switcher_window, "response",
                            G_CALLBACK (switcher_window_response),
                            notification);
          g_signal_connect_object (notification, "closed",
                                   G_CALLBACK (gtk_widget_destroy), switcher_window,
                                   G_CONNECT_SWAPPED);

          gtk_widget_show (switcher_window);
        }
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
hd_incoming_events_notified (HDNotificationManager  *nm,
                             HDNotification         *notification,
                             HDIncomingEvents       *ie)
{
  HDIncomingEventsPrivate *priv = ie->priv;
  const gchar *category;
  GtkWidget *preview_window;
  PreviewWindowResponseInfo *preview_window_response_info;
  guint i;

  g_return_if_fail (HD_IS_INCOMING_EVENTS (ie));

  /* Get category string */
  category = hd_notification_get_category (notification);

  /* Ignore internal system.note. notifications */
  if (g_str_has_prefix (category, "system.note."))
    return;

  /* Call plugins */
  for (i = 0; i < priv->plugins->len; i++)
    {
      HDNotificationPlugin *plugin = g_ptr_array_index (priv->plugins, i);

      hd_notification_plugin_notify (plugin, notification);
    }

  /* Create the notification preview window */
  preview_window = hd_incoming_event_window_new (TRUE,
                                                 NULL,
                                                 hd_notification_get_summary (notification),
                                                 hd_notification_get_body (notification),
                                                 hd_notification_get_time (notification),
                                                 hd_notification_get_icon (notification));

  preview_window_response_info = g_new (PreviewWindowResponseInfo, 1);
  preview_window_response_info->ie = ie;
  preview_window_response_info->notification = notification;

  g_signal_connect (preview_window, "response",
                    G_CALLBACK (preview_window_response),
                    preview_window_response_info);
  g_signal_connect (preview_window, "destroy",
                    G_CALLBACK (preview_window_destroy), ie);
  g_signal_connect_object (notification, "closed",
                           G_CALLBACK (gtk_widget_destroy), preview_window,
                           G_CONNECT_SWAPPED);

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
load_notification_groups (HDIncomingEvents *ie)
{
  HDConfigFile *groups_file;
  GKeyFile *key_file;
  gchar **groups;
  guint i;

  /* Load the config file */
  groups_file = hd_config_file_new (HD_DESKTOP_CONFIG_PATH,
                                    NULL,
                                    "notification-groups.conf");
  key_file = hd_config_file_load_file (groups_file, TRUE);

  if (!key_file)
    {
      g_warning ("Could not load notifications group file");
      return;
    }

  /* Get all groups */
  groups = g_key_file_get_groups (key_file, NULL);

  /* Warning if no groups */
  if (groups == NULL)
    {
      g_warning ("Notification groups file is empty");
      return;
    }

  /* Iterate all groups the group is the org.freedesktop.Notifications.Notify hints category */
  for (i = 0; groups[i]; i++)
    {
      HDIncomingEventGroup *group;
      GError *error = NULL;

      group = g_new0 (HDIncomingEventGroup, 1);

      group->notifications = g_ptr_array_new ();

      group->destination = g_key_file_get_string (key_file,
                                                  groups[i],
                                                  NOTIFICATION_GROUP_KEY_DESTINATION,
                                                  &error);
      if (error)
        goto load_key_error;

      group->summary = g_key_file_get_string (key_file,
                                              groups[i],
                                              NOTIFICATION_GROUP_KEY_SUMMARY,
                                              &error);
      if (error)
        goto load_key_error;

      group->body = g_key_file_get_string (key_file,
                                           groups[i],
                                           NOTIFICATION_GROUP_KEY_BODY,
                                           &error);
      if (error)
        goto load_key_error;

      group->icon = g_key_file_get_string (key_file,
                                           groups[i],
                                           NOTIFICATION_GROUP_KEY_ICON,
                                           &error);
      if (error)
        goto load_key_error;

      group->dbus_callback = g_key_file_get_string (key_file,
                                                    groups[i],
                                                    NOTIFICATION_GROUP_KEY_DBUS_CALL,
                                                    &error);
      if (error)
        goto load_key_error;

      group->text_domain = g_key_file_get_string (key_file,
                                                  groups[i],
                                                  NOTIFICATION_GROUP_KEY_TEXT_DOMAIN,
                                                  NULL);

      g_debug ("Add group %s", groups[i]);
      g_hash_table_insert (ie->priv->groups,
                           groups[i],
                           group);
      continue;

load_key_error:
      g_warning ("Error loading notification groups file: %s", error->message);
      group_free (group);
      g_error_free (error);
      g_free (groups[i]);
    }
  g_free (groups);

  g_key_file_free (key_file);
  g_object_unref (groups_file);
}

static void
hd_incoming_events_init (HDIncomingEvents *ie)
{
  ie->priv = HD_INCOMING_EVENTS_GET_PRIVATE (ie);

  ie->priv->groups = g_hash_table_new_full (g_str_hash,
                                            g_str_equal,
                                            (GDestroyNotify) g_free,
                                            (GDestroyNotify) group_free);

  ie->priv->preview_queue = g_queue_new ();

  ie->priv->plugins = g_ptr_array_new ();

  load_notification_groups (ie);
}

static void
hd_incoming_events_plugin_added (HDPluginManager  *pm,
                                 GObject          *plugin,
                                 HDIncomingEvents *ie)
{
  if (HD_IS_NOTIFICATION_PLUGIN (plugin))
    g_ptr_array_add (ie->priv->plugins, plugin);
  else
    g_warning ("Plugin from type %s is no HDNotificationPlugin", G_OBJECT_TYPE_NAME (plugin));
}

static void
hd_incoming_events_plugin_removed (HDPluginManager  *pm,
                                   GObject          *plugin,
                                   HDIncomingEvents *ie)
{
  if (HD_IS_NOTIFICATION_PLUGIN (plugin))
    g_ptr_array_remove_fast (ie->priv->plugins, plugin);
  else
    g_warning ("Plugin from type %s is no HDNotificationPlugin", G_OBJECT_TYPE_NAME (plugin));
}

HDIncomingEvents *
hd_incoming_events_new (HDPluginManager *pm)
{
  HDIncomingEvents *ie = g_object_new (HD_TYPE_INCOMING_EVENTS, NULL);

  g_signal_connect (hd_notification_manager_get (), "notified",
                    G_CALLBACK (hd_incoming_events_notified), ie);

  g_signal_connect (pm, "plugin-added",
                    G_CALLBACK (hd_incoming_events_plugin_added), ie);
  g_signal_connect (pm, "plugin-removed",
                    G_CALLBACK (hd_incoming_events_plugin_removed), ie);

  return ie;
}
