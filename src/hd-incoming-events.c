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

#include <osso-mem.h>

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
#define NOTIFICATION_GROUP_KEY_GROUPED_CATEGORY "Grouped-Category"
#define NOTIFICATION_GROUP_KEY_GROUPED "Grouped"

typedef struct _Notifications Notifications;


/*
 * A HDNotification contains in a category (hd_notification_get_category)
 *
 * This categories are represented as CategoryInfo
 * (loaded from notification-groups.conf) multiple categories can be mapped
 * together.
 *
 * For display of notifications in preview and switcher windows the notifications
 * are grouped together based on the mapped category (Notifications).
 *
 */

/*
 * Informations for categories loaded
 * in /etc/hildon-desktop/notification-groups.conf
 *
 */
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
  gchar *mapped_category;
  GtkWidget *switcher_window;
  Notifications *notifications;
  gboolean no_window : 1;
  gboolean mapped : 1;
  HDIncomingEvents *ie;
} CategoryInfo;

typedef void (*NotificationsCallback) (Notifications *ns,
                                       gpointer       data);

/*
 * Used to group notifications with the same (mapped) category
 * toegther for preview and switcher windows.
 */
struct _Notifications
{
  CategoryInfo          *info;
  CategoryInfo          *fallback;
  GPtrArray             *notifications;
  NotificationsCallback  cb;
  gpointer               cb_data;
};

struct _HDIncomingEventsPrivate
{
  GHashTable      *categories;

  GList           *preview_list;
  GtkWidget       *preview_window;

  GPtrArray       *plugins;

  HDPluginManager *plugin_manager;

  DBusGProxy      *mce_proxy;
};

G_DEFINE_TYPE (HDIncomingEvents, hd_incoming_events, G_TYPE_OBJECT);

/* Check if category is mapped to a virtual category
 * and returns the virtual category in this case, 
 * else return category */
static void
notification_get_category_info (HDIncomingEvents  *ie,
                                HDNotification    *n,
                                CategoryInfo     **mapped,
                                CategoryInfo     **fallback)
{
  HDIncomingEventsPrivate *priv = ie->priv;
  const gchar *category;
  CategoryInfo *info = NULL;

  /* Lookup info for notification */
  category = hd_notification_get_category (n);
  if (category)
    info = g_hash_table_lookup (priv->categories,
                                category);

  if (info && info->mapped_category)
    {
      /* If the category is mapped into another category
       * set mapped and fallback info 
       */
      if (fallback)
        *fallback = info;

      info = g_hash_table_lookup (priv->categories,
                                  info->mapped_category);

      if (mapped)
        *mapped = info;
    }
  else
    {
      /* If the category is not mapped set mapped to this category
       * and set no fallback */
      if (mapped)
        *mapped = info;
      if (fallback)
        *fallback = NULL;
    }
}

static void
notification_closed_cb (HDNotification *n,
                        Notifications  *ns)
{
  g_ptr_array_remove (ns->notifications,
                      n);
  g_signal_handlers_disconnect_by_func (n,
                                        G_CALLBACK (notification_closed_cb),
                                        ns);
  g_object_unref (n);

  if (ns->cb)
    ns->cb (ns, ns->cb_data);
}

/* 
 * Returns: an empty Notifications structure
 */
static Notifications *
notifications_new_for_category (CategoryInfo *info)
{
  Notifications *ns;
  GPtrArray *notifications = g_ptr_array_new ();

  ns = g_slice_new0 (Notifications);
  ns->info = info;
  ns->notifications = notifications;

  return ns;
}

/*
 * Returns: a Notifications structure containing just 
 * notification @n
 */
static Notifications *
notifications_new_for_notification (HDIncomingEvents *ie,
                                    HDNotification   *n)
{
  Notifications *ns;
  GPtrArray *notifications = g_ptr_array_sized_new (1);

  ns = g_slice_new0 (Notifications);
  ns->notifications = notifications;

  g_object_ref (n);
  if (ie)
    {
      notification_get_category_info (ie,
                                      n,
                                      &ns->info,
                                      &ns->fallback);

      g_debug ("%s. Category %s, Info: %p, Fallback: %p",
               __FUNCTION__,
               hd_notification_get_category (n),
               ns->info,
               ns->fallback);
    }

  g_ptr_array_add (notifications,
                   n);
  g_signal_connect (n, "closed",
                    G_CALLBACK (notification_closed_cb), ns);

  return ns;
}

