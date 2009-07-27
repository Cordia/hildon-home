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

#include "hd-cairo-surface-cache.h"
#include "hd-task-manager.h"
#include "hd-task-shortcut.h"

/* Layout from Task navigation layout guide 1.2 */
#define SHORTCUT_WIDTH 96
#define SHORTCUT_HEIGHT 96
#define ICON_WIDTH 64
#define ICON_HEIGHT 64

#define IMAGES_DIR                   "/etc/hildon/theme/images/"
#define BACKGROUND_IMAGE_FILE        IMAGES_DIR "ApplicationShortcutApplet.png"
#define BACKGROUND_ACTIVE_IMAGE_FILE IMAGES_DIR "ApplicationShortcutAppletPressed.png"

/* Private definitions */
#define HD_TASK_SHORTCUT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE (obj,\
                                                                        HD_TYPE_TASK_SHORTCUT,\
                                                                        HDTaskShortcutPrivate))

struct _HDTaskShortcutPrivate
{
  GtkWidget *icon;

  gboolean button_pressed;

  cairo_surface_t *bg_image;
  cairo_surface_t *bg_active;
};

G_DEFINE_TYPE (HDTaskShortcut, hd_task_shortcut, HD_TYPE_HOME_PLUGIN_ITEM);

static void
hd_task_shortcut_desktop_file_changed_cb (HDTaskManager  *manager,
                                          HDTaskShortcut *shortcut)
{
  HDTaskShortcutPrivate *priv = shortcut->priv;
  gchar *desktop_id;
  const gchar *icon_name;
  GdkPixbuf *pixbuf = NULL;

  desktop_id = hd_plugin_item_get_plugin_id (HD_PLUGIN_ITEM (shortcut));

  icon_name = hd_task_manager_get_icon (manager, desktop_id);
 
  if (icon_name)
    {
      if (g_path_is_absolute (icon_name))
        {
          GError *error = NULL;

          pixbuf = gdk_pixbuf_new_from_file_at_size (icon_name,
                                                     HILDON_ICON_PIXEL_SIZE_THUMB,
                                                     HILDON_ICON_PIXEL_SIZE_THUMB,
                                                     &error);
          if (error)
            {
              g_debug ("%s. Could not load icon %s from file. %s",
                       __FUNCTION__,
                       icon_name,
                       error->message);
              g_error_free (error);
            }
        }
      else
        {
          GError *error = NULL;

          pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                             icon_name,
                                             HILDON_ICON_PIXEL_SIZE_THUMB,
                                             GTK_ICON_LOOKUP_NO_SVG,
                                             &error);

          if (error)
            {
              g_debug ("%s. Could not load icon %s from theme. %s",
                       __FUNCTION__,
                       icon_name,
                       error->message);
              g_error_free (error);
            }
        }
    }

  if (!pixbuf)
    {
      GError *error = NULL;

      pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                         "tasklaunch_default_application",
                                         HILDON_ICON_PIXEL_SIZE_THUMB,
                                         GTK_ICON_LOOKUP_NO_SVG,
                                         &error);

      if (error)
        {
          g_warning ("%s. Could not load default application icon from theme. %s",
                     __FUNCTION__,
                     error->message);
          g_error_free (error);
        }
    }

  if (pixbuf)
    {
      gtk_image_set_from_pixbuf (GTK_IMAGE (priv->icon),
                                 pixbuf);
      g_object_unref (pixbuf);
    }

  if (icon_name)
    gtk_widget_show (GTK_WIDGET (shortcut));
  else
    gtk_widget_hide (GTK_WIDGET (shortcut));

  g_free (desktop_id);
}

static void
hd_task_shortcut_desktop_file_deleted_cb (HDTaskManager  *manager,
                                          HDTaskShortcut *shortcut)
{
  gboolean result;

  gtk_widget_hide (GTK_WIDGET (shortcut));

  g_signal_emit_by_name (shortcut, "delete-event", 0, &result);
}

static void
hd_task_shortcut_constructed (GObject *object)
{
  HDTaskShortcut *shortcut = HD_TASK_SHORTCUT (object);
  HDTaskManager *manager = hd_task_manager_get ();
  gchar *desktop_id, *detailed_signal;

  /* Chain up */
  G_OBJECT_CLASS (hd_task_shortcut_parent_class)->constructed (object);

  desktop_id = hd_plugin_item_get_plugin_id (HD_PLUGIN_ITEM (object));

  /* Connect to detailed desktop-file-changed signal */
  detailed_signal = g_strdup_printf ("desktop-file-changed::%s", desktop_id);
  g_signal_connect_object (manager,
                           detailed_signal,
                           G_CALLBACK (hd_task_shortcut_desktop_file_changed_cb),
                           shortcut,
                           0);
  g_free (detailed_signal);
  detailed_signal = g_strdup_printf ("desktop-file-deleted::%s", desktop_id);
  g_signal_connect_object (manager,
                           detailed_signal,
                           G_CALLBACK (hd_task_shortcut_desktop_file_deleted_cb),
                           shortcut,
                           0);

  /* Update label and icon if already there */
  hd_task_shortcut_desktop_file_changed_cb (manager,
                                            shortcut);

  /* Free memory */
  g_free (desktop_id);
  g_free (detailed_signal);
}

