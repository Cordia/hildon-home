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

#ifndef __HD_SHORTCUTS_H__
#define __HD_SHORTCUTS_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define HD_TYPE_SHORTCUTS            (hd_shortcuts_get_type ())
#define HD_SHORTCUTS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_SHORTCUTS, HDShortcuts))
#define HD_SHORTCUTS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_SHORTCUTS, HDShortcutsClass))
#define HD_IS_SHORTCUTS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_SHORTCUTS))
#define HD_IS_SHORTCUTS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_SHORTCUTS))
#define HD_SHORTCUTS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_SHORTCUTS, HDShortcutsClass))

typedef struct _HDShortcuts        HDShortcuts;
typedef struct _HDShortcutsClass   HDShortcutsClass;
typedef struct _HDShortcutsPrivate HDShortcutsPrivate;

struct _HDShortcuts
{
  GObject                       parent;

  HDShortcutsPrivate *priv;
};

struct _HDShortcutsClass
{
  GObjectClass parent;
};

GType                  hd_shortcuts_get_type (void);

HDShortcuts *hd_shortcuts_get      (void);

G_END_DECLS

#endif
