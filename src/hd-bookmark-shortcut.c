/*
 * This file is part of hildon-home.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <hildon/hildon.h>

#include <dbus/dbus.h>

#include <gconf/gconf-client.h>

#include "hd-bookmark-shortcut.h"

/* Size from Home layout guide 1.2 */
#define SHORTCUT_WIDTH 176
#define SHORTCUT_HEIGHT 146

#define THUMBNAIL_WIDTH 160
#define THUMBNAIL_HEIGHT 96

#define BORDER_WIDTH HILDON_MARGIN_DEFAULT

#define LABEL_WIDTH SHORTCUT_WIDTH - (2 * HILDON_MARGIN_DEFAULT) - (2 * HILDON_MARGIN_HALF)
#define LABEL_FONT "SmallSystemFont"
#define LABEL_COLOR "ReversedSecondaryTextColor"

/* D-Bus method/interface to load URL in browser */
#define BROWSER_INTERFACE   "com.nokia.osso_browser"
#define BROWSER_PATH        "/com/nokia/osso_browser"
#define LOAD_URL_METHOD     "load_url"

/* GConf path for boomarks */
#define BOOKMARKS_GCONF_PATH      "/apps/osso/hildon-home/bookmarks"
#define BOOKMARKS_GCONF_KEY_LABEL BOOKMARKS_GCONF_PATH "/%s/label"
#define BOOKMARKS_GCONF_KEY_URL   BOOKMARKS_GCONF_PATH "/%s/url"
#define BOOKMARKS_GCONF_KEY_ICON  BOOKMARKS_GCONF_PATH "/%s/icon"

#define HD_BOOKMARK_SHORTCUT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE (obj,\
                                                                            HD_TYPE_BOOKMARK_SHORTCUT,\
                                                                            HDBookmarkShortcutPrivate))

struct _HDBookmarkShortcutPrivate
{
  GtkWidget *label;
  GtkWidget *icon;

  gboolean button_pressed;

  gchar *url;

  GConfClient *gconf_client;
};

G_DEFINE_TYPE (HDBookmarkShortcut, hd_bookmark_shortcut, HD_TYPE_HOME_PLUGIN_ITEM);

static void
hd_bookmark_shortcut_update_from_gconf (HDBookmarkShortcut *shortcut)
{
  HDBookmarkShortcutPrivate *priv = shortcut->priv;
  gchar *plugin_id;
  gchar *key, *value;
  GError *error = NULL;

  plugin_id = hd_plugin_item_get_plugin_id (HD_PLUGIN_ITEM (shortcut));

  /* Get label value from GConf */
  key = g_strdup_printf (BOOKMARKS_GCONF_KEY_LABEL, plugin_id);
  value = gconf_client_get_string (priv->gconf_client,
                                   key,
                                   &error);

  /* Warn on error */
  if (error)
    {
      g_warning ("Could not read label value from GConf for bookmark shortcut %s. %s",
                 plugin_id,
                 error->message);
      g_error_free (error);
      error = NULL;
    }

  /* Set label */
  gtk_label_set_text (GTK_LABEL (priv->label), value);

  /* Free memory */
  g_free (key);
  g_free (value);

  /* Get icon path from GConf */
  key = g_strdup_printf (BOOKMARKS_GCONF_KEY_ICON, plugin_id);
  value = gconf_client_get_string (priv->gconf_client,
                                   key,
                                   &error);

  /* Warn on error */
  if (error)
    {
      g_warning ("Could not read icon path from GConf for bookmark shortcut %s. %s",
                 plugin_id,
                 error->message);
      g_error_free (error);
      error = NULL;
    }

  /* Set label */
  gtk_image_set_from_file (GTK_IMAGE (priv->icon), value);

  /* Free memory */
  g_free (key);
  g_free (value);

  /* Get URL from GConf */
  key = g_strdup_printf (BOOKMARKS_GCONF_KEY_URL, plugin_id);
  g_free (priv->url);
  priv->url = gconf_client_get_string (priv->gconf_client,
                                       key,
                                       &error);

  /* Warn on error */
  if (error)
    {
      g_warning ("Could not read URL from GConf for bookmark shortcut %s. %s",
                 plugin_id,
                 error->message);
      g_error_free (error);
      error = NULL;
    }

  /* Free memory */
  g_free (key);
  g_free (plugin_id);
}

static void
hd_bookmark_shortcut_constructed (GObject *object)
{
  /* Chain up */
  G_OBJECT_CLASS (hd_bookmark_shortcut_parent_class)->constructed (object);

  hd_bookmark_shortcut_update_from_gconf (HD_BOOKMARK_SHORTCUT (object));
}

