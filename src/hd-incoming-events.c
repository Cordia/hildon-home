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

#include <dbus/dbus-glib-bindings.h>
#include <mce/dbus-names.h>

#include "hd-incoming-event-window.h"
#include "hd-notification-manager.h"

#include "hd-incoming-events.h"

#define HD_INCOMING_EVENTS_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_INCOMING_EVENTS, HDIncomingEventsPrivate))

#define NOTIFICATION_GROUP_KEY_DESTINATION "Destination"
#define NOTIFICATION_GROUP_KEY_SUMMARY "Summary"
#define NOTIFICATION_GROUP_KEY_BODY "Body"
#define NOTIFICATION_GROUP_KEY_ICON "Icon"
#define NOTIFICATION_GROUP_KEY_EMPTY_SUMMARY "Empty-Summary"
#define NOTIFICATION_GROUP_KEY_PREVIEW_SUMMARY "Preview-Summary"
#define NOTIFICATION_GROUP_KEY_DBUS_CALL "D-Bus-Call"
#define NOTIFICATION_GROUP_KEY_TEXT_DOMAIN "Text-Domain"
#define NOTIFICATION_GROUP_KEY_NO_WINDOW "No-Window"
#define NOTIFICATION_GROUP_KEY_ACCOUNT_CALL "Account-Call"
#define NOTIFICATION_GROUP_KEY_ACCOUNT_HINT "Account-Hint"
#define NOTIFICATION_GROUP_KEY_LED_PATTERN "LED-Pattern"

typedef struct
{
  gchar *destination;
  gchar *summary;
  gchar *body;
  gchar *icon;
  gchar *dbus_callback;
  gchar *empty_summary;
  gchar *preview_summary;
  gchar *text_domain;
  gchar *account_hint;
  gchar *account_call;
  gchar *pattern;
  GtkWidget *switcher_window;
  GPtrArray *notifications;
  gboolean no_window : 1;
} HDIncomingEventGroup;

struct _HDIncomingEventsPrivate
{
  GHashTable      *groups;

  GList           *preview_list;
  GtkWidget       *preview_window;

  GPtrArray       *plugins;

  HDPluginManager *plugin_manager;

