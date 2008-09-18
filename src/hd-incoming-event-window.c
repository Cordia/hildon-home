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

#include <string.h>

#include <gdk/gdkx.h>

#include <X11/X.h>
#include <X11/Xatom.h>

#include "hd-incoming-event-window.h"

#define INCOMING_EVENT_WINDOW_WIDTH 342
#define INCOMING_EVENT_WINDOW_HEIGHT 80

#define INCOMING_EVENT_WINDOW_CLOSE  43
#define INCOMING_EVENT_WINDOW_ICON  24

#define MARGIN_DEFAULT 8
#define MARGIN_HALF 4

#define HD_INCOMING_EVENT_WINDOW_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_INCOMING_EVENT_WINDOW, HDIncomingEventWindowPrivate))

enum
{
  PROP_0,
  PROP_PREVIEW,
  PROP_ICON,
  PROP_TITLE,
  PROP_TIME,
  PROP_MESSAGE
};

struct _HDIncomingEventWindowPrivate
{
  gboolean preview;

  GtkWidget *icon;
  GtkWidget *title;
  GtkWidget *time;
  GtkWidget *cbox;
  GtkWidget *message;
};

G_DEFINE_TYPE (HDIncomingEventWindow, hd_incoming_event_window, GTK_TYPE_WINDOW);

static void
hd_incoming_event_window_realize (GtkWidget *widget)
{
  HDIncomingEventWindowPrivate *priv = HD_INCOMING_EVENT_WINDOW (widget)->priv;
  GdkDisplay *display;
  Atom atom;
  const gchar *notification_type;

  GTK_WIDGET_CLASS (hd_incoming_event_window_parent_class)->realize (widget);

  /* Notification window */
  gdk_window_set_type_hint (widget->window, GDK_WINDOW_TYPE_HINT_NOTIFICATION);

  /* Set the _NET_WM_WINDOW_TYPE property to _HILDON_WM_WINDOW_TYPE_HOME_APPLET */
  display = gdk_drawable_get_display (widget->window);
  atom = gdk_x11_get_xatom_by_name_for_display (display,
                                                "_HILDON_NOTIFICATION_TYPE");

  if (priv->preview)
    notification_type = "_HILDON_NOTIFICATION_TYPE_PREVIEW";
  else
    notification_type = "_HILDON_NOTIFICATION_TYPE_INCOMING_EVENT";

  XChangeProperty (GDK_WINDOW_XDISPLAY (widget->window),
                   GDK_WINDOW_XID (widget->window),
                   atom, XA_STRING, 8, PropModeReplace,
                   (guchar *) notification_type,
                   strlen (notification_type));
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
  /* HDIncomingEventWindowPrivate *priv = HD_INCOMING_EVENT_WINDOW (object)->priv; */

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

    case PROP_ICON:
      g_value_set_object (value, gtk_image_get_pixbuf (GTK_IMAGE (priv->icon)));
      break;

    case PROP_TITLE:
      g_value_set_string (value, gtk_label_get_label (GTK_LABEL (priv->title)));
      break;

    case PROP_TIME:
      g_value_set_string (value, gtk_label_get_label (GTK_LABEL (priv->time)));
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
      priv->preview = g_value_get_boolean (value);
      /* Close button is not shown in preview windows */
      if (priv->preview)
        gtk_widget_hide (priv->cbox);
      else
        gtk_widget_show (priv->cbox);
      break;

    case PROP_ICON:
      gtk_image_set_from_pixbuf (GTK_IMAGE (priv->icon), g_value_get_object (value));
      break;

    case PROP_TITLE:
      gtk_label_set_label (GTK_LABEL (priv->title), g_value_get_string (value));
      break;

    case PROP_TIME:
      gtk_label_set_label (GTK_LABEL (priv->time), g_value_get_string (value));
      break;

    case PROP_MESSAGE:
      gtk_label_set_label (GTK_LABEL (priv->message), g_value_get_string (value));
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

  widget_class->realize = hd_incoming_event_window_realize;
  widget_class->size_request = hd_incoming_event_window_size_request;

  object_class->dispose = hd_incoming_event_window_dispose;
  object_class->finalize = hd_incoming_event_window_finalize;
  object_class->get_property = hd_incoming_event_window_get_property;
  object_class->set_property = hd_incoming_event_window_set_property;

  g_object_class_install_property (object_class,
                                   PROP_PREVIEW,
                                   g_param_spec_boolean ("preview",
                                                         "Preview",
                                                         "If the window is a preview window",
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class,
                                   PROP_ICON,
                                   g_param_spec_object ("icon",
                                                        "Icon",
                                                        "The icon of the incoming event",
                                                        GDK_TYPE_PIXBUF,
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
                                   g_param_spec_string ("time",
                                                        "Time",
                                                        "The time of the incoming event",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_MESSAGE,
                                   g_param_spec_string ("message",
                                                        "Message",
                                                        "The message of the incoming event",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  g_type_class_add_private (klass, sizeof (HDIncomingEventWindowPrivate));
}

static void
hd_incoming_event_window_init (HDIncomingEventWindow *window)
{
  HDIncomingEventWindowPrivate *priv = HD_INCOMING_EVENT_WINDOW_GET_PRIVATE (window);
  GtkWidget *vbox, *hbox, *title_box, *message_box, *fbox, *hsep;
  GtkSizeGroup *icon_size_group;

  window->priv = priv;

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox);

  hbox = gtk_hbox_new (FALSE, MARGIN_DEFAULT);
  gtk_widget_show (hbox);

  title_box = gtk_hbox_new (FALSE, MARGIN_HALF);
  gtk_widget_show (title_box);

  message_box = gtk_hbox_new (FALSE, MARGIN_DEFAULT);
  gtk_widget_show (message_box);

  icon_size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  priv->icon = gtk_image_new ();
  gtk_widget_show (priv->icon);
  gtk_widget_set_size_request (priv->icon, INCOMING_EVENT_WINDOW_ICON, INCOMING_EVENT_WINDOW_ICON);
  gtk_size_group_add_widget (icon_size_group, priv->icon);

  /* fill box for the left empty space in the message row */
  fbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (fbox);
  gtk_size_group_add_widget (icon_size_group, fbox);

  priv->title = gtk_label_new (NULL);
  gtk_widget_show (priv->title);
  gtk_misc_set_alignment (GTK_MISC (priv->title), 0.0, 0.5);

  priv->time = gtk_label_new (NULL);
  gtk_widget_show (priv->time);

  /* fill box for the close button in the title row */
  priv->cbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (priv->cbox);
  gtk_widget_set_size_request (priv->cbox, INCOMING_EVENT_WINDOW_CLOSE, -1);

  priv->message = gtk_label_new (NULL);
  gtk_widget_show (priv->message);
  gtk_misc_set_alignment (GTK_MISC (priv->message), 0.0, 0.5);

  hsep = gtk_hseparator_new ();
  gtk_widget_show (hsep);

  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hsep, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), message_box, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), priv->icon, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), title_box, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (title_box), priv->title, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (title_box), priv->time, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (title_box), priv->cbox, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (message_box), fbox, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (message_box), priv->message, TRUE, TRUE, 0);
}
