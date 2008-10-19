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

#include <string.h>

#include <gtk/gtk.h>
#include <hildon/hildon.h>
#include <dbus/dbus.h>

#include <glib/gi18n.h>

#include "hd-bookmark-shortcut.h"

/* Plugin ID bookmark-shortcut prefix */
#define PLUGIN_ID_FORMAT "-x-bookmark-shortcut-%s"

#define HD_BOOKMARK_SHORTCUT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE (obj,\
                                                                            HD_TYPE_BOOKMARK_SHORTCUT,\
                                                                            HDBookmarkShortcutPrivate))

struct _HDBookmarkShortcutPrivate
{
  gchar *uri;

  GtkWidget *label;
  GtkWidget *icon;

  gboolean button_pressed;
};

enum
{
  PROP_0,
  PROP_URI,
};

G_DEFINE_TYPE (HDBookmarkShortcut, hd_bookmark_shortcut, HD_TYPE_HOME_PLUGIN_ITEM);

static void
hd_bookmark_shortcut_set_uri (HDBookmarkShortcut *shortcut,
                              const gchar        *uri)
{
  HDBookmarkShortcutPrivate *priv = shortcut->priv;

  priv->uri = g_strdup (uri);

  gtk_label_set_text (GTK_LABEL (priv->label), uri);

  /* FIXME Set label and icon from bookmark manager/file */
}

static void
hd_bookmark_shortcut_finalize (GObject *object)
{
  HDBookmarkShortcutPrivate *priv = HD_BOOKMARK_SHORTCUT (object)->priv;

  g_free (priv->uri);
  priv->uri = NULL;

  G_OBJECT_CLASS (hd_bookmark_shortcut_parent_class)->finalize (object);
}

static void
hd_bookmark_shortcut_get_property (GObject      *object,
                                   guint         prop_id,
                                   GValue       *value,
                                   GParamSpec   *pspec)
{
  HDBookmarkShortcutPrivate *priv = HD_BOOKMARK_SHORTCUT (object)->priv;

  switch (prop_id)
    {
    case PROP_URI:
      g_value_set_string (value, priv->uri);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
hd_bookmark_shortcut_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_URI:
      hd_bookmark_shortcut_set_uri (HD_BOOKMARK_SHORTCUT (object),
                                    g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

#define BROWSER_INTERFACE   "com.nokia.osso_browser"
#define BROWSER_PATH        "/com/nokia/osso_browser"
#define LOAD_URL_METHOD     "load_url"

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

  hd_bookmark_shortcut_activate_service (priv->uri);

  return TRUE;
}


static void
hd_bookmark_shortcut_class_init (HDBookmarkShortcutClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = hd_bookmark_shortcut_finalize;
  object_class->get_property = hd_bookmark_shortcut_get_property;
  object_class->set_property = hd_bookmark_shortcut_set_property;

  g_object_class_install_property (object_class,
                                   PROP_URI,
                                   g_param_spec_string ("uri",
                                                        "URI",
                                                        "URI of the bookmark",
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

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

static void
hd_bookmark_shortcut_init (HDBookmarkShortcut *applet)
{
  HDBookmarkShortcutPrivate *priv;
  GtkWidget *ebox, *vbox, *alignment;

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
  gtk_widget_set_size_request (alignment, 166, 100);

  priv->label = gtk_label_new ("http://www.google.com");
  gtk_widget_show (priv->label);
  gtk_widget_set_size_request (priv->label, 158, -1);

  priv->icon = gtk_image_new ();
  gtk_widget_show (priv->icon);
  /*  gtk_image_set_pixel_size (GTK_IMAGE (priv->icon), 64); */
  gtk_widget_set_size_request (priv->icon, 166, 100);

  gtk_container_add (GTK_CONTAINER (applet), ebox);
  gtk_container_add (GTK_CONTAINER (ebox), vbox);
  gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 0);
  gtk_container_add (GTK_CONTAINER (alignment), priv->icon);
  gtk_box_pack_start (GTK_BOX (vbox), priv->label, FALSE, FALSE, 0);

  /*  gtk_window_set_opacity (GTK_WINDOW (applet), 0); */
}

GtkWidget *
hd_bookmark_shortcut_new (const gchar *uri)
{
  GtkWidget *shortcut;
  gchar *escaped_uri, *id;

  /* Escape dashes in uri */
  escaped_uri = g_strdup (uri);
  g_strdelimit (escaped_uri, "/", '_'); /* FIXME is this unique? */

  id = g_strdup_printf (PLUGIN_ID_FORMAT, escaped_uri);

  shortcut = g_object_new (HD_TYPE_BOOKMARK_SHORTCUT,
                           "plugin-id", id,
                           "uri", uri,
                           NULL);

  g_free (escaped_uri);
  g_free (id);

  return shortcut;
}
