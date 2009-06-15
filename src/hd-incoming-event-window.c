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

#include <string.h>

#include <glib/gi18n.h>
#include <gdk/gdkx.h>
#include <hildon/hildon.h>

#include <X11/X.h>
#include <X11/Xatom.h>

#include "hd-cairo-surface-cache.h"
#include "hd-incoming-event-window.h"

/* Pixel sizes */
#define WINDOW_WIDTH 366
#define WINDOW_HEIGHT 88

#define WINDOW_MARGIN HILDON_MARGIN_DOUBLE

#define ICON_SIZE 32

#define ICON_SPACING HILDON_MARGIN_DEFAULT

#define TITLE_TEXT_PADDING 1
#define TITLE_TEXT_WIDTH (WINDOW_WIDTH - WINDOW_MARGIN - ICON_SIZE - ICON_SPACING - WINDOW_MARGIN)
#define TITLE_TEXT_HEIGHT 30
#define TITLE_TEXT_FONT "SystemFont"

#define SECONDARY_TEXT_WIDTH TITLE_TEXT_WIDTH
#define SECONDARY_TEXT_HEIGHT 24
#define SECONDARY_TEXT_FONT "SmallSystemFont"

#define IMAGES_DIR                   "/etc/hildon/theme/images/"
#define BACKGROUND_IMAGE_FILE        IMAGES_DIR "wmIncomingEvent.png"

#define MINUTE (60)
#define HOUR   (MINUTE * 60)
#define DAY    (HOUR * 24)
#define YEAR   (DAY * 365)

/* Timeout in seconds */
#define INCOMING_EVENT_WINDOW_PREVIEW_TIMEOUT 4

#define HD_INCOMING_EVENT_WINDOW_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_INCOMING_EVENT_WINDOW, HDIncomingEventWindowPrivate))

enum
{
  PROP_0,
  PROP_PREVIEW,
  PROP_DESTINATION,
  PROP_ICON,
  PROP_TITLE,
  PROP_TIME,
  PROP_AMOUNT,
  PROP_MESSAGE
};

enum {
    RESPONSE,
    N_SIGNALS
};

static guint signals[N_SIGNALS];  

struct _HDIncomingEventWindowPrivate
{
  gboolean preview;
  gchar *destination;

  GtkWidget *icon;
  GtkWidget *title;
  GtkWidget *amount_label;
  GtkWidget *message;

  time_t time;

  gulong amount;

  guint timeout_id;

  guint update_time_source;

  cairo_surface_t *bg_image;
};

G_DEFINE_TYPE (HDIncomingEventWindow, hd_incoming_event_window, GTK_TYPE_WINDOW);

static gboolean
hd_incoming_event_window_timeout (HDIncomingEventWindow *window)
{
  HDIncomingEventWindowPrivate *priv = window->priv;

  GDK_THREADS_ENTER ();

  priv->timeout_id = 0;

  g_signal_emit (window, signals[RESPONSE], 0, GTK_RESPONSE_DELETE_EVENT);

  GDK_THREADS_LEAVE ();

  return FALSE;
}

static gboolean
hd_incoming_event_window_map_event (GtkWidget   *widget,
                                    GdkEventAny *event)
{
  HDIncomingEventWindowPrivate *priv = HD_INCOMING_EVENT_WINDOW (widget)->priv;
  gboolean result = FALSE;

  if (GTK_WIDGET_CLASS (hd_incoming_event_window_parent_class)->map_event)
    result = GTK_WIDGET_CLASS (hd_incoming_event_window_parent_class)->map_event (widget,
                                                                                  event);

  if (priv->preview)
    {
      priv->timeout_id = g_timeout_add_seconds (INCOMING_EVENT_WINDOW_PREVIEW_TIMEOUT,
                                                (GSourceFunc) hd_incoming_event_window_timeout,
                                                widget);
    }

  return result;
}

static gboolean
hd_incoming_event_window_delete_event (GtkWidget   *widget,
                                       GdkEventAny *event)
{
  HDIncomingEventWindowPrivate *priv = HD_INCOMING_EVENT_WINDOW (widget)->priv;

  if (priv->timeout_id)
    {
      g_source_remove (priv->timeout_id);
      priv->timeout_id = 0;
    }

  g_signal_emit (widget, signals[RESPONSE], 0, GTK_RESPONSE_DELETE_EVENT);

  return TRUE;
}

