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
#define NOTIFICATION_GROUP_KEY_TITLE_TEXT "Title-Text"
#define NOTIFICATION_GROUP_KEY_TITLE_TEXT_EMPTY "Title-Text-Empty"
#define NOTIFICATION_GROUP_KEY_SECONDARY_TEXT "Secondary-Text"
#define NOTIFICATION_GROUP_KEY_SECONDARY_TEXT_EMPTY "Secondary-Text-Empty"
#define NOTIFICATION_GROUP_KEY_ICON "Icon"
#define NOTIFICATION_GROUP_KEY_DBUS_CALL "D-Bus-Call"
#define NOTIFICATION_GROUP_KEY_TEXT_DOMAIN "Text-Domain"
#define NOTIFICATION_GROUP_KEY_NO_WINDOW "No-Window"
#define NOTIFICATION_GROUP_KEY_ACCOUNT_CALL "Account-Call"
#define NOTIFICATION_GROUP_KEY_ACCOUNT_HINT "Account-Hint"
#define NOTIFICATION_GROUP_KEY_LED_PATTERN "LED-Pattern"
#define NOTIFICATION_GROUP_KEY_GROUP "Group"
#define NOTIFICATION_GROUP_KEY_SPLIT_IN_THREADS "Split-In-Threads"

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
  gchar *title_text;
  gchar *title_text_empty;
  gchar *secondary_text;
  gchar *secondary_text_empty;
  gchar *icon;
  gchar *dbus_callback;
  gchar *text_domain;
  gchar *account_hint;
  gchar *account_call;
  gchar *pattern;
  gchar *group;
  gchar *split_in_threads;
  gboolean no_window : 1;
} CategoryInfo;

typedef void (*NotificationsCallback) (Notifications *ns,
                                       gpointer       data);

/*
 * Used to group notifications with the same (mapped) category
 * toegther for preview and switcher windows.
 */
struct _Notifications
{
  gchar                 *group;
  const gchar           *thread; /* pointer in group to the thread part */

  GPtrArray             *notifications;
  NotificationsCallback  cb;
  gpointer               cb_data;

  GtkWidget             *window;
};

struct _HDIncomingEventsPrivate
{
  GHashTable      *categories;

  GList           *preview_list;
  GtkWidget       *preview_window;

  GHashTable      *switcher_groups;

  GPtrArray       *plugins;

  HDPluginManager *plugin_manager;

  DBusGProxy      *mce_proxy;
};

G_DEFINE_TYPE (HDIncomingEvents, hd_incoming_events, G_TYPE_OBJECT);

/* Check if category is mapped to a virtual category
 * and returns the virtual category in this case, 
 * else return category */
