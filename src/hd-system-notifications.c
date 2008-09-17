/*
 * This file is part of libhildondesktop
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

#include "hd-notification-manager.h"

#include "hd-system-notifications.h"

#define HD_SYSTEM_NOTIFICATIONS_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_SYSTEM_NOTIFICATIONS, HDSystemNotificationsPrivate))

typedef struct
{
  guint                  id;
  HDSystemNotifications *sn;
} HDSystemNotificationsDialogInfo;

struct _HDSystemNotificationsPrivate
{
  HDNotificationManager *nm;
  GQueue                *dialog_queue;
  GHashTable            *notifications;
};

G_DEFINE_TYPE (HDSystemNotifications, hd_system_notifications, G_TYPE_OBJECT);

static GtkWidget *
create_note_infoprint (const gchar *summary, 
                       const gchar *body, 
                       const gchar *icon_name)
{
  GtkWidget *banner;

  banner = GTK_WIDGET (g_object_new (HILDON_TYPE_BANNER, 
                                     "is-timed", FALSE,
                                     NULL));

  hildon_banner_set_markup (HILDON_BANNER (banner), body);
  hildon_banner_set_icon (HILDON_BANNER (banner), icon_name);

  return banner;
}

static void
show_next_system_dialog (HDSystemNotifications *sn)
{
  GtkWidget *next_dialog = NULL;

  g_queue_pop_head (sn->priv->dialog_queue);

  while (!g_queue_is_empty (sn->priv->dialog_queue))
    {
      next_dialog = (GtkWidget *) g_queue_peek_head (sn->priv->dialog_queue);

      if (GTK_IS_WIDGET (next_dialog))
        {
          gtk_widget_show_all (next_dialog);
          break;
        }
      else
        {
          g_queue_pop_head (sn->priv->dialog_queue);
        }
    }
}

static void
system_notification_dialog_destroy (GtkWidget             *widget,
                                    HDSystemNotifications *sn)
{
  show_next_system_dialog (sn);
}

static void
system_notification_dialog_response (GtkWidget                       *widget,
                                     gint                             response,	
                                     HDSystemNotificationsDialogInfo *ninfo)
{
  HDSystemNotifications *sn = ninfo->sn;

  hd_notification_manager_call_action (sn->priv->nm, ninfo->id, "default");
  hd_notification_manager_close_notification (sn->priv->nm, ninfo->id, NULL);

  g_free (ninfo);

  gtk_widget_destroy (widget);
}

static gboolean
hd_desktop_pulsate_progress_bar (gpointer user_data)
{
  if (GTK_IS_PROGRESS_BAR (user_data)) 
    {
      gtk_progress_bar_pulse (GTK_PROGRESS_BAR (user_data));
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static GtkWidget *
create_note_dialog (const gchar  *summary, 
                    const gchar  *body, 
                    const gchar  *icon_name,
                    gint          dialog_type,
                    gchar       **actions)
{
  GtkWidget *note;
  gint i;

  /* If it's a progress dialog, add the progress bar */
  if (dialog_type == 4)
    {
      GtkWidget *progressbar;

      progressbar = gtk_progress_bar_new ();

      gtk_progress_bar_pulse (GTK_PROGRESS_BAR (progressbar));

      note = hildon_note_new_cancel_with_progress_bar (NULL,
                                                       body,
                                                       GTK_PROGRESS_BAR (progressbar));

      g_timeout_add (100, hd_desktop_pulsate_progress_bar, progressbar);
    }
  else
    {
      note = hildon_note_new_information_with_icon_name (NULL, 
                                                         body, 
                                                         icon_name);
    }

  /* If there's a default action, get the label and set
   * the button text */
  for (i = 0; actions && actions[i] != NULL; i += 2)
    {
      gchar *label = actions[i + 1];

      if (g_str_equal (actions[i], "default"))
        {
          hildon_note_set_button_text (HILDON_NOTE (note), label);
          break;
        }
    }

  return note;
}

static void
notification_closed (HDNotificationManager *nm,
                     guint id,
                     HDSystemNotifications *sn)
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
notification_sent (HDNotificationManager  *nm,
                   const gchar            *app_name,
                   guint                   id,
                   const gchar            *icon,
                   const gchar            *summary,
                   const gchar            *body,
                   gchar                 **actions,
                   GHashTable             *hints,
                   gint                    timeout,
                   HDSystemNotifications  *sn)
{
  GtkWidget *notification = NULL;
  const gchar *category;

  g_return_if_fail (HD_IS_SYSTEM_NOTIFICATIONS (sn));

  /* Get category string */
  category = g_value_get_string (g_hash_table_lookup (hints, "category"));

  if (g_str_equal (category, "system.note.infoprint")) 
    {
      notification = create_note_infoprint (summary, 
                                            body, 
                                            icon);

      gtk_widget_show_all (notification);
    }
  else if (g_str_equal (category, "system.note.dialog")) 
    {
      HDSystemNotificationsDialogInfo *ninfo;
      gint dialog_type = 0;

      dialog_type = g_value_get_int (g_hash_table_lookup (hints, "dialog-type"));

      notification = create_note_dialog (summary, 
                                         body, 
                                         icon,
                                         dialog_type,
                                         actions);

      ninfo = g_new0 (HDSystemNotificationsDialogInfo, 1); 

      ninfo->id = id;
      ninfo->sn = sn;

      g_signal_connect (G_OBJECT (notification),
                        "response",
                        G_CALLBACK (system_notification_dialog_response),
                        ninfo);

      g_signal_connect (G_OBJECT (notification),
                        "destroy",
                        G_CALLBACK (system_notification_dialog_destroy),
                        sn);

      if (g_queue_is_empty (sn->priv->dialog_queue))
        {
          gtk_widget_show_all (notification);
        }

      g_queue_push_tail (sn->priv->dialog_queue, notification);
    } 
  else
    return

  g_hash_table_insert (sn->priv->notifications, 
                       GINT_TO_POINTER (id), 
                       notification);
}

static void
hd_system_notifications_class_init (HDSystemNotificationsClass *klass)
{
  g_type_class_add_private (klass, sizeof (HDSystemNotificationsPrivate));
}

static void
hd_system_notifications_init (HDSystemNotifications *sn)
{
  sn->priv = HD_SYSTEM_NOTIFICATIONS_GET_PRIVATE (sn);

  sn->priv->notifications = g_hash_table_new_full (g_direct_hash, 
                                                   g_direct_equal,
                                                   NULL,
                                                   NULL);
  sn->priv->dialog_queue = g_queue_new ();
}

HDSystemNotifications *
hd_system_notifications_get ()
{
  HDSystemNotifications *sn = g_object_new (HD_TYPE_SYSTEM_NOTIFICATIONS, NULL);

  sn->priv->nm = hd_notification_manager_get ();
  g_signal_connect (sn->priv->nm, "notification-sent",
                    G_CALLBACK (notification_sent), sn);
  g_signal_connect (sn->priv->nm, "notification-closed",
                    G_CALLBACK (notification_closed), sn);

  return sn;
}