  DBusGProxy      *mce_proxy;
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
  g_free (group->empty_summary);
  g_free (group->preview_summary);
  g_free (group->text_domain);
  g_free (group->account_call);
  g_free (group->account_hint);
  g_free (group->pattern);
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
group_activate_notifications (HDIncomingEventGroup *group,
                              GPtrArray            *notifications)
{
  guint i;

  if (notifications->len == 1)
    {
      /* Always call default action if it is only one un-grouped notification */
      HDNotification *notification;

      notification = g_ptr_array_index (notifications,
                                        0);

      hd_notification_manager_call_action (hd_notification_manager_get (),
                                           notification,
                                           "default");
      goto close_all;
    }

  if (group->account_call && group->account_hint)
    {
      const gchar *cmp_account = NULL;

      /* If account call is available, check if the account hint is the same for
       * each notification, if yes call the account call with the account hint
       * as argument
       */

      for (i = 0; i < notifications->len; i++)
        {
          HDNotification *notification;
          GValue *value;

          notification = g_ptr_array_index (notifications,
                                            i);
          value = hd_notification_get_hint (notification,
                                            group->account_hint);

          if (!G_VALUE_HOLDS (value, G_TYPE_STRING))
            {
              cmp_account = NULL;
              break;
            }
          else
            {
              const char *account = g_value_get_string (value);

              if (cmp_account == NULL)
                cmp_account = account;
              else if (g_strcmp0 (cmp_account, account))
                {
                  cmp_account = NULL;
                  break;
                }
            }
        }

      if (cmp_account)
        {
          gchar *dbus_call = g_strdup_printf ("%s string:%s",
                                              group->account_call,
                                              cmp_account);
          hd_notification_manager_call_dbus_callback (hd_notification_manager_get (),
                                                      dbus_call);
          g_free (dbus_call);

          goto close_all;
        }
    }

  if (group->dbus_callback)
    {
      /* Call D-Bus callback if available else call default action on each notification */
      hd_notification_manager_call_dbus_callback (hd_notification_manager_get (),
                                                  group->dbus_callback);
    }
  else
    {
      for (i = 0; i < notifications->len; i++)
        {
          HDNotification *notification;

          notification = g_ptr_array_index (notifications,
                                            i);

          hd_notification_manager_call_action (hd_notification_manager_get (),
                                               notification,
                                               "default");
        }
    }

close_all:

  /* Close all applications */
  for (i = 0; i < notifications->len; i++)
    {
      HDNotification *notification;

      notification = g_ptr_array_index (notifications,
                                        i);

      hd_notification_manager_close_notification (hd_notification_manager_get (),
                                                  hd_notification_get_id (notification),
                                                  NULL);
    }
}

static void
group_window_response (HDIncomingEventWindow *window,
                       gint                   response_id,
                       HDIncomingEventGroup  *group)
{
  guint i;

  if (response_id == GTK_RESPONSE_OK)
    {
      /* Close all applications */
      for (i = 0; i < group->notifications->len; i++)
        {
          HDNotification *notification;

          notification = g_ptr_array_index (group->notifications,
                                            i);

          /* The group is removed all at once */
          g_signal_handlers_disconnect_by_func (notification, group_notification_closed, group);
        }

      group_activate_notifications (group,
                                    group->notifications);
    }
  else
    {
      /* Close all applications */
      for (i = 0; i < group->notifications->len; i++)
        {
          HDNotification *notification;

          notification = g_ptr_array_index (group->notifications,
                                            i);

          /* The group is removed all at once */
          g_signal_handlers_disconnect_by_func (notification, group_notification_closed, group);

          hd_notification_manager_close_notification (hd_notification_manager_get (),
                                                      hd_notification_get_id (notification),
                                                      NULL);
        }
    }

  g_ptr_array_remove_range (group->notifications,
                            0,
                            group->notifications->len);
  group_update (group);
}

static void
group_update_window_content (HDIncomingEventGroup *group,
                             GtkWidget            *window,
                             GPtrArray            *notifications)
{
  /* Check if there are multiple notifications or just one */
  if (notifications->len > 1)
    {
      gint i;
      gint64 max_time = -2;
      const gchar *latest_summary = "";
      gchar *text_domain;
      gchar *summary;
      gchar *body;

      /* Create a grouped notification */

      /* Find the latest summary and time */
      i = notifications->len - 1;
      do
        {
          HDNotification *notification;
          gint64 time;

          notification = g_ptr_array_index (notifications,
                                            i);

          time = hd_notification_get_time (notification);
          if (time > max_time)
            {
              max_time = time;
              latest_summary = hd_notification_get_summary (notification);
            }
        }
      while (i-- > 0);

      if (group && group->empty_summary && (!latest_summary || !latest_summary[0]))
        latest_summary = group->empty_summary;

      /* Use default domain if no text domain is set */
      text_domain = (group && group->text_domain) ? group->text_domain : GETTEXT_PACKAGE;

      /* Create translated summary and body texts */
      summary = g_strdup_printf (dgettext (text_domain,
                                           group->summary),
                                 notifications->len);
      body = g_strdup_printf (dgettext (text_domain,
                                        group->body),
                              latest_summary);

      g_object_set (window,
                    "title", summary,
                    "message", body,
                    "icon", group->icon,
                    "time", max_time,
                    NULL);

      g_free (summary);
      g_free (body);
    }
  else
    {
      HDNotification *notification = g_ptr_array_index (notifications,
                                                        0);
      const gchar *summary, *body, *icon;

      /* Check if there is a special summary for this group */
      summary = hd_notification_get_summary (notification);
      if (group && group->empty_summary && (!summary || !summary[0]))
        summary = group->empty_summary;

      body = hd_notification_get_body (notification);
      if (group && group->preview_summary)
        {
          body = summary;
          summary = group->preview_summary;
        }

      icon = hd_notification_get_icon (notification);
      if (group && (!icon || !icon[0]))
        {
          icon = group->icon;
        }

      g_object_set (window,
                    "title", summary,
                    "message", body,
                    "icon", icon,
                    "time", (gint64) hd_notification_get_time (notification),
                    NULL);
    }

}

static void
group_update (HDIncomingEventGroup *group)
{
  g_debug ("update group");

  if (!group->notifications->len)
    {
      /* Last notification in this group was closed,
       *  destroy window */
      if (GTK_IS_WIDGET (group->switcher_window))
        {
          gtk_widget_destroy (group->switcher_window);
          group->switcher_window = NULL;
        }
    }
  else
    {
      /* At least one notification needs to be displayed,
       * create a window if it does not exist yet */
      if (!GTK_IS_WIDGET (group->switcher_window))
        {
          group->switcher_window = hd_incoming_event_window_new (FALSE,
                                                                 group->destination,
                                                                 NULL, NULL, -1, NULL);
          g_signal_connect (group->switcher_window, "response",
                            G_CALLBACK (group_window_response),
                            group);
        }

      group_update_window_content (group,
                                   group->switcher_window,
                                   group->notifications);

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

static void
add_switcher_notification (HDIncomingEvents         *ie,
                           HDNotification           *notification)
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

typedef struct
{
  HDIncomingEvents     *ie;
  HDIncomingEventGroup *group;
  GPtrArray            *notifications;
} PreviewWindowResponseInfo;

static void
preview_window_response (HDIncomingEventWindow     *window,
                         gint                       response_id,
                         PreviewWindowResponseInfo *info)
{
  HDIncomingEvents *ie = info->ie;
  GPtrArray *notifications = info->notifications;

  g_debug ("preview_window_response response_id:%d", response_id);

  g_free (info);

  if (response_id == GTK_RESPONSE_OK)
    {
      group_activate_notifications (info->group,
                                    notifications);
    }
  else if (response_id == GTK_RESPONSE_DELETE_EVENT)
    {
      guint i;
      for (i = 0; i < notifications->len; i++)
        {
          HDNotification *notification;

          notification = g_ptr_array_index (notifications, i);

          add_switcher_notification (ie, notification);
        }
    }

  g_ptr_array_free (notifications, TRUE);

  gtk_widget_destroy (GTK_WIDGET (window));
}

/* Function to set/reset mce led pattern */
static void
hd_incoming_events_set_led_pattern (HDIncomingEvents *ie,
                                    const gchar      *pattern,
                                    gboolean          activate)
{
  HDIncomingEventsPrivate *priv = ie->priv;

  if (activate)
    g_debug("Activate LED pattern : %s\n", pattern);
  else
    g_debug("Deactivate LED pattern : %s\n", pattern);

  /* Just make a method call as per args to activate/deactivate
   * given led pattern without looking for any reply(We don't have a reply) */
  dbus_g_proxy_call_no_reply (priv->mce_proxy,
                              (activate) ? MCE_ACTIVATE_LED_PATTERN :
                              MCE_DEACTIVATE_LED_PATTERN,
                              G_TYPE_STRING,
                              pattern,
                              G_TYPE_INVALID,
                              G_TYPE_INVALID);
}

static void 
close_led_notification (HDNotification *notification,
                        const gchar    *pattern)
{
  hd_incoming_events_set_led_pattern (hd_incoming_events_get (),
                                      pattern,
                                      FALSE);
}

static void
remove_from_preview_list (HDNotification   *notification,
                          HDIncomingEvents *ie)
{
  HDIncomingEventsPrivate *priv = ie->priv;

  priv->preview_list = g_list_remove (priv->preview_list,
                                      notification);
}

static void show_preview_window (HDIncomingEvents *ie);

static void
preview_window_destroy_cb (GtkWidget        *window,
                           HDIncomingEvents *ie)
{
  HDIncomingEventsPrivate *priv = ie->priv;

  priv->preview_window = NULL;

  show_preview_window (ie);
}

static void
show_preview_window (HDIncomingEvents *ie)
{
  HDIncomingEventsPrivate *priv = ie->priv;
  HDNotification *n;
  GPtrArray *notifications;
  const gchar *category;
  HDIncomingEventGroup *group;
  PreviewWindowResponseInfo *preview_window_response_info;

  if (priv->preview_window || !priv->preview_list)
    return;

  /* Get first notification */
  notifications = g_ptr_array_new ();

  n = priv->preview_list->data;
  g_ptr_array_add (notifications,
                   n);
  priv->preview_list = g_list_delete_link (priv->preview_list,
                                           priv->preview_list);

  category = hd_notification_get_category (n);
  group = g_hash_table_lookup (priv->groups,
                               category);

  /* Get all further notifications with the same category */
  while (priv->preview_list)
    {
      n = priv->preview_list->data;

      if (category && !strcmp (category,
                               hd_notification_get_category (n)))
        {
          g_ptr_array_add (notifications,
                           n);
          priv->preview_list = g_list_delete_link (priv->preview_list,
                                                   priv->preview_list);
        }
      else
        break;
    }

  /* Create the notification preview window */
  priv->preview_window = hd_incoming_event_window_new (TRUE,
                                                       NULL,
                                                       NULL,
                                                       NULL,
                                                       -1,
                                                       NULL);

  group_update_window_content (group,
                               priv->preview_window,
                               notifications);

  preview_window_response_info = g_new (PreviewWindowResponseInfo, 1);
  preview_window_response_info->ie = ie;
  preview_window_response_info->group = group;
  preview_window_response_info->notifications = notifications;

  g_signal_connect (priv->preview_window, "response",
                    G_CALLBACK (preview_window_response),
                    preview_window_response_info);
  g_signal_connect (priv->preview_window, "destroy",
                    G_CALLBACK (preview_window_destroy_cb), ie);
/*  g_signal_connect_object (notification, "closed",
                           G_CALLBACK (gtk_widget_destroy), priv->preview_window,
                           G_CONNECT_SWAPPED);*/

  /* Send dbus request to mce to turn display backlight on */
  if (priv->mce_proxy)
    {
      g_debug ("%s. Call %s",
               __FUNCTION__,
               MCE_DISPLAY_ON_REQ);
      dbus_g_proxy_call_no_reply (priv->mce_proxy, MCE_DISPLAY_ON_REQ,
                                  G_TYPE_INVALID, G_TYPE_INVALID);
    }

  gtk_widget_show (priv->preview_window);
}

static gint
cmp_category_func (gconstpointer a,
                   gconstpointer b)
{
  const gchar *cat_a = hd_notification_get_category ((HDNotification *) a);
  const gchar *cat_b = hd_notification_get_category ((HDNotification *) b);

  if (!cat_a || !cat_a[0] || !cat_b || !cat_b[0])
    return 1;

  if (strcmp (cat_a, cat_b) == 0)
    return -1;

  return 1;
}

static void
hd_incoming_events_notified (HDNotificationManager  *nm,
                             HDNotification         *notification,
                             gboolean                replayed_event,
                             HDIncomingEvents       *ie)
{
  HDIncomingEventsPrivate *priv = ie->priv;
  const gchar *category;
  guint i;
  HDIncomingEventGroup *group;
  GValue *p;
  const gchar *pattern = NULL;

  g_return_if_fail (HD_IS_INCOMING_EVENTS (ie));

  /* Get category string */
  category = hd_notification_get_category (notification);
  /* Ignore internal system.note. notifications */
  if (g_str_has_prefix (category, "system.note."))
    return;

  if (replayed_event)
    {
      add_switcher_notification (ie, notification);
      return;
    }

  /* Call plugins */
  for (i = 0; i < priv->plugins->len; i++)
    {
      HDNotificationPlugin *plugin = g_ptr_array_index (priv->plugins, i);

      hd_notification_plugin_notify (plugin, notification);
    }

  /* Lookup category and handle special summary cases */
  group = g_hash_table_lookup (ie->priv->groups,
                               category);

  /* Lets see if we have any led event for this category */
  p = hd_notification_get_hint (notification, "led-pattern");
  if (G_VALUE_HOLDS_STRING (p))
    pattern = g_value_get_string (p);
  if (!pattern)
    pattern = group->pattern;

  if (pattern)
    {
      /* Activate pattern */
      hd_incoming_events_set_led_pattern (ie,
                                          pattern,
                                          TRUE);
      /* Handler to deactivate the led event */
      g_signal_connect_data (notification, "closed",
                             G_CALLBACK (close_led_notification),
                             g_strdup (pattern),
                             (GClosureNotify) g_free,
                             0);
    }

  /* Return if no notification window should be shown */
  if (group && group->no_window)
    return;

  priv->preview_list = g_list_insert_sorted (priv->preview_list,
                                             notification,
                                             cmp_category_func);

  g_signal_connect (notification, "closed",
                    G_CALLBACK (remove_from_preview_list), ie);

  show_preview_window (ie);
}

static void
hd_incoming_events_dispose (GObject *object)
{
  HDIncomingEventsPrivate *priv = HD_INCOMING_EVENTS (object)->priv;

  if (priv->plugin_manager)
    priv->plugin_manager = (g_object_unref (priv->plugin_manager), NULL);

  if (priv->mce_proxy)
    priv->mce_proxy = (g_object_unref (priv->mce_proxy), NULL);

  G_OBJECT_CLASS (hd_incoming_events_parent_class)->dispose (object);
}

static void
hd_incoming_events_finalize (GObject *object)
{
  HDIncomingEventsPrivate *priv = HD_INCOMING_EVENTS (object)->priv;

  if (priv->groups)
    priv->groups = (g_hash_table_destroy (priv->groups), NULL);

  if (priv->preview_list)
    priv->preview_list = (g_list_free (priv->preview_list), NULL);

  if (priv->plugins)
    priv->plugins = (g_ptr_array_free (priv->plugins, TRUE), NULL);

  G_OBJECT_CLASS (hd_incoming_events_parent_class)->finalize (object);
}

static void
hd_incoming_events_class_init (HDIncomingEventsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = hd_incoming_events_dispose;
  object_class->finalize = hd_incoming_events_finalize;

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
      g_object_unref (groups_file);
      return;
    }

  /* Get all groups */
  groups = g_key_file_get_groups (key_file, NULL);

  /* Warning if no groups */
  if (groups == NULL)
    {
      g_warning ("Notification groups file is empty");
      g_object_unref (groups_file);
      g_key_file_free (key_file);
      return;
    }

  /* Iterate all groups the group is the org.freedesktop.Notifications.Notify hints category */
  for (i = 0; groups[i]; i++)
    {
      HDIncomingEventGroup *group;
      gchar *untranslated;
      GError *error = NULL;

      group = g_new0 (HDIncomingEventGroup, 1);

      group->notifications = g_ptr_array_new ();

      group->no_window = g_key_file_get_boolean (key_file,
                                                 groups[i],
                                                 NOTIFICATION_GROUP_KEY_NO_WINDOW,
                                                 NULL);
      /* We do not need more information as no notification windows are shown */
      if (group->no_window)
        {
          g_debug ("Add group %s", groups[i]);
          g_hash_table_insert (ie->priv->groups,
                               groups[i],
                               group);
          continue;
        }

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

      group->text_domain = g_key_file_get_string (key_file,
                                                  groups[i],
                                                  NOTIFICATION_GROUP_KEY_TEXT_DOMAIN,
                                                  NULL);

      untranslated = g_key_file_get_string (key_file,
                                            groups[i],
                                            NOTIFICATION_GROUP_KEY_EMPTY_SUMMARY,
                                            NULL);
      if (untranslated)
        {
          group->empty_summary = g_strdup (dgettext (group->text_domain,
                                                     untranslated));
          g_free (untranslated);
        }

      untranslated = g_key_file_get_string (key_file,
                                            groups[i],
                                            NOTIFICATION_GROUP_KEY_PREVIEW_SUMMARY,
                                            NULL);
      if (untranslated)
        {
          group->preview_summary = g_strdup (dgettext (group->text_domain,
                                                       untranslated));
          g_free (untranslated);
        }

      group->dbus_callback = g_key_file_get_string (key_file,
                                                    groups[i],
                                                    NOTIFICATION_GROUP_KEY_DBUS_CALL,
                                                    NULL);

      group->account_hint = g_key_file_get_string (key_file,
                                                   groups[i],
                                                   NOTIFICATION_GROUP_KEY_ACCOUNT_HINT,
                                                   NULL);
 
      group->account_call = g_key_file_get_string (key_file,
                                                   groups[i],
                                                   NOTIFICATION_GROUP_KEY_ACCOUNT_CALL,
                                                   NULL);

      group->pattern = g_key_file_get_string (key_file,
                                              groups[i],
                                              NOTIFICATION_GROUP_KEY_LED_PATTERN,
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

static gboolean
load_plugins_idle (gpointer data)
{
  /* Load the configuration of the plugin manager and load plugins */
  hd_plugin_manager_run (HD_PLUGIN_MANAGER (data));

  return FALSE;
}

static void
hd_incoming_events_init (HDIncomingEvents *ie)
{
  HDIncomingEventsPrivate *priv;
  DBusGConnection *connection;
  GError *error = NULL;

  priv = ie->priv = HD_INCOMING_EVENTS_GET_PRIVATE (ie);

  priv->groups = g_hash_table_new_full (g_str_hash,
                                        g_str_equal,
                                        (GDestroyNotify) g_free,
                                        (GDestroyNotify) group_free);

  priv->plugins = g_ptr_array_new ();

  priv->plugin_manager = hd_plugin_manager_new (hd_config_file_new_with_defaults ("notification.conf"));

  /* Connect to plugin manager signals */
  g_signal_connect (priv->plugin_manager, "plugin-added",
                    G_CALLBACK (hd_incoming_events_plugin_added), ie);
  g_signal_connect (priv->plugin_manager, "plugin-removed",
                    G_CALLBACK (hd_incoming_events_plugin_removed), ie);

  /* Load notification plugins when idle */
  gdk_threads_add_idle (load_plugins_idle, priv->plugin_manager);

  /* Connect to notification manager signals */
  g_signal_connect_object (hd_notification_manager_get (), "notified",
                           G_CALLBACK (hd_incoming_events_notified), ie, 0);
  load_notification_groups (ie);

  /* Get D-Bus proxy for mce calls */
  connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);

  if (error)
    {
      g_warning ("Could not connect to System D-Bus. %s", error->message);
      g_error_free (error);
    }
  else
    {
      priv->mce_proxy = dbus_g_proxy_new_for_name (connection,
                                                   MCE_SERVICE,
                                                   MCE_REQUEST_PATH,
                                                   MCE_REQUEST_IF);
      g_debug ("%s. Got mce Proxy", __FUNCTION__);
    }
}

HDIncomingEvents *
hd_incoming_events_get (void)
{
  static HDIncomingEvents *ie = NULL;
  
  if (G_UNLIKELY (!ie))
    ie = g_object_new (HD_TYPE_INCOMING_EVENTS, NULL);

  return ie;
}