static const gchar *
notification_get_group (HDNotification *n)
{
  HDIncomingEvents *ie = hd_incoming_events_get ();
  HDIncomingEventsPrivate *priv = ie->priv;
  const gchar *category;
  CategoryInfo *info = NULL;

  /* Lookup info for notification */
  category = hd_notification_get_category (n);
  if (category)
    info = g_hash_table_lookup (priv->categories,
                                category);

  if (info && info->group)
    return info->group;

  return category;
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
 * Returns: a Notifications structure containing just 
 * notification @n
 */
static Notifications *
notifications_new_for_notification (HDNotification *n,
                                    const gchar    *thread)
{
  Notifications *ns;
  GPtrArray *notifications = g_ptr_array_sized_new (1);

  ns = g_slice_new0 (Notifications);
  ns->notifications = notifications;

  g_object_ref (n);
  g_ptr_array_add (notifications,
                   n);
  g_signal_connect (n, "closed",
                    G_CALLBACK (notification_closed_cb), ns);

  if (thread)
    {
      const gchar *group = notification_get_group (n);

      ns->group = g_strdup_printf ("%s#%s",
                                   group,
                                   thread);

      ns->thread = ns->group + strlen (group) + 1;
    }
  else
    ns->group = g_strdup (notification_get_group (n));


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

  g_free (ns->group);

  for (i = 0; i < notifications->len; i++)
    {
      HDNotification *n = g_ptr_array_index (notifications, i);

      g_signal_handlers_disconnect_by_func (n, G_CALLBACK (notification_closed_cb), ns);

      g_object_unref (n);
    }

  g_ptr_array_free (notifications, TRUE);

  /* Last notification in this group was closed,
   *  destroy window */
  if (GTK_IS_WIDGET (ns->window))
    ns->window = (gtk_widget_destroy (ns->window), NULL);

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
  g_return_if_fail (ns->group);

  /* The notifications are always of the same (mapped) category */
  g_return_if_fail (!g_strcmp0 (ns->group, other->group));

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

static CategoryInfo *
notifications_get_category_info (Notifications *ns)
{
  HDIncomingEvents *ie = hd_incoming_events_get ();
  HDIncomingEventsPrivate *priv = ie->priv;
  HDNotification *notification;

  if (notifications_is_empty (ns))
    return NULL;

  notification = g_ptr_array_index (ns->notifications,
                                    ns->notifications->len - 1);

  return g_hash_table_lookup (priv->categories,
                              hd_notification_get_category (notification));
}

/* If account call is available, check if the account hint is the same for
 * each notification
 */
static const gchar *
notifications_get_common_account (Notifications *ns)
{
  CategoryInfo *info;
  const gchar *account = NULL;
  guint i;

  info = notifications_get_category_info (ns);

  if (!info || !info->account_call || !info->account_hint)
    return NULL;


  for (i = 0; i < ns->notifications->len; i++)
    {
      GValue *value;
      HDNotification *n = g_ptr_array_index (ns->notifications,
                                             i); 

      value = hd_notification_get_hint (n, info->account_hint);

      if (!value || !G_VALUE_HOLDS_STRING (value))
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

/* notifications_get_amount:
 * @ns: a #Notifications instance
 *
 * Returns the amount of notifications taking the amount hint into account
 */
static guint
notifications_get_amount (Notifications *ns)
{
  guint i, amount = 0;

  for (i = 0; i < ns->notifications->len; i++)
    {
      GValue *v;
      HDNotification *n = g_ptr_array_index (ns->notifications,
                                             i);

      v = hd_notification_get_hint (n, "amount");
      if (v && G_VALUE_HOLDS_UINT (v))
        amount += MAX (g_value_get_uint (v), 1);
      else if (v && G_VALUE_HOLDS_INT (v))
        amount += MAX (g_value_get_int (v), 1);
      else
        amount++;
    }

  return amount;
}

/* Activate an array of notifications
 *
 * Returns: %TRUE if the notifications were sucessfullly activated
 */ 
static gboolean
notifications_activate (Notifications *ns)
{
  CategoryInfo *info;
  guint i;
  const gchar *common_account;

  if (notifications_is_empty (ns))
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

  if (notifications_get_amount (ns) == 1)
    {
      /* Always call default action if it is only one notification */
      HDNotification *n = g_ptr_array_index (ns->notifications, 0);

      hd_notification_manager_call_action (hd_notification_manager_get (),
                                           n,
                                           "default");
      notifications_close_all (ns);
      return TRUE;
    }

  info = notifications_get_category_info (ns);
  common_account = notifications_get_common_account (ns);

  if (common_account)
    {
      hd_notification_manager_call_dbus_callback_with_arg (hd_notification_manager_get (),
                                                           info->account_call,
                                                           common_account);
      notifications_close_all (ns);
      return TRUE;
    }

  if (info && info->dbus_callback)
    {
      /* Call D-Bus callback if available else call default action on each notification */
      hd_notification_manager_call_dbus_callback (hd_notification_manager_get (),
                                                  info->dbus_callback);
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
  HDNotification *notification;
  CategoryInfo *info;
  const gchar *title_text, *secondary_text;
  const gchar *icon, *destination;

  /* Destroy the window when all notifications are closed */
  if (notifications_is_empty (ns))
    {
      gtk_widget_destroy (window);
      return;
    }

  /* Update the window with information about the latest notification */
  notification = g_ptr_array_index (ns->notifications,
                                    ns->notifications->len - 1);
  info = notifications_get_category_info (ns);

  if (info && info->title_text)
    title_text = info->title_text;
  else
    {
      title_text = hd_notification_get_summary (notification);

      if (!title_text || !title_text[0])
        {
          if (info && info->title_text_empty)
            title_text = info->title_text_empty;
        }
    }

  if (info && info->secondary_text)
    secondary_text = info->secondary_text;
  else
    {
      secondary_text = hd_notification_get_body (notification);

      if (!secondary_text || !secondary_text[0])
        {
          if (info && info->secondary_text_empty)
            secondary_text = info->secondary_text_empty;
        }
    }

  /* Icon */
  if (info && info->icon)
    icon = info->icon;
  else
    icon = hd_notification_get_icon (notification);

  /* Destination */
  if (ns->thread)
    destination = ns->thread;
  else if (info && info->destination)
    destination = info->destination;
  else
    destination = NULL;

  g_object_set (window,
                "title", title_text,
                "message", secondary_text,
                "icon", icon,
                "time", (gint64) hd_notification_get_time (notification),
                "amount", notifications_get_amount (ns),
                "destination", destination,
                NULL);
}

static void
switcher_window_response (HDIncomingEventWindow *window,
                          gint                   response_id,
                          Notifications         *ns)
{
  if (response_id == GTK_RESPONSE_OK)
    notifications_activate (ns);
  else
    notifications_close_all (ns);
}

static void
notifications_update_switcher_window (Notifications *ns,
                                      GtkWidget     *window)
{
  HDIncomingEvents *ie = hd_incoming_events_get ();
  HDIncomingEventsPrivate *priv = ie->priv;
  CategoryInfo *info;

  info = notifications_get_category_info (ns);

  if (notifications_is_empty (ns))
    {
      g_hash_table_remove (priv->switcher_groups,
                           ns->group);
    }
  else
    {
      /* At least one notification needs to be displayed,
       * create a window if it does not exist yet */
      if (!GTK_IS_WIDGET (ns->window))
        {
          ns->window = hd_incoming_event_window_new (FALSE,
                                                     info->destination,
                                                     NULL, NULL, -1, NULL);
          g_signal_connect (ns->window, "response",
                            G_CALLBACK (switcher_window_response),
                            ns);
        }

      notifications_update_window (ns,
                                   ns->window);

      gtk_widget_show (ns->window);
    }
}

/* notifications_split_in_threads:
 * 
 * Return a new Hashtable with group->Notifications pairs, where group
 * consists of group#thread when a thread is given.
 */
static GHashTable *
notifications_split_in_threads (Notifications *ns)
{
  GHashTable *table;
  CategoryInfo *info;

  table = g_hash_table_new (g_str_hash, g_str_equal);

  info = notifications_get_category_info (ns);

  if (!info->split_in_threads)
    {
      g_hash_table_insert (table,
                           ns->group,
                           ns);
    }
  else
    {
      guint i;

      for (i = 0; i < ns->notifications->len; i++)
        {
          HDNotification *n = g_ptr_array_index (ns->notifications,
                                                 i);
          GValue *v;
          const gchar *thread = NULL;
          Notifications *thread_ns, *new_ns;

          v = hd_notification_get_hint (n, info->split_in_threads);
          if (v && G_VALUE_HOLDS_STRING (v))
            thread = g_value_get_string (v);

          new_ns = notifications_new_for_notification (n,
                                                       thread);

          thread_ns = g_hash_table_lookup (table,
                                           new_ns->group);

          g_debug ("%s. Thread: %s", __FUNCTION__, new_ns->group);

          if (thread_ns)
            {
              notifications_append (thread_ns,
                                    new_ns);
              notifications_free (new_ns);
            }
          else
            {
              g_hash_table_insert (table,
                                   new_ns->group,
                                   new_ns);
            }
        }

      notifications_free (ns);
    }

  return table;
}

static void
notifications_add_to_switcher (Notifications *ns)
{
  HDIncomingEvents *ie = hd_incoming_events_get ();
  HDIncomingEventsPrivate *priv = ie->priv;
  CategoryInfo *info;

  if (notifications_is_empty (ns))
    return;

  info = notifications_get_category_info (ns);
  
  if (info)
    {
      GHashTable *threads;
      GHashTableIter iter;
      gpointer key, value;

      threads = notifications_split_in_threads (ns);

      g_hash_table_iter_init (&iter, threads);
      while (g_hash_table_iter_next (&iter, &key, &value)) 
        {
          Notifications *thread_ns = value;
          Notifications *group_ns = g_hash_table_lookup (priv->switcher_groups,
                                                         key);

          if (group_ns)
            {
              notifications_append (group_ns,
                                    thread_ns);
              notifications_free (thread_ns);
            }
          else
            {
              group_ns = thread_ns;
              g_hash_table_insert (priv->switcher_groups,
                                   g_strdup (thread_ns->group),
                                   thread_ns);
            }

          notifications_update_switcher_window (group_ns,
                                                NULL);
          group_ns->cb = (NotificationsCallback) notifications_update_switcher_window;
          group_ns->cb_data = NULL;
        }

      g_hash_table_destroy (threads);
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
                        ns);
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
          goto end;
        }
    }
  else if (response_id == GTK_RESPONSE_DELETE_EVENT)
    {
      notifications_add_to_switcher (ns);
      goto end;
    }

  notifications_free (ns);

end:
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

  if (!g_strcmp0 (l_a->group, l_b->group))
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
  CategoryInfo *info;

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

  ns = notifications_new_for_notification (notification, NULL);
  info = notifications_get_category_info (ns);

  /* Replayed events are just added to the switcher */
  if (replayed_event)
    {
      notifications_add_to_switcher (ns);

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
  if (p && G_VALUE_HOLDS_STRING (p))
    pattern = g_value_get_string (p);
  if (!pattern && info)
    pattern = info->pattern;

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
  if (info && info->no_window)
    return;

  /* Check if no notification windows should be shown */
  p = hd_notification_get_hint (notification, "no-notification-window");
  if ((G_VALUE_HOLDS_BOOLEAN (p) && g_value_get_boolean (p)) ||
      (G_VALUE_HOLDS_UCHAR (p) && g_value_get_uchar (p)))
    {
      /* Send dbus request to mce to turn display backlight on */
      if (priv->mce_proxy)
        {
          g_debug ("%s. Call %s",
                   __FUNCTION__,
                   MCE_DISPLAY_ON_REQ);
          dbus_g_proxy_call_no_reply (priv->mce_proxy, MCE_DISPLAY_ON_REQ,
                                      G_TYPE_INVALID, G_TYPE_INVALID);
        }
      return;      
    }

  if (info)
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
  g_free (info->title_text);
  g_free (info->title_text_empty);
  g_free (info->secondary_text);
  g_free (info->secondary_text_empty);
  g_free (info->icon);
  g_free (info->dbus_callback);
  g_free (info->text_domain);
  g_free (info->account_call);
  g_free (info->account_hint);
  g_free (info->pattern);
  g_free (info->group);
  g_free (info);
}

static gchar *
get_translated_string (GKeyFile *keyfile,
                       gchar    *group,
                       gchar    *key,
                       gchar    *translation_domain,
                       GError  **error)
{
  gchar *value, *translated;

  value = g_key_file_get_string (keyfile,
                                 group,
                                 key,
                                 error);

  if (!value)
    return value;

  translated = g_strdup (dgettext (translation_domain ? translation_domain : GETTEXT_PACKAGE,
                                   value));
  g_free (value);

  return translated;  
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
      GError *error = NULL;

      info = g_new0 (CategoryInfo, 1);

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

      info->text_domain = g_key_file_get_string (key_file,
                                                 infos[i],
                                                 NOTIFICATION_GROUP_KEY_TEXT_DOMAIN,
                                                 NULL);

      info->title_text = get_translated_string (key_file,
                                                infos[i],
                                                NOTIFICATION_GROUP_KEY_TITLE_TEXT,
                                                info->text_domain,
                                                NULL);

      info->title_text_empty = get_translated_string (key_file,
                                                      infos[i],
                                                      NOTIFICATION_GROUP_KEY_TITLE_TEXT_EMPTY,
                                                      info->text_domain,
                                                      NULL);

      info->secondary_text = get_translated_string (key_file,
                                                    infos[i],
                                                    NOTIFICATION_GROUP_KEY_SECONDARY_TEXT,
                                                    info->text_domain,
                                                    NULL);

      info->secondary_text_empty = get_translated_string (key_file,
                                                          infos[i],
                                                          NOTIFICATION_GROUP_KEY_SECONDARY_TEXT_EMPTY,
                                                          info->text_domain,
                                                          NULL);

      info->icon = g_key_file_get_string (key_file,
                                          infos[i],
                                          NOTIFICATION_GROUP_KEY_ICON,
                                          NULL);

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

      info->split_in_threads = g_key_file_get_string (key_file,
                                                      infos[i],
                                                      NOTIFICATION_GROUP_KEY_SPLIT_IN_THREADS,
                                                      NULL);

      info->group = g_key_file_get_string (key_file,
                                           infos[i],
                                           NOTIFICATION_GROUP_KEY_GROUP,
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

  priv->switcher_groups = g_hash_table_new_full (g_str_hash,
                                                 g_str_equal,
                                                 (GDestroyNotify) g_free,
                                                 (GDestroyNotify) notifications_free);
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
