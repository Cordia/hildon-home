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

#ifndef __HD_BOOKMARK_SHORTCUT_H__
#define __HD_BOOKMARK_SHORTCUT_H__

#include <libhildondesktop/libhildondesktop.h>

G_BEGIN_DECLS

#define HD_TYPE_BOOKMARK_SHORTCUT            (hd_bookmark_shortcut_get_type ())
#define HD_BOOKMARK_SHORTCUT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_BOOKMARK_SHORTCUT, HDBookmarkShortcut))
#define HD_BOOKMARK_SHORTCUT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_BOOKMARK_SHORTCUT, HDBookmarkShortcutClass))
#define HD_IS_BOOKMARK_SHORTCUT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_BOOKMARK_SHORTCUT))
#define HD_IS_BOOKMARK_SHORTCUT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_BOOKMARK_SHORTCUT))
#define HD_BOOKMARK_SHORTCUT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_BOOKMARK_SHORTCUT, HDBookmarkShortcutClass))

typedef struct _HDBookmarkShortcut        HDBookmarkShortcut;
typedef struct _HDBookmarkShortcutClass   HDBookmarkShortcutClass;
typedef struct _HDBookmarkShortcutPrivate HDBookmarkShortcutPrivate;

struct _HDBookmarkShortcut
{
  HDHomePluginItem           parent;

  HDBookmarkShortcutPrivate *priv;
};

struct _HDBookmarkShortcutClass
{
  HDHomePluginItemClass      parent;
};

GType      hd_bookmark_shortcut_get_type (void);

G_END_DECLS

#endif
