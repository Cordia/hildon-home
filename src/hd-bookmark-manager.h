/*
 * This file is part of hildon-desktop
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

#ifndef __HD_BOOKMARK_MANAGER_H__
#define __HD_BOOKMARK_MANAGER_H__

#include <glib.h>
#include <glib-object.h>

#include <libhildondesktop/libhildondesktop.h>

G_BEGIN_DECLS

#define HD_TYPE_BOOKMARK_MANAGER            (hd_bookmark_manager_get_type ())
#define HD_BOOKMARK_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_BOOKMARK_MANAGER, HDBookmarkManager))
#define HD_BOOKMARK_MANAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  HD_TYPE_BOOKMARK_MANAGER, HDBookmarkManagerClass))
#define HD_IS_BOOKMARK_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_BOOKMARK_MANAGER))
#define HD_IS_BOOKMARK_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  HD_TYPE_BOOKMARK_MANAGER))
#define HD_BOOKMARK_MANAGER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  HD_TYPE_BOOKMARK_MANAGER, HDBookmarkManagerClass))

typedef struct _HDBookmarkManager        HDBookmarkManager;
typedef struct _HDBookmarkManagerClass   HDBookmarkManagerClass;
typedef struct _HDBookmarkManagerPrivate HDBookmarkManagerPrivate;

struct _HDBookmarkManager 
{
  GObject gobject;

  HDBookmarkManagerPrivate *priv;
};

struct _HDBookmarkManagerClass 
{
  GObjectClass parent_class;
};

GType              hd_bookmark_manager_get_type         (void);

HDBookmarkManager *hd_bookmark_manager_get              (void);

GtkTreeModel      *hd_bookmark_manager_get_model        (HDBookmarkManager *manager);

void               hd_bookmark_manager_install_bookmark (HDBookmarkManager *manager,
                                                         GtkTreeIter       *iter);

G_END_DECLS

#endif /* __HD_BOOKMARK_MANAGER_H__ */
