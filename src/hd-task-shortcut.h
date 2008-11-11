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

#ifndef __HD_TASK_SHORTCUT_H__
#define __HD_TASK_SHORTCUT_H__

#include <libhildondesktop/libhildondesktop.h>

G_BEGIN_DECLS

#define HD_TYPE_TASK_SHORTCUT            (hd_task_shortcut_get_type ())
#define HD_TASK_SHORTCUT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_TASK_SHORTCUT, HDTaskShortcut))
#define HD_TASK_SHORTCUT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_TASK_SHORTCUT, HDTaskShortcutClass))
#define HD_IS_TASK_SHORTCUT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_TASK_SHORTCUT))
#define HD_IS_TASK_SHORTCUT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_TASK_SHORTCUT))
#define HD_TASK_SHORTCUT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_TASK_SHORTCUT, HDTaskShortcutClass))

typedef struct _HDTaskShortcut        HDTaskShortcut;
typedef struct _HDTaskShortcutClass   HDTaskShortcutClass;
typedef struct _HDTaskShortcutPrivate HDTaskShortcutPrivate;

struct _HDTaskShortcut
{
  HDHomePluginItem parent;

  HDTaskShortcutPrivate *priv;
};

struct _HDTaskShortcutClass
{
  HDHomePluginItemClass parent;
};

GType hd_task_shortcut_get_type (void);

G_END_DECLS

#endif