/*
 * Frees Notifications structure @ns
 */
static void
notifications_free (Notifications *ns)
{
  GPtrArray *notifications = ns->notifications;
  guint i;

  for (i = 0; i < notifications->len; i++)
    {
      HDNotification *n = g_ptr_array_index (notifications, i);

      g_signal_handlers_disconnect_by_func (n, G_CALLBACK (notification_closed_cb), ns);

      g_object_unref (n);
    }

  g_ptr_array_free (notifications, TRUE);
  g_slice_free (Notifications, ns);
}

/*
 * Check if Notifications structure is empty
 */
static gboolean
notifications_is_empty (Notifications *ns)
{
  return ns->notifications->len == 0;
}

/*
 * Append all notifications from @other to @ns.
 * @ns and @other have to be the same category.
 */
static void
notifications_append (Notifications *ns,
                      Notifications *other)
{
  guint i;

  /* Only notifications with an info can be grouped together */
  g_return_if_fail (ns->info);

  /* The notifications are always of the same (mapped) category */
  g_return_if_fail (ns->info == other->info);

  /* update fallback to latest appended notification */
  ns->fallback = other->fallback;

  for (i = 0; i < other->notifications->len; i++)
    {
      HDNotification *n = g_ptr_array_index (other->notifications, i);

      g_object_ref (n);
      g_ptr_array_add (ns->notifications, n);
      g_signal_connect (n, "closed",
                        G_CALLBACK (notification_closed_cb), ns);
    }
}

/* Close all notifications and call the update cb */
static void
notifications_close_all (Notifications *ns)
{
  guint i;

  for (i = 0; i < ns->notifications->len; i++)
    {
      HDNotification *n = g_ptr_array_index (ns->notifications,
                                             i);
      g_signal_handlers_disconnect_by_func (n,
                                            notification_closed_cb,
                                            ns);
      hd_notification_manager_close_notification (hd_notification_manager_get (),
                                                  hd_notification_get_id (n),
                                                  NULL);
      g_object_unref (n);
    }

  g_ptr_array_remove_range (ns->notifications,
                            0,
                            ns->notifications->len);

  if (ns->cb)
    ns->cb (ns, ns->cb_data);
}

/* If account call is available, check if the account hint is the same for
 * each notification
 */
static const gchar *
notifications_get_common_account (Notifications *ns)
{
  const gchar *account = NULL;
  guint i;

  if (!ns->info->account_call || !ns->info->account_hint)
    return NULL;


  for (i = 0; i < ns->notifications->len; i++)
    {
      GValue *value;
      HDNotification *n = g_ptr_array_index (ns->notifications,
                                             i); 

      value = hd_notification_get_hint (n, ns->info->account_hint);

      if (!G_VALUE_HOLDS (value, G_TYPE_STRING))
        {
          return NULL;
        }
      else
        {
          if (!account)
            account = g_value_get_string (value);
          else if (g_strcmp0 (account,
                              g_value_get_string (value)))
            return NULL;
        }
    }

  return account;
}

/* Activate an array of notifications
 *
 * Returns: %TRUE if the notifications were sucessfullly activated
 */ 
static gboolean
notifications_activate (Notifications *ns)
{
  guint i;
  const gchar *common_account;

  if (ns->notifications->len == 0)
    return TRUE;

  if (osso_mem_in_lowmem_state ())
    {
      g_debug ("%s. Do not activate notification in low mem state.",
               __FUNCTION__);

      hildon_banner_show_information (NULL, NULL,
                                      dgettext ("ke-recv",
                                                "memr_ti_close_applications"));

      return FALSE;
    }

  if (ns->notifications->len == 1)
    {
      /* Always call default action if it is only one notification */
      HDNotification *n = g_ptr_array_index (ns->notifications, 0);

      hd_notification_manager_call_action (hd_notification_manager_get (),
                                           n,
                                           "default");
      notifications_close_all (ns);
      return TRUE;
    }

  common_account = notifications_get_common_account (ns);

  if (common_account)
    {
      hd_notification_manager_call_dbus_callback_with_arg (hd_notification_manager_get (),
                                                           ns->info->account_call,
                                                           common_account);
      notifications_close_all (ns);
      return TRUE;
    }

  if (ns->info->dbus_callback)
    {
      /* Call D-Bus callback if available else call default action on each notification */
      hd_notification_manager_call_dbus_callback (hd_notification_manager_get (),
                                                  ns->info->dbus_callback);
      notifications_close_all (ns);
      return TRUE;
    }
  else
    {
      for (i = 0; i < ns->notifications->len; i++)
        {
          HDNotification *n = g_ptr_array_index (ns->notifications,
                                                 i);

          hd_notification_manager_call_action (hd_notification_manager_get (),
                                               n,
                                               "default");
        }
    }

  notifications_close_all (ns);
  return TRUE;
}

