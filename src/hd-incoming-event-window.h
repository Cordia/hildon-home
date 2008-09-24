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

#ifndef __HD_INCOMING_EVENT_WINDOW_H__
#define __HD_INCOMING_EVENT_WINDOW_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define HD_TYPE_INCOMING_EVENT_WINDOW             (hd_incoming_event_window_get_type ())
#define HD_INCOMING_EVENT_WINDOW(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_INCOMING_EVENT_WINDOW, HDIncomingEventWindow))
#define HD_INCOMING_EVENT_WINDOW_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_INCOMING_EVENT_WINDOW, HDIncomingEventWindowClass))
#define HD_IS_INCOMING_EVENT_WINDOW(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_INCOMING_EVENT_WINDOW))
#define HD_IS_INCOMING_EVENT_WINDOW_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_INCOMING_EVENT_WINDOW))
#define HD_INCOMING_EVENT_WINDOW_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_INCOMING_EVENT_WINDOW, HDIncomingEventWindowClass))

typedef struct _HDIncomingEventWindow        HDIncomingEventWindow;
typedef struct _HDIncomingEventWindowClass   HDIncomingEventWindowClass;
typedef struct _HDIncomingEventWindowPrivate HDIncomingEventWindowPrivate;

/** HDIncomingEventWindow:
 *
 * A base class for other items.
 */
struct _HDIncomingEventWindow
{
  GtkWindow parent;

  HDIncomingEventWindowPrivate *priv;
};

struct _HDIncomingEventWindowClass
{
  GtkWindowClass parent;

  void (*response) (HDIncomingEventWindow *window,
                    gint                   response_id);
};

GType      hd_incoming_event_window_get_type (void);

GtkWidget *hd_incoming_event_window_new      (gboolean     preview,
                                              const gchar *summary,
                                              const gchar *body,
                                              time_t       time,
                                              const gchar *icon);

G_END_DECLS

#endif