static void
hd_bookmark_shortcut_dispose (GObject *object)
{
  HDBookmarkShortcutPrivate *priv = HD_BOOKMARK_SHORTCUT (object)->priv;

  if (priv->gconf_client)
    {
      g_object_unref (priv->gconf_client);
      priv->gconf_client = NULL;
    }

  /* Chain up */
  G_OBJECT_CLASS (hd_bookmark_shortcut_parent_class)->dispose (object);
}

static void
hd_bookmark_shortcut_finalize (GObject *object)
{
  HDBookmarkShortcutPrivate *priv = HD_BOOKMARK_SHORTCUT (object)->priv;

  g_free (priv->url);

  /* Chain up */
  G_OBJECT_CLASS (hd_bookmark_shortcut_parent_class)->finalize (object);
}

static void
hd_bookmark_shortcut_activate_service (const gchar *url)
{
  DBusMessage *msg = NULL;
  DBusError error;
  DBusConnection *conn;

  /* FIXME use libosso? */

  dbus_error_init (&error);
  conn = dbus_bus_get (DBUS_BUS_SESSION, &error);
  if (dbus_error_is_set (&error))
    {
      g_warning ("could not get session D-Bus: %s", error.message);
      dbus_error_free (&error);
      return;
    }

  msg = dbus_message_new_method_call (BROWSER_INTERFACE,
                                      BROWSER_PATH,
                                      BROWSER_INTERFACE,
                                      LOAD_URL_METHOD);
  if (msg == NULL)
    {
      g_warning ("failed to create message");
      return;
    }

  dbus_message_set_auto_start (msg, TRUE);
  dbus_message_set_no_reply (msg, TRUE);

  if (!dbus_message_append_args (msg,
                                 DBUS_TYPE_STRING, &url,
                                 DBUS_TYPE_INVALID))
    {
      g_warning ("failed to add url argument");
      return;
    }

  if (!dbus_connection_send (conn, msg, NULL))
    g_warning ("dbus_connection_send failed");

  dbus_message_unref (msg);
}

static gboolean
hd_bookmark_shortcut_activate (HDBookmarkShortcut  *shortcut,
                               GError         **error)
{
  HDBookmarkShortcutPrivate *priv = shortcut->priv;

  hd_bookmark_shortcut_activate_service (priv->url);

  return TRUE;
}

static gchar *
hd_bookmark_shortcut_get_applet_id (HDHomePluginItem *item)
{
  gchar *p, *applet_id;

  p = HD_HOME_PLUGIN_ITEM_CLASS (hd_bookmark_shortcut_parent_class)->get_applet_id (item);

  applet_id = g_strdup_printf ("BookmarkShortcut:%s", p);

  g_free (p);

  return applet_id;
}

static void
hd_bookmark_shortcut_class_init (HDBookmarkShortcutClass *klass)
{
  HDHomePluginItemClass *home_plugin_class = HD_HOME_PLUGIN_ITEM_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  home_plugin_class->get_applet_id = hd_bookmark_shortcut_get_applet_id;

  object_class->constructed = hd_bookmark_shortcut_constructed;
  object_class->dispose = hd_bookmark_shortcut_dispose;
  object_class->finalize = hd_bookmark_shortcut_finalize;

  g_type_class_add_private (klass, sizeof (HDBookmarkShortcutPrivate));
}

static gboolean
button_press_event_cb (GtkWidget      *widget,
                       GdkEventButton *event,
                       HDBookmarkShortcut *shortcut)
{
  HDBookmarkShortcutPrivate *priv = shortcut->priv;

  if (event->button == 1)
    {
      priv->button_pressed = TRUE;

      return TRUE;
    }

  return FALSE;
}

static gboolean
button_release_event_cb (GtkWidget      *widget,
                         GdkEventButton *event,
                         HDBookmarkShortcut *shortcut)
{
  HDBookmarkShortcutPrivate *priv = shortcut->priv;

  if (event->button == 1 &&
      priv->button_pressed)
    {
      priv->button_pressed = FALSE;

      hd_bookmark_shortcut_activate (shortcut, NULL);
      return TRUE;
    }

  return FALSE;
}

static gboolean
leave_notify_event_cb (GtkWidget        *widget,
                       GdkEventCrossing *event,
                       HDBookmarkShortcut   *shortcut)
{
  HDBookmarkShortcutPrivate *priv = shortcut->priv;

  priv->button_pressed = FALSE;

  return FALSE;
}