static void
hd_task_shortcut_dispose (GObject *object)
{
  HDTaskShortcutPrivate *priv = HD_TASK_SHORTCUT (object)->priv;
 
  if (priv->bg_image)
    priv->bg_image = (cairo_surface_destroy (priv->bg_image), NULL);

  if (priv->bg_active)
    priv->bg_active = (cairo_surface_destroy (priv->bg_active), NULL);

  G_OBJECT_CLASS (hd_task_shortcut_parent_class)->dispose (object);
}

static void
hd_task_shortcut_finalize (GObject *object)
{
  G_OBJECT_CLASS (hd_task_shortcut_parent_class)->finalize (object);
}

static gchar *
hd_task_shortcut_get_applet_id (HDHomePluginItem *item)
{
  gchar *p, *applet_id;

  p = HD_HOME_PLUGIN_ITEM_CLASS (hd_task_shortcut_parent_class)->get_applet_id (item);

  applet_id = g_strdup_printf ("TaskShortcut:%s", p);

  g_free (p);

  return applet_id;
}

static void
hd_task_shortcut_realize (GtkWidget *widget)
{
  GdkScreen *screen;

  screen = gtk_widget_get_screen (widget);
  gtk_widget_set_colormap (widget,
                           gdk_screen_get_rgba_colormap (screen));

  gtk_widget_set_app_paintable (widget,
                                TRUE);

  GTK_WIDGET_CLASS (hd_task_shortcut_parent_class)->realize (widget);
}

static gboolean
hd_task_shortcut_expose_event (GtkWidget *widget,
                               GdkEventExpose *event)
{
  HDTaskShortcutPrivate *priv = HD_TASK_SHORTCUT (widget)->priv; 
  cairo_t *cr;
  cairo_surface_t *bg;

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

  if (bg)
    {
      cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
      cairo_set_source_surface (cr, bg, 0.0, 0.0);
      cairo_paint (cr);
    }

  cairo_destroy (cr);

  return GTK_WIDGET_CLASS (hd_task_shortcut_parent_class)->expose_event (widget,
                                                                         event);
}

static void
hd_task_shortcut_class_init (HDTaskShortcutClass *klass)
{
  HDHomePluginItemClass *home_plugin_class = HD_HOME_PLUGIN_ITEM_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  home_plugin_class->get_applet_id = hd_task_shortcut_get_applet_id;

  object_class->constructed = hd_task_shortcut_constructed;
  object_class->dispose = hd_task_shortcut_dispose;
  object_class->finalize = hd_task_shortcut_finalize;

  widget_class->realize = hd_task_shortcut_realize;
  widget_class->expose_event = hd_task_shortcut_expose_event;

  g_type_class_add_private (klass, sizeof (HDTaskShortcutPrivate));
}

static gboolean
button_press_event_cb (GtkWidget      *widget,
                       GdkEventButton *event,
                       HDTaskShortcut *shortcut)
{
  HDTaskShortcutPrivate *priv = shortcut->priv;

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
                         HDTaskShortcut *shortcut)
{
  HDTaskShortcutPrivate *priv = shortcut->priv;

  if (event->button == 1 &&
      priv->button_pressed)
    {
      gchar *desktop_id;

      priv->button_pressed = FALSE;

      gtk_widget_queue_draw (widget);

      desktop_id = hd_plugin_item_get_plugin_id (HD_PLUGIN_ITEM (shortcut));

      hd_task_manager_launch_task (hd_task_manager_get (),
                                   desktop_id);

      g_free (desktop_id);

      return TRUE;
    }

  return FALSE;
}

static gboolean
leave_notify_event_cb (GtkWidget        *widget,
                       GdkEventCrossing *event,
                       HDTaskShortcut   *shortcut)
{
  HDTaskShortcutPrivate *priv = shortcut->priv;

  priv->button_pressed = FALSE;

  gtk_widget_queue_draw (widget);

  return FALSE;
}

static void
hd_task_shortcut_init (HDTaskShortcut *applet)
{
  HDTaskShortcutPrivate *priv;
  GtkWidget *alignment;

  priv = HD_TASK_SHORTCUT_GET_PRIVATE (applet);
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
  gtk_widget_show (alignment);

  priv->icon = gtk_image_new ();
  gtk_widget_show (priv->icon);
  gtk_image_set_pixel_size (GTK_IMAGE (priv->icon), ICON_WIDTH);
  gtk_widget_set_size_request (priv->icon, ICON_WIDTH, ICON_HEIGHT);

  gtk_container_add (GTK_CONTAINER (applet), alignment);
  gtk_container_add (GTK_CONTAINER (alignment), priv->icon);

  gtk_widget_set_size_request (GTK_WIDGET (applet), SHORTCUT_WIDTH, SHORTCUT_HEIGHT);

  priv->bg_image = hd_cairo_surface_cache_get_surface (hd_cairo_surface_cache_get (),
                                                       BACKGROUND_IMAGE_FILE);
  priv->bg_active = hd_cairo_surface_cache_get_surface (hd_cairo_surface_cache_get (),
                                                        BACKGROUND_ACTIVE_IMAGE_FILE);
  g_signal_connect_object (hd_cairo_surface_cache_get (), "changed",
                           G_CALLBACK (gtk_widget_queue_draw), applet, G_CONNECT_SWAPPED);
}