static gboolean
hd_incoming_event_window_button_press_event (GtkWidget      *widget,
                                             GdkEventButton *event)
{
  HDIncomingEventWindowPrivate *priv = HD_INCOMING_EVENT_WINDOW (widget)->priv;

  if (priv->timeout_id)
    {
      g_source_remove (priv->timeout_id);
      priv->timeout_id = 0;
    }

  /* Emit the ::response signal */
  g_signal_emit (widget, signals[RESPONSE], 0, GTK_RESPONSE_OK);

  return TRUE;
}

static void
hd_incoming_event_window_set_string_xwindow_property (GtkWidget *widget,
                                                      const gchar *prop,
                                                      const gchar *value)
{
  Atom atom;
  GdkWindow *window;
  GdkDisplay *dpy;

  /* Check if widget is realized. */
  if (!GTK_WIDGET_REALIZED (widget))
    return;

  window = widget->window;

  dpy = gdk_drawable_get_display (window);
  atom = gdk_x11_get_xatom_by_name_for_display (dpy, prop);

  if (value)
    {
      /* Set property to given value */
      XChangeProperty (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XID (window),
                       atom, XA_STRING, 8, PropModeReplace,
                       (const guchar *)value, strlen (value));
    }
  else
    {
      /* Delete property if no value is given */
      XDeleteProperty (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XID (window),
                       atom);
    }
}

/*
 * Update the displayed relative time in the task switcher notification thumbnail window
 */
static gboolean
hd_incoming_event_window_update_time (HDIncomingEventWindow *window)
{
  HDIncomingEventWindowPrivate *priv = window->priv;
  time_t current_time, difference, timeout;
  gchar *time_text;

  time (&current_time);

  difference = current_time - priv->time;

  if (difference < 2 * MINUTE)
    {
      time_text = g_strdup (dgettext ("hildon-libs",
                                      "wdgt_va_ago_one_minute"));
      timeout = (2 * MINUTE) - difference;
    }
  else if (difference < HOUR)
    {
      time_text = g_strdup_printf (dgettext ("hildon-libs",
                                             "wdgt_va_ago_minutes"),
                                   difference / MINUTE);
      timeout = MINUTE - (difference % MINUTE);
    }
  else if (difference < (2 * HOUR))
    {
      time_text = g_strdup (dgettext ("hildon-libs",
                                      "wdgt_va_ago_one_hour"));
      timeout = (2 * HOUR) - difference;
    }
  else if (difference < DAY)
    {
      time_text = g_strdup_printf (dgettext ("hildon-libs",
                                             "wdgt_va_ago_hours"),
                                   difference / HOUR);
      timeout = HOUR - (difference % HOUR);
    }
  else if (difference < 2 * DAY)
    {
      time_text = g_strdup (dgettext ("hildon-libs",
                                      "wdgt_va_ago_one_day"));
      timeout = 2 * DAY - difference;
    }
  else if (difference < YEAR)
    {
      time_text = g_strdup_printf (dgettext ("hildon-libs",
                                             "wdgt_va_ago_days"),
                                   difference / DAY);
      timeout = DAY - (difference % DAY);
    }
  else if (difference < 2 * YEAR)
    {
      time_text = g_strdup (dgettext ("hildon-libs",
                                      "wdgt_va_ago_one_year"));
      timeout = 2 * YEAR - difference;
    }
  else
    {
      time_text = g_strdup_printf (dgettext ("hildon-libs",
                                             "wdgt_va_ago_years"),
                                   difference / YEAR);
      timeout = YEAR - (difference % YEAR);
    }

  hd_incoming_event_window_set_string_xwindow_property (GTK_WIDGET (window),
                                                        "_HILDON_INCOMING_EVENT_NOTIFICATION_TIME",
                                                        time_text);
  if (priv->update_time_source)
    priv->update_time_source = (g_source_remove (priv->update_time_source), 0);

  priv->update_time_source = g_timeout_add_seconds (timeout,
                                                    (GSourceFunc) hd_incoming_event_window_update_time,
                                                    window);

  g_free (time_text);

  return FALSE;
}

