/*
 * This file is part of hildon-home
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Based on hd-desktop.c (hildon-desktop)
 *   Copyright (C) 2006 Nokia Corporation.
 *
 *   Author:  Lucas Rocha <lucas.rocha@nokia.com>
 *   Contact: Karoliina Salminen <karoliina.t.salminen@nokia.com>
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

struct _HDSystemNotificationsPrivate
{
  HDNotificationManager *nm;
  GQueue                *dialog_queue;
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
system_notification_dialog_response (GtkWidget      *widget,
                                     gint            response,	
                                     HDNotification *notification)
{
  HDNotificationManager *nm = hd_notification_manager_get ();

  hd_notification_manager_call_action (nm, notification, "default");
  hd_notification_manager_close_notification (nm, hd_notification_get_id (notification), NULL);

  gtk_widget_destroy (widget);
}

static gboolean
hd_desktop_pulsate_progress_bar (gpointer user_data)
{
  gtk_progress_bar_pulse (GTK_PROGRESS_BAR (user_data));
  return TRUE;
}

static void
progressbar_destroy_cb (GtkObject *progressbar,
                        gpointer   user_data)
{
  g_source_remove (GPOINTER_TO_UINT (user_data));
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
      guint timeout_id;

      progressbar = gtk_progress_bar_new ();

      gtk_progress_bar_pulse (GTK_PROGRESS_BAR (progressbar));

      note = hildon_note_new_cancel_with_progress_bar (NULL,
                                                       body,
                                                       GTK_PROGRESS_BAR (progressbar));

      timeout_id = gdk_threads_add_timeout (100, hd_desktop_pulsate_progress_bar, progressbar);
      g_signal_connect (progressbar, "destroy",
                        G_CALLBACK (progressbar_destroy_cb),
                        GUINT_TO_POINTER (timeout_id));
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
system_notifications_notified (HDNotificationManager *nm,
                               HDNotification        *notification,
                               gboolean               replayed_event,
                               HDSystemNotifications *sn)
{
  GtkWidget *dialog = NULL;
  const gchar *category;

  g_return_if_fail (HD_IS_SYSTEM_NOTIFICATIONS (sn));

  /* Get category string */
  category = hd_notification_get_category (notification);

  if (category && g_str_equal (category, "system.note.infoprint"))
    {
      g_return_if_fail (!replayed_event);
      dialog = create_note_infoprint (hd_notification_get_summary (notification),
                                      hd_notification_get_body (notification), 
                                      hd_notification_get_icon (notification));

      gtk_widget_show_all (dialog);
    }
  else if (category && g_str_equal (category, "system.note.dialog")) 
    {
      g_return_if_fail (!replayed_event);
      dialog = create_note_dialog (hd_notification_get_summary (notification),
                                   hd_notification_get_body (notification), 
                                   hd_notification_get_icon (notification), 
                                   hd_notification_get_dialog_type (notification),
                                   hd_notification_get_actions (notification));

      g_signal_connect (dialog,
                        "response",
                        G_CALLBACK (system_notification_dialog_response),
                        notification);

      g_signal_connect_swapped (dialog,
                                "destroy",
                                G_CALLBACK (show_next_system_dialog),
                                sn);

      if (g_queue_is_empty (sn->priv->dialog_queue))
        gtk_widget_show_all (dialog);

      g_queue_push_tail (sn->priv->dialog_queue, dialog);
    } 
  else
    return;

  g_signal_connect_object (notification, "closed",
                           G_CALLBACK (gtk_widget_destroy), dialog,
                           G_CONNECT_SWAPPED);
}

static void
destroy_dialog (GtkWidget *dialog, HDSystemNotifications *sn)
{
  g_signal_handlers_disconnect_by_func (dialog, show_next_system_dialog, sn);
  gtk_widget_destroy (dialog);
}

static void
hd_system_notifications_dispose (GObject *object)
{
  HDSystemNotificationsPrivate *priv = HD_SYSTEM_NOTIFICATIONS (object)->priv;

  if (priv->dialog_queue)
    {
      g_queue_foreach (priv->dialog_queue, (GFunc) destroy_dialog, object);
      priv->dialog_queue = (g_queue_free (priv->dialog_queue), NULL);
    }

  G_OBJECT_CLASS (hd_system_notifications_parent_class)->dispose (object);
}

static void
hd_system_notifications_class_init (HDSystemNotificationsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = hd_system_notifications_dispose;

  g_type_class_add_private (klass, sizeof (HDSystemNotificationsPrivate));
}

static void
hd_system_notifications_init (HDSystemNotifications *sn)
{
  sn->priv = HD_SYSTEM_NOTIFICATIONS_GET_PRIVATE (sn);

  sn->priv->dialog_queue = g_queue_new ();
}

HDSystemNotifications *
hd_system_notifications_get ()
{
  HDSystemNotifications *sn = g_object_new (HD_TYPE_SYSTEM_NOTIFICATIONS, NULL);

  sn->priv->nm = hd_notification_manager_get ();
  g_signal_connect (sn->priv->nm, "notified",
                    G_CALLBACK (system_notifications_notified), sn);

  return sn;
}
