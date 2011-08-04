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

#include "hd-cairo-surface-cache.h"
#include "hd-bookmark-shortcut.h"
#include "hd-dbus-utils.h"

/* Size is taken from "hd-bookmark-shortcut.h" */
#define SHORTCUT_WIDTH HD_BOOKMARK_SHORTCUT_WIDTH
#define SHORTCUT_HEIGHT HD_BOOKMARK_SHORTCUT_HEIGHT

#define THUMBNAIL_WIDTH HD_BOOKMARK_THUMBNAIL_WIDTH
#define THUMBNAIL_HEIGHT HD_BOOKMARK_THUMBNAIL_HEIGHT

#define BORDER_WIDTH_LEFT HD_BOOKMARK_BORDER_WIDTH_LEFT
#define BORDER_WIDTH_TOP HD_BOOKMARK_BORDER_WIDTH_TOP

#define LABEL_WIDTH (SHORTCUT_WIDTH -                   \
                     (2 * HILDON_MARGIN_DEFAULT) -      \
                     (2 * HILDON_MARGIN_HALF))

#define LABEL_FONT "SmallSystemFont"

/* D-Bus method/interface to load URL in browser */
#define BROWSER_INTERFACE   "com.nokia.osso_browser"
#define BROWSER_PATH        "/com/nokia/osso_browser"
#define LOAD_URL_METHOD     "open_new_window"

/* GConf path for boomarks */
#define BOOKMARKS_GCONF_PATH      "/apps/osso/hildon-home/bookmarks"

#define HD_BOOKMARK_SHORTCUT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE (obj,\
                                                                            HD_TYPE_BOOKMARK_SHORTCUT,\
                                                                            HDBookmarkShortcutPrivate))

gint task_bookmarks_width;
gboolean task_bookmarks_hide_bg;

struct _HDBookmarkShortcutPrivate
{
  GtkWidget *label;

  gboolean button_pressed;

  gchar *url;

  GConfClient *gconf_client;

  cairo_surface_t *thumbnail_icon;
  cairo_surface_t *default_thumbnail_icon;

  cairo_surface_t *bg_image;
  cairo_surface_t *bg_active;
  cairo_surface_t *thumb_mask;
};

G_DEFINE_TYPE (HDBookmarkShortcut, hd_bookmark_shortcut, HD_TYPE_HOME_PLUGIN_ITEM);

static inline gchar *
get_gconf_key (const gchar *plugin_id,
               const gchar *suffix)
{
  return g_strdup_printf ("%s/%s/%s",
                          BOOKMARKS_GCONF_PATH,
                          plugin_id,
                          suffix);
}

static inline gchar *
get_string_from_gconf (GConfClient *client,
                       const gchar *plugin_id,
                       const gchar *suffix)
{
  gchar *key, *value;
  GError *error = NULL;

  key = get_gconf_key (plugin_id, suffix);
  value = gconf_client_get_string (client,
                                   key,
                                   &error);
  if (error)
    {
      g_warning ("%s. Could not read %s from GConf for bookmark shortcut %s. %s",
                 __FUNCTION__,
                 suffix,
                 plugin_id,
                 error->message);
      g_error_free (error);
    }

  g_free (key);

  return value;
}

static inline gchar *
get_label_from_gconf (GConfClient *client,
                      const gchar *plugin_id)
{
  return get_string_from_gconf (client,
                                plugin_id,
                                "label");
}

static inline gchar *
get_icon_path_from_gconf (GConfClient *client,
                          const gchar *plugin_id)
{
  return get_string_from_gconf (client,
                                plugin_id,
                                "icon");
}

static inline gchar *
get_url_from_gconf (GConfClient *client,
                    const gchar *plugin_id)
{
  return get_string_from_gconf (client,
                                plugin_id,
                                "url");
}