static gboolean
delete_event_cb (GtkWidget          *widget,
                 GdkEvent           *event,
                 HDBookmarkShortcut *shortcut)
{
  HDBookmarkShortcutPrivate *priv = shortcut->priv;
  gchar *plugin_id;
  gchar *key;
  GError *error = NULL;

  plugin_id = hd_plugin_item_get_plugin_id (HD_PLUGIN_ITEM (shortcut));

  /* Unset label value in GConf */
  key = g_strdup_printf (BOOKMARKS_GCONF_KEY_LABEL, plugin_id);
  gconf_client_unset (priv->gconf_client,
                      key,
                      &error);

  /* Warn on error */
  if (error)
    {
      g_warning ("Could not unset label value in GConf for bookmark shortcut %s. %s",
                 plugin_id,
                 error->message);
      g_error_free (error);
      error = NULL;
    }

  g_free (key);

  /* Unset icon path in GConf */
  key = g_strdup_printf (BOOKMARKS_GCONF_KEY_ICON, plugin_id);
  gconf_client_unset (priv->gconf_client,
                      key,
                      &error);

  /* Warn on error */
  if (error)
    {
      g_warning ("Could not unset icon path in GConf for bookmark shortcut %s. %s",
                 plugin_id,
                 error->message);
      g_error_free (error);
      error = NULL;
    }

  /* Free memory */
  g_free (key);

  /* Unset URL in GConf */
  key = g_strdup_printf (BOOKMARKS_GCONF_KEY_URL, plugin_id);
  gconf_client_unset (priv->gconf_client,
                      key,
                      &error);

  /* Warn on error */
  if (error)
    {
      g_warning ("Could not unset URL in GConf for bookmark shortcut %s. %s",
                 plugin_id,
                 error->message);
      g_error_free (error);
      error = NULL;
    }

  /* Free memory */
  g_free (key);
  g_free (plugin_id);

  return FALSE;
}

static void
hd_bookmark_shortcut_init (HDBookmarkShortcut *applet)
{
  HDBookmarkShortcutPrivate *priv;
  GtkWidget *ebox, *vbox, *alignment, *label_alignment;

  priv = HD_BOOKMARK_SHORTCUT_GET_PRIVATE (applet);
  applet->priv = priv;

  ebox = gtk_event_box_new ();
  gtk_widget_show (ebox);
  gtk_widget_add_events (ebox,
                         GDK_BUTTON_PRESS_MASK |
                         GDK_BUTTON_RELEASE_MASK |
                         GDK_LEAVE_NOTIFY_MASK);
  g_signal_connect (ebox, "button-press-event",
                    G_CALLBACK (button_press_event_cb), applet);
  g_signal_connect (ebox, "button-release-event",
                    G_CALLBACK (button_release_event_cb), applet);
  g_signal_connect (ebox, "leave-notify-event",
                    G_CALLBACK (leave_notify_event_cb), applet);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox);

  alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_widget_show (alignment);
  gtk_widget_set_size_request (alignment, THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT);

  label_alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_alignment_set_padding (GTK_ALIGNMENT (label_alignment), 0, 0, HILDON_MARGIN_HALF, HILDON_MARGIN_HALF);
  gtk_widget_show (label_alignment);

  priv->label = gtk_label_new (NULL);
  gtk_widget_show (priv->label);
  gtk_widget_set_size_request (priv->label, LABEL_WIDTH, -1);
  hildon_helper_set_logical_font (priv->label, LABEL_FONT);
  hildon_helper_set_logical_color (priv->label, GTK_RC_FG, GTK_STATE_NORMAL, LABEL_COLOR);

  priv->icon = gtk_image_new ();
  gtk_widget_show (priv->icon);
  /*  gtk_image_set_pixel_size (GTK_IMAGE (priv->icon), 64); */
  gtk_widget_set_size_request (priv->icon, THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT);

  gtk_container_add (GTK_CONTAINER (applet), ebox);
  gtk_container_add (GTK_CONTAINER (ebox), vbox);
  gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 0);
  gtk_container_add (GTK_CONTAINER (alignment), priv->icon);
  gtk_box_pack_start (GTK_BOX (vbox), label_alignment, TRUE, TRUE, 0);
  gtk_container_add (GTK_CONTAINER (label_alignment), priv->label);

  gtk_widget_set_size_request (GTK_WIDGET (applet), SHORTCUT_WIDTH, SHORTCUT_HEIGHT);
  gtk_container_set_border_width (GTK_CONTAINER (applet), BORDER_WIDTH);
  g_signal_connect (applet, "delete-event",
                    G_CALLBACK (delete_event_cb), applet);

  priv->gconf_client = gconf_client_get_default ();
}