static void
notifications_update_window (Notifications *ns,
                             GtkWidget     *window)
{
  /* Destroy the window when all notifications are closed */
  if (notifications_is_empty (ns))
    {
      gtk_widget_destroy (window);
      return;
    }

  /* Check if there are multiple notifications or just one */
  if (ns->notifications->len > 1)
    {
      gint64 max_time;
      HDNotification *latest_notification;
      const gchar *latest_summary = NULL;
      gchar *summary = NULL, *body = NULL;
      const gchar *icon;
      CategoryInfo *info = ns->info;
      CategoryInfo *fallback = ns->fallback;

      latest_notification = g_ptr_array_index (ns->notifications,
                                               ns->notifications->len - 1);
      max_time = hd_notification_get_time (latest_notification);

      latest_summary = hd_notification_get_summary (latest_notification);

      if (!latest_summary || !latest_summary[0])
        {
          if (info->empty_summary)
            latest_summary = info->empty_summary;
          else if (fallback && fallback->empty_summary)
            latest_summary = fallback->empty_summary;
        }

      /* Icon */
      if (info->icon)
        icon = info->icon;
      else if (fallback && fallback->icon)
        icon = fallback->icon;
      else
        icon = hd_notification_get_icon (latest_notification);

      /* Create translated summary and body texts */
      if (info->summary)
        {
          summary = g_strdup_printf (dgettext (info->text_domain ? info->text_domain : GETTEXT_PACKAGE,
                                               info->summary),
                                     ns->notifications->len);
        }
      else if (fallback && fallback->summary)
        {
          summary = g_strdup_printf (dgettext (fallback->text_domain ? fallback->text_domain : GETTEXT_PACKAGE,
                                               fallback->summary),
                                     ns->notifications->len);
        }

      if (info->body)
        {
          body = g_strdup_printf (dgettext (info->text_domain ? info->text_domain : GETTEXT_PACKAGE,
                                            info->body),
                                  latest_summary);
        }
      else if (fallback && fallback->body)
        {
          body = g_strdup_printf (dgettext (fallback->text_domain ? fallback->text_domain : GETTEXT_PACKAGE,
                                            fallback->body),
                                  latest_summary);
        }

      g_object_set (window,
                    "title", summary,
                    "message", body,
                    "icon", icon,
                    "time", max_time,
                    NULL);

      g_free (summary);
      g_free (body);
    }
  else
    {
      HDNotification *notification = g_ptr_array_index (ns->notifications, 0);
      const gchar *summary, *body, *icon;
      CategoryInfo *fallback = NULL;

      if (ns->info && ns->info->mapped)
        fallback = g_hash_table_lookup (ns->info->ie->priv->categories,
                                          hd_notification_get_category (notification));


      /* Check if there is a special summary for this ns->info */
      summary = hd_notification_get_summary (notification);
      if (!summary || !summary[0])
        {
          if (ns->info && ns->info->empty_summary)
            summary = ns->info->empty_summary;
          else if (fallback && fallback->empty_summary)
            summary = fallback->empty_summary;
        }

      body = hd_notification_get_body (notification);
      if (ns->info && ns->info->preview_summary)
        {
          body = summary;
          summary = ns->info->preview_summary;
        }
      else if (fallback && fallback->preview_summary)
        {
          body = summary;
          summary = fallback->preview_summary;
        }

      icon = hd_notification_get_icon (notification);
      if (!icon || !icon[0])
        {
          if (ns->info && ns->info->icon)
            icon = ns->info->icon;
          else if (fallback && fallback->icon)
            icon = fallback->icon;
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
switcher_window_response (HDIncomingEventWindow *window,
                          gint                   response_id,
                          Notifications         *ns)
{
  if (response_id == GTK_RESPONSE_OK)
    {
      notifications_activate (ns);
    }
  else
    {
      notifications_close_all (ns);
    }
}

static void
notifications_update_switcher_window (Notifications *ns,
                                      GtkWidget     *window)
{
  if (notifications_is_empty (ns))
    {
      /* Last notification in this group was closed,
       *  destroy window */
      if (GTK_IS_WIDGET (ns->info->switcher_window))
        {
          gtk_widget_destroy (ns->info->switcher_window);
          ns->info->switcher_window = NULL;
        }
    }
  else
    {
      /* At least one notification needs to be displayed,
       * create a window if it does not exist yet */
      if (!GTK_IS_WIDGET (ns->info->switcher_window))
        {
          ns->info->switcher_window = hd_incoming_event_window_new (FALSE,
                                                                    ns->info->destination,
                                                                    NULL, NULL, -1, NULL);
          g_signal_connect (ns->info->switcher_window, "response",
                            G_CALLBACK (switcher_window_response),
                            ns->info->notifications);
        }

      notifications_update_window (ns->info->notifications,
                                   ns->info->switcher_window);

      gtk_widget_show (ns->info->switcher_window);
    }
}

static void
notifications_add_to_switcher (Notifications *ns)
{
  if (ns->info)
    {
      notifications_append (ns->info->notifications,
                            ns);

      notifications_update_switcher_window (ns->info->notifications,
                                            NULL);
      ns->info->notifications->cb = (NotificationsCallback) notifications_update_switcher_window;
      ns->info->notifications->cb_data = NULL;
    }
  else if (ns->notifications->len == 1)
    {
      GtkWidget *switcher_window;
      HDNotification *notification = g_ptr_array_index (ns->notifications, 0);

      switcher_window = hd_incoming_event_window_new (FALSE,
                                                      NULL,
                                                      hd_notification_get_summary (notification),
                                                      hd_notification_get_body (notification),
                                                      hd_notification_get_time (notification),
                                                      hd_notification_get_icon (notification));

      g_signal_connect (switcher_window, "response",
                        G_CALLBACK (switcher_window_response),
                        notifications_new_for_notification (NULL,
                                                            notification));
      g_signal_connect_object (notification, "closed",
                               G_CALLBACK (gtk_widget_destroy), switcher_window,
                               G_CONNECT_SWAPPED);

      gtk_widget_show (switcher_window);
    }
  else
    {
      g_warning ("%s. No info defined for notifications array", __FUNCTION__);
    }
}

static void
preview_window_response (HDIncomingEventWindow *window,
                         gint                   response_id,
                         Notifications         *ns)
{
  g_debug ("preview_window_response response_id:%d", response_id);

  if (response_id == GTK_RESPONSE_OK)
    {
      if (!notifications_activate (ns))
        {
          /* Notifications could not be activated add them to task switcher */
          notifications_add_to_switcher (ns);
        }
    }
  else if (response_id == GTK_RESPONSE_DELETE_EVENT)
    notifications_add_to_switcher (ns);

  notifications_free (ns);

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
  Notifications *ns;

  if (priv->preview_window || !priv->preview_list)
    return;

  /* Pop first notification from preview ns */
  ns = priv->preview_list->data;
  priv->preview_list = g_list_delete_link (priv->preview_list,
                                           priv->preview_list);

  /* Create the notification preview window */
  priv->preview_window = hd_incoming_event_window_new (TRUE,
                                                       NULL,
                                                       NULL,
                                                       NULL,
                                                       -1,
                                                       NULL);

  ns->cb  = (NotificationsCallback) notifications_update_window;
  ns->cb_data = priv->preview_window;

  notifications_update_window (ns,
                               priv->preview_window);

  g_signal_connect (priv->preview_window, "response",
                    G_CALLBACK (preview_window_response),
                    ns);
  g_signal_connect (priv->preview_window, "destroy",
                    G_CALLBACK (preview_window_destroy_cb), ie);

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
notifications_cmp (gconstpointer a,
                   gconstpointer b)
{
  const Notifications *l_a = a;
  const Notifications *l_b = b;

  if (l_a->info == l_b->info)
    return 0;

  return -1;
}

static void
preview_list_notifications_cb (Notifications *ns,
                               gpointer       data)
{
  HDIncomingEventsPrivate *priv = data;

  if (notifications_is_empty (ns))
    {
      priv->preview_list = g_list_remove (priv->preview_list,
                                          ns);
      notifications_free (ns);
    }
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
  GValue *p;
  const gchar *pattern = NULL;
  Notifications *ns;

  g_return_if_fail (HD_IS_INCOMING_EVENTS (ie));

  /* Get category string */
  category = hd_notification_get_category (notification);

  /* Do nothing for system.note.* notifications */
  if (category && g_str_has_prefix (category, "system.note."))
    {
      /*
      for (i = 0; i < priv->plugins->len; i++)
        {
          HDNotificationPlugin *plugin = g_ptr_array_index (priv->plugins, i);

          hd_notification_plugin_notify (plugin, notification);
        }
      */
      return;
    }

  ns = notifications_new_for_notification (ie,
                                             notification); 

  /* Replayed events are just added to the switcher */
  if (replayed_event)
    {
      notifications_add_to_switcher (ns);
      notifications_free (ns);

      return;
    }

  /* Call plugins */
  for (i = 0; i < priv->plugins->len; i++)
    {
      HDNotificationPlugin *plugin = g_ptr_array_index (priv->plugins, i);

      hd_notification_plugin_notify (plugin, notification);
    }

  /* Lets see if we have any led event for this category */
  p = hd_notification_get_hint (notification, "led-pattern");
  if (G_VALUE_HOLDS_STRING (p))
    pattern = g_value_get_string (p);
  if (!pattern && ns->info)
    pattern = ns->info->pattern;
  if (!pattern && ns->fallback)
    pattern = ns->fallback->pattern;

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
  if (ns->info && ns->info->no_window)
    return;

  if (ns->info)
    {
      GList *l  = g_list_find_custom (priv->preview_list,
                                      ns,
                                      notifications_cmp);

      if (l)
        {
          Notifications *existing = l->data;

          notifications_append (existing,
                                ns);
          notifications_free (ns);
        }
      else
        {
          priv->preview_list = g_list_append (priv->preview_list,
                                              ns);
          ns->cb = preview_list_notifications_cb;
          ns->cb_data = priv;
        }
    }
  else
    {
      priv->preview_list = g_list_append (priv->preview_list,
                                          ns);
    }

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

  if (priv->categories)
    priv->categories = (g_hash_table_destroy (priv->categories), NULL);

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
category_info_free (CategoryInfo *info)
{
  g_free (info->destination);
  g_free (info->summary);
  g_free (info->body);
  g_free (info->icon);
  g_free (info->dbus_callback);
  g_free (info->empty_summary);
  g_free (info->preview_summary);
  g_free (info->text_domain);
  g_free (info->account_call);
  g_free (info->account_hint);
  g_free (info->pattern);
  g_free (info->mapped_category);
  if (GTK_IS_WIDGET (info->switcher_window))
    gtk_widget_destroy (info->switcher_window);
  notifications_free (info->notifications);
  g_free (info);
}

/* 
 * Loads all the category infos from /etc/hildon-desktop/notification-groups.conf
 */
static void
load_category_infos (HDIncomingEvents *ie)
{
  HDConfigFile *infos_file;
  GKeyFile *key_file;
  gchar **infos;
  guint i;

  /* Load the config file */
  infos_file = hd_config_file_new (HD_DESKTOP_CONFIG_PATH,
                                   NULL,
                                   "notification-groups.conf");
  key_file = hd_config_file_load_file (infos_file, TRUE);

  if (!key_file)
    {
      g_warning ("Could not load notifications info file");
      g_object_unref (infos_file);
      return;
    }

  /* Get all infos */
  infos = g_key_file_get_groups (key_file, NULL);

  /* Warning if no infos */
  if (infos == NULL)
    {
      g_warning ("Notification infos file is empty");
      g_object_unref (infos_file);
      g_key_file_free (key_file);
      return;
    }

  /* Iterate all infos the info is the org.freedesktop.Notifications.Notify hints category */
  for (i = 0; infos[i]; i++)
    {
      CategoryInfo *info;
      gchar *untranslated;
      GError *error = NULL;

      info = g_new0 (CategoryInfo, 1);
      info->ie = ie;

      info->notifications = notifications_new_for_category (info);

      info->no_window = g_key_file_get_boolean (key_file,
                                                infos[i],
                                                NOTIFICATION_GROUP_KEY_NO_WINDOW,
                                                NULL);
      /* We do not need more information as no notification windows are shown */
      if (info->no_window)
        {
          g_debug ("Add category %s", infos[i]);
          g_hash_table_insert (ie->priv->categories,
                               infos[i],
                               info);
          continue;
        }

      info->destination = g_key_file_get_string (key_file,
                                                 infos[i],
                                                 NOTIFICATION_GROUP_KEY_DESTINATION,
                                                 &error);
      if (error)
        goto load_key_error;

      info->summary = g_key_file_get_string (key_file,
                                             infos[i],
                                             NOTIFICATION_GROUP_KEY_SUMMARY,
                                             NULL);

      info->body = g_key_file_get_string (key_file,
                                          infos[i],
                                          NOTIFICATION_GROUP_KEY_BODY,
                                          NULL);

      info->icon = g_key_file_get_string (key_file,
                                          infos[i],
                                          NOTIFICATION_GROUP_KEY_ICON,
                                          NULL);

      info->text_domain = g_key_file_get_string (key_file,
                                                 infos[i],
                                                 NOTIFICATION_GROUP_KEY_TEXT_DOMAIN,
                                                 NULL);

      untranslated = g_key_file_get_string (key_file,
                                            infos[i],
                                            NOTIFICATION_GROUP_KEY_EMPTY_SUMMARY,
                                            NULL);
      if (untranslated)
        {
          info->empty_summary = g_strdup (dgettext (info->text_domain,
                                                    untranslated));
          g_free (untranslated);
        }

      untranslated = g_key_file_get_string (key_file,
                                            infos[i],
                                            NOTIFICATION_GROUP_KEY_PREVIEW_SUMMARY,
                                            NULL);
      if (untranslated)
        {
          info->preview_summary = g_strdup (dgettext (info->text_domain,
                                                      untranslated));
          g_free (untranslated);
        }

      info->dbus_callback = g_key_file_get_string (key_file,
                                                   infos[i],
                                                   NOTIFICATION_GROUP_KEY_DBUS_CALL,
                                                   NULL);

      info->account_hint = g_key_file_get_string (key_file,
                                                  infos[i],
                                                  NOTIFICATION_GROUP_KEY_ACCOUNT_HINT,
                                                  NULL);

      info->account_call = g_key_file_get_string (key_file,
                                                  infos[i],
                                                  NOTIFICATION_GROUP_KEY_ACCOUNT_CALL,
                                                  NULL);

      info->pattern = g_key_file_get_string (key_file,
                                             infos[i],
                                             NOTIFICATION_GROUP_KEY_LED_PATTERN,
                                             NULL);

      info->mapped_category = g_key_file_get_string (key_file,
                                                     infos[i],
                                                     NOTIFICATION_GROUP_KEY_GROUPED_CATEGORY,
                                                     NULL);

      info->mapped = g_key_file_get_boolean (key_file,
                                             infos[i],
                                             NOTIFICATION_GROUP_KEY_GROUPED,
                                             NULL);

      g_debug ("Add category %s", infos[i]);
      g_hash_table_insert (ie->priv->categories,
                           infos[i],
                           info);

      continue;

load_key_error:
      g_warning ("Error loading notification infos file: %s", error->message);
      category_info_free (info);
      g_error_free (error);
      g_free (infos[i]);
    }
  g_free (infos);

  g_key_file_free (key_file);
  g_object_unref (infos_file);
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

  priv->categories = g_hash_table_new_full (g_str_hash,
                                            g_str_equal,
                                            (GDestroyNotify) g_free,
                                            (GDestroyNotify) category_info_free);

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
  load_category_infos (ie);

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