static inline cairo_surface_t *
scale_icon (cairo_surface_t *icon,
            double           width,
            double           height)
{
  cairo_surface_t *scaled_icon;
  cairo_t *cr;

  scaled_icon = cairo_surface_create_similar (icon,
                                              cairo_surface_get_content (icon),
                                              THUMBNAIL_WIDTH,
                                              THUMBNAIL_HEIGHT);

  cr = cairo_create (scaled_icon);

  cairo_scale (cr,
               THUMBNAIL_WIDTH / width,
               THUMBNAIL_HEIGHT / height);

  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_surface (cr,
                            icon,
                            0.0,
                            0.0);

  cairo_paint (cr);
  cairo_destroy (cr);

  return scaled_icon;
}

static inline cairo_surface_t *
scale_icon_if_required (cairo_surface_t *icon)
{
  cairo_surface_t *scaled_icon;
  int width, height;

  width = cairo_image_surface_get_width (icon);
  height = cairo_image_surface_get_height (icon);

  if (width == THUMBNAIL_WIDTH &&
      height == THUMBNAIL_HEIGHT)
    return cairo_surface_reference (icon);

  scaled_icon = scale_icon (icon, width, height);

  return scaled_icon;
}

static inline cairo_surface_t *
get_icon_from_gconf (GConfClient *client,
                     const gchar *plugin_id)
{
  gchar *icon_path;

  icon_path = get_icon_path_from_gconf (client, plugin_id);

  if (icon_path)
    {
      cairo_surface_t *icon, *scaled_icon = NULL;
      cairo_status_t status;

      icon = cairo_image_surface_create_from_png (icon_path);
      status = cairo_surface_status (icon);

      if (status != CAIRO_STATUS_SUCCESS)
        g_warning ("%s. Could not get thumbnail from file %s. %s",
                   __FUNCTION__,
                   icon_path,
                   cairo_status_to_string (status));
      else
        scaled_icon = scale_icon_if_required (icon);

      cairo_surface_destroy (icon);
      g_free (icon_path);

      return scaled_icon;
    }

  return NULL;
}

static void
hd_bookmark_shortcut_update_from_gconf (HDBookmarkShortcut *shortcut)
{
  HDBookmarkShortcutPrivate *priv = shortcut->priv;
  gchar *plugin_id;
  gchar *label;

  plugin_id = hd_plugin_item_get_plugin_id (HD_PLUGIN_ITEM (shortcut));

  /* Set label-text only on non-resized bookmarks. */
  if (HD_BOOKMARK_SHORTCUT_WIDTH == HD_BOOKMARK_DEF_SHORTCUT_WIDTH) {
    label = get_label_from_gconf (priv->gconf_client,
                                  plugin_id);

    gtk_label_set_text (GTK_LABEL (priv->label), 
                        label);
    g_free (label);
  }

  if (priv->thumbnail_icon)
    priv->thumbnail_icon = (cairo_surface_destroy (priv->thumbnail_icon), NULL);

  priv->thumbnail_icon = get_icon_from_gconf (priv->gconf_client,
                                              plugin_id);

  /* Get URL from GConf */
  g_free (priv->url);
  priv->url = get_url_from_gconf (priv->gconf_client,
                                  plugin_id);

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
    priv->gconf_client = (g_object_unref (priv->gconf_client), NULL);

  if (priv->bg_image)
    priv->bg_image = (cairo_surface_destroy (priv->bg_image), NULL);

  if (priv->bg_active)
    priv->bg_active = (cairo_surface_destroy (priv->bg_active), NULL);

  if (priv->thumb_mask)
    priv->thumb_mask = (cairo_surface_destroy (priv->thumb_mask), NULL);

  if (priv->thumbnail_icon)
    priv->thumbnail_icon = (cairo_surface_destroy (priv->thumbnail_icon), NULL);

  if (priv->default_thumbnail_icon)
    priv->default_thumbnail_icon = (cairo_surface_destroy (priv->default_thumbnail_icon), NULL);

  /* Chain up */
  G_OBJECT_CLASS (hd_bookmark_shortcut_parent_class)->dispose (object);
}

