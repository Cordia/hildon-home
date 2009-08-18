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

#ifndef __HD_INCOMING_EVENTS_H__
#define __HD_INCOMING_EVENTS_H__

#include <glib-object.h>
#include <libhildondesktop/libhildondesktop.h>

G_BEGIN_DECLS

#define HD_TYPE_INCOMING_EVENTS            (hd_incoming_events_get_type ())
#define HD_INCOMING_EVENTS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_INCOMING_EVENTS, HDIncomingEvents))
#define HD_INCOMING_EVENTS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_INCOMING_EVENTS, HDIncomingEventsClass))
#define HD_IS_INCOMING_EVENTS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_INCOMING_EVENTS))
#define HD_IS_INCOMING_EVENTS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_INCOMING_EVENTS))
#define HD_INCOMING_EVENTS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_INCOMING_EVENTS, HDIncomingEventsClass))

typedef struct _HDIncomingEvents        HDIncomingEvents;
typedef struct _HDIncomingEventsClass   HDIncomingEventsClass;
typedef struct _HDIncomingEventsPrivate HDIncomingEventsPrivate;

struct _HDIncomingEvents
{
  GObject                       parent;

  HDIncomingEventsPrivate *priv;
};

struct _HDIncomingEventsClass
{
  GObjectClass parent;
};

GType             hd_incoming_events_get_type (void);

HDIncomingEvents *hd_incoming_events_get      (void);

gboolean          hd_incoming_events_get_display_on (void);

G_END_DECLS

#endif
