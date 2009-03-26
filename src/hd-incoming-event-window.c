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

#include <gdk/gdkx.h>
#include <hildon/hildon.h>

#include <X11/X.h>
#include <X11/Xatom.h>

#include "hd-cairo-surface-cache.h"
#include "hd-incoming-event-window.h"

/* Pixel sizes */
#define INCOMING_EVENT_WINDOW_WIDTH 342
#define INCOMING_EVENT_WINDOW_HEIGHT 80

#define INCOMING_EVENT_WINDOW_CLOSE  43
#define INCOMING_EVENT_WINDOW_ICON  24

#define MARGIN_DEFAULT 8
#define MARGIN_HALF 4

#define IMAGES_DIR                   "/etc/hildon/theme/images/"
#define BACKGROUND_IMAGE_FILE        IMAGES_DIR "wmIncomingEvent.png"

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
  GtkWidget *time_label;
  GtkWidget *cbox;
  GtkWidget *message;

  time_t time;

  guint timeout_id;

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

static void
hd_incoming_event_window_realize (GtkWidget *widget)
{
  HDIncomingEventWindowPrivate *priv = HD_INCOMING_EVENT_WINDOW (widget)->priv;
  GdkScreen *screen;
  const gchar *notification_type, *icon;
  GtkIconSize icon_size;

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
                             "_HILDON_INCOMING_EVENT_NOTIFICATION_TIME",
                             gtk_label_get_label (GTK_LABEL (priv->time_label)));
  hd_incoming_event_window_set_string_xwindow_property (widget,
                          "_HILDON_INCOMING_EVENT_NOTIFICATION_SUMMARY",
                          gtk_label_get_text (GTK_LABEL (priv->title)));
  hd_incoming_event_window_set_string_xwindow_property (widget,
                             "_HILDON_INCOMING_EVENT_NOTIFICATION_MESSAGE",
                             gtk_label_get_label (GTK_LABEL (priv->message)));
  hd_incoming_event_window_set_string_xwindow_property (widget,
                          "_HILDON_INCOMING_EVENT_NOTIFICATION_DESTINATION",
                          priv->destination);
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
hd_incoming_event_window_size_request (GtkWidget      *widget,
                                       GtkRequisition *requisition)
{
  GTK_WIDGET_CLASS (hd_incoming_event_window_parent_class)->size_request (widget, requisition);

  requisition->width = INCOMING_EVENT_WINDOW_WIDTH;
  requisition->height = INCOMING_EVENT_WINDOW_HEIGHT;
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
      g_value_set_int64 (value, priv->time);
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
      /* Close button is not shown in preview windows */
      if (priv->preview)
        gtk_widget_hide (priv->cbox);
      else
        gtk_widget_show (priv->cbox);
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
                                    HILDON_ICON_SIZE_SMALL);
      hd_incoming_event_window_set_string_xwindow_property (
                             GTK_WIDGET (object),
                             "_HILDON_INCOMING_EVENT_NOTIFICATION_ICON",
                             g_value_get_string (value));
      break;

    case PROP_TITLE:
      gtk_label_set_text (GTK_LABEL (priv->title), g_value_get_string (value));
      hd_incoming_event_window_set_string_xwindow_property (
                          GTK_WIDGET (object),
                          "_HILDON_INCOMING_EVENT_NOTIFICATION_SUMMARY",
                          g_value_get_string (value));
      break;

    case PROP_TIME:
        {
          gchar buf[20] = "";

          priv->time = g_value_get_int64 (value);
          if (priv->time >= 0)
            strftime (buf, 20, "%H:%M", localtime (&(priv->time)));
          gtk_label_set_text (GTK_LABEL (priv->time_label), buf);
        }
      break;

    case PROP_MESSAGE:
      gtk_label_set_text (GTK_LABEL (priv->message), g_value_get_string (value));
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
  widget_class->size_request = hd_incoming_event_window_size_request;

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
                                   g_param_spec_int64 ("time",
                                                       "Time",
                                                       "The time of the incoming event (time_t)",
                                                       G_MININT64,
                                                       G_MAXINT64,
                                                       -1,
                                                       G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_MESSAGE,
                                   g_param_spec_string ("message",
                                                        "Message",
                                                        "The message of the incoming event",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  /* Add shadow to label */
  gtk_rc_parse_string ("style \"HDIncomingEventWindow-Label\" = \"osso-color-themeing\" {\n"
                       "  fg[NORMAL] = @ReversedTextColor\n"
                       "  text[NORMAL] = @ReversedTextColor\n"
                       "  engine \"sapwood\" {\n"
                       "    shadowcolor = @DefaultTextColor\n"
                       "  }\n"
                       "} widget \"*.HDIncomingEventWindow-Label\" style \"HDIncomingEventWindow-Label\"");

  g_type_class_add_private (klass, sizeof (HDIncomingEventWindowPrivate));
}

static void
hd_incoming_event_window_init (HDIncomingEventWindow *window)
{
  HDIncomingEventWindowPrivate *priv = HD_INCOMING_EVENT_WINDOW_GET_PRIVATE (window);
  GtkWidget *vbox, *hbox, *title_box, *message_box, *fbox, *hsep;
  GtkSizeGroup *icon_size_group;

  window->priv = priv;

  vbox = gtk_vbox_new (FALSE, HILDON_MARGIN_DEFAULT);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), HILDON_MARGIN_DEFAULT);
  gtk_widget_show (vbox);

  hbox = gtk_hbox_new (FALSE, HILDON_MARGIN_DEFAULT);
  gtk_widget_show (hbox);

  title_box = gtk_hbox_new (FALSE, HILDON_MARGIN_HALF);
  gtk_widget_show (title_box);

  message_box = gtk_hbox_new (FALSE, HILDON_MARGIN_DEFAULT);
  gtk_widget_show (message_box);

  icon_size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  priv->icon = gtk_image_new ();
  gtk_widget_show (priv->icon);
  gtk_image_set_pixel_size (GTK_IMAGE (priv->icon), INCOMING_EVENT_WINDOW_ICON);