static void
create_default_thumbnail (HDBookmarkShortcut *shortcut,
                          cairo_surface_t    *surface)
{
  HDBookmarkShortcutPrivate *priv = shortcut->priv;

  if (!priv->default_thumbnail_icon)
    {
      cairo_t *cr;
      GtkStyle *style;
      GdkColor color;
      GdkPixbuf *pixbuf;

      priv->default_thumbnail_icon = cairo_surface_create_similar (surface,
                                                                   cairo_surface_get_content (surface),
                                                                   THUMBNAIL_WIDTH,
                                                                   THUMBNAIL_HEIGHT);
      cr = cairo_create (priv->default_thumbnail_icon);

      /* Paint background */
      style = gtk_widget_get_style (GTK_WIDGET (shortcut));

      cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

      if (gtk_style_lookup_color (style,
                                  "DefaultBackgroundColor",
                                  &color))
        gdk_cairo_set_source_color (cr, &color);
      else
        cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 1.0);

      cairo_paint (cr);

      pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                         "general_bookmark",
                                         64,
                                         GTK_ICON_LOOKUP_NO_SVG,
                                         NULL);

      if (pixbuf)
        {
          cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
          gdk_cairo_set_source_pixbuf (cr,
                                       pixbuf,
                                       (THUMBNAIL_WIDTH - gdk_pixbuf_get_width (pixbuf)) / 2.0,
                                       (THUMBNAIL_HEIGHT - gdk_pixbuf_get_height (pixbuf)) / 2.0);
          cairo_paint (cr);

          g_object_unref (pixbuf);
        }

      cairo_destroy (cr);
    }
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

  if (!url)
    {
      g_warning ("%s. Cannot open empty URL.", __FUNCTION__);
      return;
    }

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
hd_bookmark_shortcut_realize (GtkWidget *widget)
{
  GdkScreen *screen;

  screen = gtk_widget_get_screen (widget);
  gtk_widget_set_colormap (widget,
                           gdk_screen_get_rgba_colormap (screen));

  gtk_widget_set_app_paintable (widget,
                                TRUE);

  GTK_WIDGET_CLASS (hd_bookmark_shortcut_parent_class)->realize (widget);
}

static gboolean
hd_bookmark_shortcut_expose_event (GtkWidget *widget,
                                   GdkEventExpose *event)
{
  HDBookmarkShortcutPrivate *priv = HD_BOOKMARK_SHORTCUT (widget)->priv;
  cairo_t *cr;
  cairo_surface_t *bg;
  cairo_surface_t *thumbnail_icon;

  cr = gdk_cairo_create (GDK_DRAWABLE (widget->window));
  gdk_cairo_region (cr, event->region);
  cairo_clip (cr);

  if (priv->button_pressed)
    bg = priv->bg_active;
  else
    bg = priv->bg_image;

  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

  cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.0);
  cairo_paint (cr);

  if (priv->thumbnail_icon)
    thumbnail_icon = priv->thumbnail_icon;
  else
    {
      create_default_thumbnail (HD_BOOKMARK_SHORTCUT (widget),
                                bg);
      thumbnail_icon = priv->default_thumbnail_icon;
    }

  if (thumbnail_icon)
    {
      cairo_set_source_surface (cr,
                                thumbnail_icon,
                                BORDER_WIDTH_LEFT,
                                BORDER_WIDTH_TOP);
      if (priv->thumb_mask)
        cairo_mask_surface (cr,
                            priv->thumb_mask,
                            BORDER_WIDTH_LEFT,
                            BORDER_WIDTH_TOP);
    }

  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

  if (bg)
    {
      cairo_set_source_surface (cr, bg, 0.0, 0.0);
      cairo_paint (cr);
    }

  cairo_destroy (cr);

  return GTK_WIDGET_CLASS (hd_bookmark_shortcut_parent_class)->expose_event (widget,
                                                                             event);
}