static void
hd_incoming_event_window_update_title_and_amount (HDIncomingEventWindow *window)
{
  HDIncomingEventWindowPrivate *priv = window->priv;
  gchar *display_amount, *title;

  display_amount = g_strdup_printf ("%lu", priv->amount);
  title = g_strdup_printf ("%s%s",
                           priv->amount > 1 ? "... " : "",
                           gtk_label_get_text (GTK_LABEL (priv->title)));

  hd_incoming_event_window_set_string_xwindow_property (GTK_WIDGET (window),
                                                        "_HILDON_INCOMING_EVENT_NOTIFICATION_AMOUNT",
                                                        display_amount);
  hd_incoming_event_window_set_string_xwindow_property (GTK_WIDGET (window),
                                                        "_HILDON_INCOMING_EVENT_NOTIFICATION_SUMMARY",
                                                        title);

  g_free (display_amount);
  g_free (title);
}

static void
hd_incoming_event_window_realize (GtkWidget *widget)
{
  HDIncomingEventWindowPrivate *priv = HD_INCOMING_EVENT_WINDOW (widget)->priv;
  GdkScreen *screen;
  const gchar *notification_type, *icon;
  GtkIconSize icon_size;
  GdkPixmap *pixmap;
  cairo_t *cr;

  screen = gtk_widget_get_screen (widget);
  gtk_widget_set_colormap (widget,
                           gdk_screen_get_rgba_colormap (screen));

  gtk_widget_set_app_paintable (widget,
                                TRUE);

  gtk_window_set_decorated (GTK_WINDOW (widget), FALSE);

  GTK_WIDGET_CLASS (hd_incoming_event_window_parent_class)->realize (widget);

  /* Notification window */
  gdk_window_set_type_hint (widget->window, GDK_WINDOW_TYPE_HINT_NOTIFICATION);

  /* Set the _NET_WM_WINDOW_TYPE property to _HILDON_WM_WINDOW_TYPE_HOME_APPLET */
  if (priv->preview)
    notification_type = "_HILDON_NOTIFICATION_TYPE_PREVIEW";
  else
    notification_type = "_HILDON_NOTIFICATION_TYPE_INCOMING_EVENT";
  hd_incoming_event_window_set_string_xwindow_property (widget,
                                            "_HILDON_NOTIFICATION_TYPE",
                                            notification_type);

  /* Assume the properties have already been set.  Earlier these X window
   * properties couldn't be set because we weren't realized. */
  gtk_image_get_icon_name (GTK_IMAGE (priv->icon), &icon, &icon_size);
  hd_incoming_event_window_set_string_xwindow_property (widget,
                             "_HILDON_INCOMING_EVENT_NOTIFICATION_ICON",
                             icon);
  hd_incoming_event_window_set_string_xwindow_property (widget,
                          "_HILDON_INCOMING_EVENT_NOTIFICATION_SUMMARY",
                          gtk_label_get_text (GTK_LABEL (priv->title)));
  hd_incoming_event_window_set_string_xwindow_property (widget,
                             "_HILDON_INCOMING_EVENT_NOTIFICATION_MESSAGE",
                             gtk_label_get_label (GTK_LABEL (priv->message)));
  hd_incoming_event_window_set_string_xwindow_property (widget,
                          "_HILDON_INCOMING_EVENT_NOTIFICATION_DESTINATION",
                          priv->destination);
  hd_incoming_event_window_update_time (HD_INCOMING_EVENT_WINDOW (widget));
  hd_incoming_event_window_update_title_and_amount (HD_INCOMING_EVENT_WINDOW (widget));

  /* Set background to transparent pixmap */
  pixmap = gdk_pixmap_new (GDK_DRAWABLE (widget->window), 1, 1, -1);
  cr = gdk_cairo_create (GDK_DRAWABLE (pixmap));
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.0);
  cairo_paint (cr);
  cairo_destroy (cr);

  gdk_window_set_back_pixmap (widget->window, pixmap, FALSE);
}

static gboolean
hd_incoming_event_window_expose_event (GtkWidget *widget,
                                       GdkEventExpose *event)
{
  HDIncomingEventWindowPrivate *priv = HD_INCOMING_EVENT_WINDOW (widget)->priv;
  cairo_t *cr;

  cr = gdk_cairo_create (GDK_DRAWABLE (widget->window));
  gdk_cairo_region (cr, event->region);
  cairo_clip (cr);

  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

  cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.0);
  cairo_paint (cr);

  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

  cairo_set_source_surface (cr, priv->bg_image, 0.0, 0.0);
  cairo_paint (cr);

  cairo_destroy (cr);

  return GTK_WIDGET_CLASS (hd_incoming_event_window_parent_class)->expose_event (widget,
                                                                                 event);
}

