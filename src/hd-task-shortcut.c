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

#include "hd-task-manager.h"

#include "hd-task-shortcut.h"

/* Layout from Task navigation layout guide 1.2 */
#define SHORTCUT_WIDTH 142
#define SHORTCUT_HEIGHT 100

#define ICON_WIDTH 64
#define ICON_HEIGHT 64

#define ICON_BORDER HILDON_MARGIN_HALF

#define BORDER_WIDTH HILDON_MARGIN_DEFAULT

#define LABEL_WIDTH SHORTCUT_WIDTH

#define LABEL_FONT "SystemFont"
#define LABEL_COLOR "DefaultTextColor"

/* Private definitions */
#define HD_TASK_SHORTCUT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE (obj,\
                                                                        HD_TYPE_TASK_SHORTCUT,\
                                                                        HDTaskShortcutPrivate))

struct _HDTaskShortcutPrivate
{
  GtkWidget *label;
  GtkWidget *icon;

  gboolean button_pressed;
};

G_DEFINE_TYPE (HDTaskShortcut, hd_task_shortcut, HD_TYPE_HOME_PLUGIN_ITEM);

static void
hd_task_shortcut_desktop_file_changed_cb (HDTaskManager  *manager,
                                          HDTaskShortcut *shortcut)
{
  HDTaskShortcutPrivate *priv = shortcut->priv;
  gchar *desktop_id;
  const gchar *icon_name;

  desktop_id = hd_plugin_item_get_plugin_id (HD_PLUGIN_ITEM (shortcut));

  gtk_label_set_text (GTK_LABEL (priv->label),
                      hd_task_manager_get_label (manager,
                                                 desktop_id));

  icon_name = hd_task_manager_get_icon (manager, desktop_id);
 
  if (icon_name)
    {
      if (g_path_is_absolute (icon_name))
        gtk_image_set_from_file (GTK_IMAGE (priv->icon),
                                 icon_name);
      else
        gtk_image_set_from_icon_name (GTK_IMAGE (priv->icon),
                                      icon_name,
                                      GTK_ICON_SIZE_INVALID);
    }

  g_free (desktop_id);
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
  g_signal_connect (manager,
                    detailed_signal,
                    G_CALLBACK (hd_task_shortcut_desktop_file_changed_cb),
                    shortcut);

  /* Update label and icon if already there */
  hd_task_shortcut_desktop_file_changed_cb (manager,
                                            shortcut);

  /* Free memory */
  g_free (desktop_id);
  g_free (detailed_signal);
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

static void
rounded_rectangle (cairo_t *cr,
                   double   x,
                   double   y,
                   double   w,
                   double   h,
                   double   r)
{
        /*   A----BQ
         *  H      C
         *  |      |
         *  G      D
         *   F----E
         */

        cairo_move_to  (cr, x + r,     y);          /* Move to A */
        cairo_line_to  (cr, x + w - r, y);          /* Straight line to B */
        cairo_curve_to (cr, x + w,     y,           /* Curve to C, */
                            x + w,     y,           /* control points are both at Q */
                            x + w,     y + r);
        cairo_line_to  (cr, x + w,     y + h - r);  /* Move to D */
        cairo_curve_to (cr, x + w,     y + h,       /* Curve to E */
                            x + w,     y + h,
                            x + w - r, y + h);
        cairo_line_to  (cr, x + r,     y + h);      /* Line to F */
        cairo_curve_to (cr, x,         y + h,       /* Curve to G */
                            x,         y + h,
                            x,         y + h - r);
        cairo_line_to  (cr, x,         y + r);      /* Line to H */
        cairo_curve_to (cr, x,         y,
                            x,         y,
                            x + r,     y);          /* Curve to A */
}

static gboolean
hd_task_shortcut_expose_event (GtkWidget *widget,
                               GdkEventExpose *event)
{
  cairo_t *cr;

  cr = gdk_cairo_create (GDK_DRAWABLE (widget->window));
  gdk_cairo_region (cr, event->region);
  cairo_clip (cr);

  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.0);
  cairo_paint (cr);

  cairo_new_path (cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.5);
  rounded_rectangle (cr, BORDER_WIDTH, BORDER_WIDTH + (ICON_BORDER * 2.0) + ICON_HEIGHT,
                   SHORTCUT_WIDTH, SHORTCUT_HEIGHT - ICON_HEIGHT - (ICON_BORDER), BORDER_WIDTH * 2);
  cairo_fill (cr);

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

  return FALSE;
}

static void
hd_task_shortcut_init (HDTaskShortcut *applet)
{
  HDTaskShortcutPrivate *priv;
  GtkWidget *vbox, *alignment;

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

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox);

  alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_widget_show (alignment);
  gtk_widget_set_size_request (alignment, ICON_WIDTH + (ICON_BORDER * 2), ICON_HEIGHT + (ICON_BORDER * 2));
  gtk_container_set_border_width (GTK_CONTAINER (alignment), ICON_BORDER);

  priv->label = gtk_label_new (NULL);
  gtk_widget_show (priv->label);
  gtk_widget_set_size_request (priv->label, LABEL_WIDTH, -1);
  hildon_helper_set_logical_font (priv->label, LABEL_FONT);
  hildon_helper_set_logical_color (priv->label, GTK_RC_FG, GTK_STATE_NORMAL, LABEL_COLOR);

  priv->icon = gtk_image_new ();
  gtk_widget_show (priv->icon);
  gtk_image_set_pixel_size (GTK_IMAGE (priv->icon), ICON_WIDTH);
  gtk_widget_set_size_request (priv->icon, ICON_WIDTH, ICON_HEIGHT);

  gtk_container_add (GTK_CONTAINER (applet), vbox);
  gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 0);
  gtk_container_add (GTK_CONTAINER (alignment), priv->icon);
  gtk_box_pack_start (GTK_BOX (vbox), priv->label, TRUE, TRUE, 0);

  gtk_widget_set_size_request (GTK_WIDGET (applet), SHORTCUT_WIDTH + (BORDER_WIDTH * 2), SHORTCUT_HEIGHT + (BORDER_WIDTH * 2));
  gtk_container_set_border_width (GTK_CONTAINER (applet), BORDER_WIDTH);
}