static void
hd_bookmark_shortcut_style_set (GtkWidget *widget,
                                GtkStyle  *previous_style)
{
  HDBookmarkShortcutPrivate *priv = HD_BOOKMARK_SHORTCUT (widget)->priv;

  if (priv->default_thumbnail_icon)
    priv->default_thumbnail_icon = (cairo_surface_destroy (priv->default_thumbnail_icon), NULL);

  if (GTK_WIDGET_CLASS (hd_bookmark_shortcut_parent_class)->style_set)
    GTK_WIDGET_CLASS (hd_bookmark_shortcut_parent_class)->style_set (widget,
                                                                     previous_style);
}

static void
hd_bookmark_shortcut_class_init (HDBookmarkShortcutClass *klass)
{
  HDHomePluginItemClass *home_plugin_class = HD_HOME_PLUGIN_ITEM_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  home_plugin_class->get_applet_id = hd_bookmark_shortcut_get_applet_id;

  widget_class->realize = hd_bookmark_shortcut_realize;
  widget_class->expose_event = hd_bookmark_shortcut_expose_event;
  widget_class->style_set = hd_bookmark_shortcut_style_set;

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

      gtk_widget_queue_draw (widget);

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

      gtk_widget_queue_draw (widget);

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

  gtk_widget_queue_draw (widget);

  return FALSE;
}

static gboolean
delete_event_cb (GtkWidget          *widget,
                 GdkEvent           *event,
                 HDBookmarkShortcut *shortcut)
{
  gchar *plugin_id;

  plugin_id = hd_plugin_item_get_plugin_id (HD_PLUGIN_ITEM (shortcut));

  hd_shortcuts_remove_bookmark_shortcut (plugin_id);

  g_free (plugin_id);

  return FALSE;
}

static void
hd_bookmark_shortcut_init (HDBookmarkShortcut *applet)
{
  HDBookmarkShortcutPrivate *priv;
  GtkWidget *alignment;

  priv = HD_BOOKMARK_SHORTCUT_GET_PRIVATE (applet);
  applet->priv = priv;

  gtk_widget_add_events (GTK_WIDGET (applet),
                         GDK_BUTTON_PRESS_MASK |
                         GDK_BUTTON_RELEASE_MASK |
                         GDK_LEAVE_NOTIFY_MASK);
  g_signal_connect (applet, "button-press-event",
                    G_CALLBACK (button_press_event_cb), applet);
  g_signal_connect (applet, "button-release-event",
                    G_CALLBACK (button_release_event_cb), applet);
  g_signal_connect (applet, "leave-notify-event",
                    G_CALLBACK (leave_notify_event_cb), applet);

  alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment),
                             104, 8,
                             8, 8);
  gtk_widget_show (alignment);

  priv->label = gtk_label_new (NULL);
  gtk_widget_set_name (priv->label, "hildon-shadow-label");
  gtk_widget_show (priv->label);
  gtk_widget_set_size_request (priv->label, LABEL_WIDTH, -1);
  hildon_helper_set_logical_font (priv->label, LABEL_FONT);

  gtk_container_add (GTK_CONTAINER (applet), alignment);
  gtk_container_add (GTK_CONTAINER (alignment), priv->label);

  gtk_widget_set_size_request (GTK_WIDGET (applet), SHORTCUT_WIDTH, SHORTCUT_HEIGHT);
  g_signal_connect (applet, "delete-event",
                    G_CALLBACK (delete_event_cb), applet);

  if (!HD_BOOKMARK_HIDE_BG) {
    priv->bg_image = hd_cairo_surface_cache_get_surface (hd_cairo_surface_cache_get (),
                                                         HD_BOOKMARK_SCALED_BACKGROUND_IMAGE_FILE);
    priv->bg_active = hd_cairo_surface_cache_get_surface (hd_cairo_surface_cache_get (),
                                                         HD_BOOKMARK_SCALED_BACKGROUND_ACTIVE_IMAGE_FILE);
  } else {
    priv->bg_active = 0;
    priv->bg_image = 0;
  }
  priv->thumb_mask = hd_cairo_surface_cache_get_surface (hd_cairo_surface_cache_get (),
                                                       HD_BOOKMARK_SCALED_THUMBNAIL_MASK_FILE);

  priv->gconf_client = gconf_client_get_default ();
}
