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

#define HD_BOOKMARK_IMAGES_DIR                   "/etc/hildon/theme/images/"
#define HD_BOOKMARK_BACKGROUND_IMAGE_FILE        HD_BOOKMARK_IMAGES_DIR "WebShortcutAppletBackground.png"
#define HD_BOOKMARK_BACKGROUND_ACTIVE_IMAGE_FILE HD_BOOKMARK_IMAGES_DIR "WebShortcutAppletBackgroundActive.png"
#define HD_BOOKMARK_THUMBNAIL_MASK_FILE          HD_BOOKMARK_IMAGES_DIR "WebShortCutAppletThumbnailMask.png"

#define HD_BOOKMARK_SCALED_BACKGROUND_IMAGE_FILE        HD_BOOKMARK_IMAGES_DIR "WebShortcutAppletBackgroundScaled.png"
#define HD_BOOKMARK_SCALED_BACKGROUND_ACTIVE_IMAGE_FILE HD_BOOKMARK_IMAGES_DIR "WebShortcutAppletBackgroundActiveScaled.png"
#define HD_BOOKMARK_SCALED_THUMBNAIL_MASK_FILE          HD_BOOKMARK_IMAGES_DIR "WebShortCutAppletThumbnailMaskScaled.png"

/* Default size from Home layout guide 1.2 */
#define HD_BOOKMARK_DEF_SHORTCUT_WIDTH 176
#define HD_BOOKMARK_DEF_SHORTCUT_HEIGHT 146

#define HD_BOOKMARK_DEF_THUMBNAIL_WIDTH 160.0
#define HD_BOOKMARK_DEF_THUMBNAIL_HEIGHT 96.0

#define HD_BOOKMARK_DEF_BORDER_WIDTH_LEFT 8
#define HD_BOOKMARK_DEF_BORDER_WIDTH_TOP 8

/* Scaling to comply with Home layout guide 1.2 */
#define HD_BOOKMARK_SHORTCUT_WIDTH task_bookmarks_width
#define HD_BOOKMARK_SHORTCUT_HEIGHT (task_bookmarks_width / 1.2 )
#define HD_BOOKMARK_HIDE_BG task_bookmarks_hide_bg

#define HD_BOOKMARK_THUMBNAIL_WIDTH ( HD_BOOKMARK_SHORTCUT_WIDTH / 1.1 )
#define HD_BOOKMARK_THUMBNAIL_HEIGHT ( HD_BOOKMARK_THUMBNAIL_WIDTH / 1.65 )

#define HD_BOOKMARK_BORDER_WIDTH_LEFT ( HD_BOOKMARK_SHORTCUT_WIDTH / 21 )
#define HD_BOOKMARK_BORDER_WIDTH_TOP ( HD_BOOKMARK_SHORTCUT_WIDTH / 21 )

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