static void
hd_incoming_event_window_dispose (GObject *object)
{
  HDIncomingEventWindowPrivate *priv = HD_INCOMING_EVENT_WINDOW (object)->priv;

  if (priv->timeout_id)
    {
      g_source_remove (priv->timeout_id);
      priv->timeout_id = 0;
    }

  if (priv->update_time_source)
    priv->update_time_source = (g_source_remove (priv->update_time_source), 0);

  if (priv->bg_image)
    priv->bg_image = (cairo_surface_destroy (priv->bg_image), NULL);

  G_OBJECT_CLASS (hd_incoming_event_window_parent_class)->dispose (object);
}

static void
hd_incoming_event_window_finalize (GObject *object)
{
  /* HDIncomingEventWindowPrivate *priv = HD_INCOMING_EVENT_WINDOW (object)->priv; */

  G_OBJECT_CLASS (hd_incoming_event_window_parent_class)->finalize (object);
}

static void
hd_incoming_event_window_get_property (GObject      *object,
                                       guint         prop_id,
                                       GValue       *value,
                                       GParamSpec   *pspec)
{
  HDIncomingEventWindowPrivate *priv = HD_INCOMING_EVENT_WINDOW (object)->priv;

  switch (prop_id)
    {
    case PROP_PREVIEW:
      g_value_set_boolean (value, priv->preview);
      break;

    case PROP_DESTINATION:
      g_value_set_string (value, priv->destination);
      break;

    case PROP_ICON:
        {
          const gchar *icon_name;
          
          gtk_image_get_icon_name (GTK_IMAGE (priv->icon), &icon_name, NULL);

          g_value_set_string (value, icon_name);
        }
      break;

    case PROP_TITLE:
      g_value_set_string (value, gtk_label_get_label (GTK_LABEL (priv->title)));
      break;

    case PROP_TIME:
      g_value_set_long (value, priv->time);
      break;

    case PROP_AMOUNT:
      g_value_set_ulong (value, priv->amount);
      break;

    case PROP_MESSAGE:
      g_value_set_string (value, gtk_label_get_label (GTK_LABEL (priv->message)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
hd_incoming_event_window_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  HDIncomingEventWindowPrivate *priv = HD_INCOMING_EVENT_WINDOW (object)->priv;

  switch (prop_id)
    {
    case PROP_PREVIEW:
      /* NOTE Neither we nor the wm supports changing the preview
       * property of an incoming event window. */
      priv->preview = g_value_get_boolean (value);
      break;

    case PROP_DESTINATION:
      g_free (priv->destination);
      priv->destination = g_value_dup_string (value);
      hd_incoming_event_window_set_string_xwindow_property (
                         GTK_WIDGET (object),
                         "_HILDON_INCOMING_EVENT_NOTIFICATION_DESTINATION",
                         priv->destination);
      break;

    case PROP_ICON:
      gtk_image_set_from_icon_name (GTK_IMAGE (priv->icon),
                                    g_value_get_string (value),
                                    HILDON_ICON_SIZE_STYLUS);
      hd_incoming_event_window_set_string_xwindow_property (
                             GTK_WIDGET (object),
                             "_HILDON_INCOMING_EVENT_NOTIFICATION_ICON",
                             g_value_get_string (value));
      break;

    case PROP_TITLE:
      gtk_label_set_text (GTK_LABEL (priv->title), g_value_get_string (value));
      hd_incoming_event_window_update_title_and_amount (HD_INCOMING_EVENT_WINDOW (object));
      break;

    case PROP_TIME:
      priv->time = g_value_get_long (value);
      hd_incoming_event_window_update_time (HD_INCOMING_EVENT_WINDOW (object));
      break;

    case PROP_AMOUNT:
      priv->amount = g_value_get_ulong (value);
      hd_incoming_event_window_update_title_and_amount (HD_INCOMING_EVENT_WINDOW (object));
      if (priv->amount > 1)
        {
          gchar *display_amount;

          display_amount = g_strdup_printf ("%lu", priv->amount);
          gtk_label_set_text (GTK_LABEL (priv->amount_label),
                              display_amount);

          g_free (display_amount);
        }
      else
        gtk_label_set_text (GTK_LABEL (priv->amount_label), "");
      break;

    case PROP_MESSAGE:
      gtk_label_set_text (GTK_LABEL (priv->message), g_value_get_string (value));
      hd_incoming_event_window_set_string_xwindow_property (
                          GTK_WIDGET (object),
                          "_HILDON_INCOMING_EVENT_NOTIFICATION_MESSAGE",
                          g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
hd_incoming_event_window_class_init (HDIncomingEventWindowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  widget_class->button_press_event = hd_incoming_event_window_button_press_event;
  widget_class->delete_event = hd_incoming_event_window_delete_event;
  widget_class->map_event = hd_incoming_event_window_map_event;
  widget_class->realize = hd_incoming_event_window_realize;
  widget_class->expose_event = hd_incoming_event_window_expose_event;

  object_class->dispose = hd_incoming_event_window_dispose;
  object_class->finalize = hd_incoming_event_window_finalize;
  object_class->get_property = hd_incoming_event_window_get_property;
  object_class->set_property = hd_incoming_event_window_set_property;

  signals[RESPONSE] = g_signal_new ("response",
                                    G_OBJECT_CLASS_TYPE (klass),
                                    G_SIGNAL_RUN_LAST,
                                    G_STRUCT_OFFSET (HDIncomingEventWindowClass, response),
                                    NULL, NULL,
                                    g_cclosure_marshal_VOID__INT,
                                    G_TYPE_NONE, 1,
                                    G_TYPE_INT);

  g_object_class_install_property (object_class,
                                   PROP_PREVIEW,
                                   g_param_spec_boolean ("preview",
                                                         "Preview",
                                                         "If the window is a preview window",
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class,
                                   PROP_DESTINATION,
                                   g_param_spec_string ("destination",
                                                        "Destination",
                                                        "The application we can associate this notification with",
                                                        NULL,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_ICON,
                                   g_param_spec_string ("icon",
                                                        "Icon",
                                                        "The icon-name of the incoming event",
                                                        NULL,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_TITLE,
                                   g_param_spec_string ("title",
                                                        "Title",
                                                        "The title of the incoming event",
                                                        NULL,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_TIME,
                                   g_param_spec_long ("time",
                                                      "Time",
                                                      "The time of the incoming event (time_t)",
                                                      G_MINLONG,
                                                      G_MAXLONG,
                                                      -1,
                                                      G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_AMOUNT,
                                   g_param_spec_ulong ("amount",
                                                       "Amount",
                                                       "The amount of incoming events",
                                                       1,
                                                       G_MAXULONG,
                                                       1,
                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class,
                                   PROP_MESSAGE,
                                   g_param_spec_string ("message",
                                                        "Message",
                                                        "The message of the incoming event",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  /* Add shadow to label */
  gtk_rc_parse_string ("style \"HDIncomingEventWindow-Text\" = \"osso-color-themeing\" {\n"
                       "  fg[NORMAL] = @NotificationTextColor\n"
                       "  engine \"sapwood\" {\n"
                       "    shadowcolor = @DefaultTextColor\n"
                       "  }\n"
                       "} widget \"*.HDIncomingEventWindow-Text\" style \"HDIncomingEventWindow-Text\"\n"
                       "style \"HDIncomingEventWindow-Secondary\" = \"osso-color-themeing\" {\n"
                       "  fg[NORMAL] = @NotificationSecondaryTextColor\n"
                       "  engine \"sapwood\" {\n"
                       "    shadowcolor = @DefaultTextColor\n"
                       "  }\n"
                       "} widget \"*.HDIncomingEventWindow-Secondary\" style \"HDIncomingEventWindow-Secondary\"");
  g_type_class_add_private (klass, sizeof (HDIncomingEventWindowPrivate));
}

static void
hd_incoming_event_window_init (HDIncomingEventWindow *window)
{
  HDIncomingEventWindowPrivate *priv = HD_INCOMING_EVENT_WINDOW_GET_PRIVATE (window);
  GtkWidget *main_table;

  window->priv = priv;

  main_table = gtk_table_new (2, 2, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (main_table), ICON_SPACING);
  gtk_container_set_border_width (GTK_CONTAINER (main_table), WINDOW_MARGIN);
  gtk_widget_set_size_request (GTK_WIDGET (main_table), WINDOW_WIDTH, WINDOW_HEIGHT);
  gtk_widget_show (main_table);

  priv->icon = gtk_image_new ();
/*  gtk_image_set_pixel_size (GTK_IMAGE (priv->icon), HILDON_ICON_SIZE_STYLUS); */
  gtk_widget_set_size_request (priv->icon, ICON_SIZE, ICON_SIZE);
  gtk_widget_show (priv->icon);
  gtk_table_attach (GTK_TABLE (main_table),
                    priv->icon,
                    0, 1,
                    0, 1,
                    0, 0,
                    0, 0);

  priv->title = gtk_label_new (NULL);
  gtk_widget_set_name (GTK_WIDGET (priv->title), "HDIncomingEventWindow-Text");
  gtk_misc_set_alignment (GTK_MISC (priv->title), 0.0, 0.5);
  gtk_widget_set_size_request (priv->title, TITLE_TEXT_WIDTH, TITLE_TEXT_HEIGHT);
  hildon_helper_set_logical_font (priv->title, TITLE_TEXT_FONT);
  gtk_widget_show (priv->title);
  gtk_table_attach (GTK_TABLE (main_table),
                    priv->title,
                    1, 2,
                    0, 1,
                    GTK_EXPAND | GTK_FILL, GTK_FILL,
                    0, TITLE_TEXT_PADDING);

  priv->amount_label = gtk_label_new (NULL);
  gtk_widget_set_name (GTK_WIDGET (priv->amount_label), "HDIncomingEventWindow-Text");
  gtk_misc_set_alignment (GTK_MISC (priv->amount_label), 0.5, 0.5);
  gtk_widget_set_size_request (priv->amount_label, ICON_SIZE, SECONDARY_TEXT_HEIGHT);
  hildon_helper_set_logical_font (priv->amount_label, SECONDARY_TEXT_FONT);
  gtk_widget_show (priv->amount_label);
  gtk_table_attach (GTK_TABLE (main_table),
                    priv->amount_label,
                    0, 1,
                    1, 2,
                    GTK_FILL, GTK_FILL,
                    0, 0);

  priv->message = gtk_label_new (NULL);
  gtk_widget_set_name (GTK_WIDGET (priv->message), "HDIncomingEventWindow-Secondary");
  gtk_misc_set_alignment (GTK_MISC (priv->message), 0.0, 0.5);
  gtk_widget_set_size_request (priv->message, SECONDARY_TEXT_WIDTH, SECONDARY_TEXT_HEIGHT);
  hildon_helper_set_logical_font (priv->message, SECONDARY_TEXT_FONT);
  gtk_widget_show (priv->message);
  gtk_table_attach (GTK_TABLE (main_table),
                    priv->message,
                    1, 2,
                    1, 2,
                    GTK_EXPAND | GTK_FILL, GTK_FILL,
                    0, 0);

  /* Pack containers */
  gtk_container_add (GTK_CONTAINER (window), main_table);

  /* Enable handling of button press events */
  gtk_widget_add_events (GTK_WIDGET (window), GDK_BUTTON_PRESS_MASK);

  /* Don't take focus away from the toplevel application. */
  gtk_window_set_accept_focus (GTK_WINDOW (window), FALSE);

  /* bg image */
  g_signal_connect_object (hd_cairo_surface_cache_get (), "changed",
                           G_CALLBACK (gtk_widget_queue_draw), window, G_CONNECT_SWAPPED);
  priv->bg_image = hd_cairo_surface_cache_get_surface (hd_cairo_surface_cache_get (),
                                                       BACKGROUND_IMAGE_FILE);


  gtk_widget_set_size_request (GTK_WIDGET (window),
                               cairo_image_surface_get_width (priv->bg_image),
                               cairo_image_surface_get_height (priv->bg_image));
}

GtkWidget *
hd_incoming_event_window_new (gboolean     preview,
                              const gchar *destination,
                              const gchar *summary,
                              const gchar *body,
                              time_t       time,
                              const gchar *icon)
{
  GtkWidget *window;

  window = g_object_new (HD_TYPE_INCOMING_EVENT_WINDOW,
                         "preview", preview,
                         "destination", destination,
                         "title", summary,
                         "message", body,
                         "icon", icon,
                         "time", time,
                         NULL);

  return window;
}

