/*
 * This file is part of hildon-home
 *
 * Copyright (C) 2008, 2009 Nokia Corporation.
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

#ifndef __HD_SHORTCUT_WIDGETS_H__
#define __HD_SHORTCUT_WIDGETS_H__

#include "hd-widgets.h"

G_BEGIN_DECLS

#define HD_TYPE_SHORTCUT_WIDGETS            (hd_shortcut_widgets_get_type ())
#define HD_SHORTCUT_WIDGETS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_SHORTCUT_WIDGETS, HDShortcutWidgets))
#define HD_SHORTCUT_WIDGETS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  HD_TYPE_SHORTCUT_WIDGETS, HDShortcutWidgetsClass))
#define HD_IS_SHORTCUT_WIDGETS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_SHORTCUT_WIDGETS))
#define HD_IS_SHORTCUT_WIDGETS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  HD_TYPE_SHORTCUT_WIDGETS))
#define HD_SHORTCUT_WIDGETS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  HD_TYPE_SHORTCUT_WIDGETS, HDShortcutWidgetsClass))

typedef struct _HDShortcutWidgets        HDShortcutWidgets;
typedef struct _HDShortcutWidgetsClass   HDShortcutWidgetsClass;
typedef struct _HDShortcutWidgetsPrivate HDShortcutWidgetsPrivate;

struct _HDShortcutWidgets 
{
  HDWidgets parent;

  HDShortcutWidgetsPrivate *priv;
};

struct _HDShortcutWidgetsClass 
{
  HDWidgetsClass parent_class;

  void (*desktop_file_changed)             (HDShortcutWidgets *manager);
};

GType          hd_shortcut_widgets_get_type    (void);

HDWidgets    *hd_shortcut_widgets_get          (void);

const gchar  *hd_shortcut_widgets_get_icon     (HDShortcutWidgets *manager,
                                                const gchar       *desktop_id);
void          hd_shortcut_widgets_launch_task  (HDShortcutWidgets *manager,
                                                const gchar       *desktop_id);

G_END_DECLS

#endif /* __HD_SHORTCUT_WIDGETS_H__ */