/*  gtk_widget_set_size_request (priv->icon, INCOMING_EVENT_WINDOW_ICON, INCOMING_EVENT_WINDOW_ICON); */
  gtk_size_group_add_widget (icon_size_group, priv->icon);

  /* fill box for the left empty space in the message row */
  fbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (fbox);
  gtk_size_group_add_widget (icon_size_group, fbox);

  priv->title = gtk_label_new (NULL);
  gtk_widget_set_name (GTK_WIDGET (priv->title), "HDIncomingEventWindow-Label");
  gtk_widget_show (priv->title);
  gtk_misc_set_alignment (GTK_MISC (priv->title), 0.0, 0.5);

  priv->time_label = gtk_label_new (NULL);
  gtk_widget_set_name (GTK_WIDGET (priv->time_label), "HDIncomingEventWindow-Label");
  gtk_widget_show (priv->time_label);

  /* fill box for the close button in the title row */
  priv->cbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (priv->cbox);
  gtk_widget_set_size_request (priv->cbox, INCOMING_EVENT_WINDOW_CLOSE, -1);

  priv->message = gtk_label_new (NULL);
  gtk_widget_set_name (GTK_WIDGET (priv->message), "HDIncomingEventWindow-Label");
  gtk_widget_show (priv->message);
  gtk_misc_set_alignment (GTK_MISC (priv->message), 0.0, 0.5);

  hsep = gtk_hseparator_new ();
/*  gtk_widget_show (hsep);*/

  /* Pack containers */
  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hsep, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), message_box, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), priv->icon, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), title_box, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (title_box), priv->title, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (title_box), priv->time_label, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (title_box), priv->cbox, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (message_box), fbox, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (message_box), priv->message, TRUE, TRUE, 0);

  /* Enable handling of button press events */
  gtk_widget_add_events (GTK_WIDGET (window), GDK_BUTTON_PRESS_MASK);

  /* Don't take focus away from the toplevel application. */
  gtk_window_set_accept_focus (GTK_WINDOW (window), FALSE);

  /* bg image */
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
                         "time", (gint64) time,
                         "icon", icon,
                         NULL);

  return window;
}

