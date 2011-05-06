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

#define HD_TASK_IMAGES_DIR                   "/etc/hildon/theme/images/"
#define HD_TASK_BACKGROUND_IMAGE_FILE        HD_TASK_IMAGES_DIR "ApplicationShortcutApplet.png"
#define HD_TASK_BACKGROUND_ACTIVE_IMAGE_FILE HD_TASK_IMAGES_DIR "ApplicationShortcutAppletPressed.png"

#define HD_TASK_SCALED_BACKGROUND_IMAGE_FILE        HD_TASK_IMAGES_DIR "ApplicationShortcutAppletScaled.png"
#define HD_TASK_SCALED_BACKGROUND_ACTIVE_IMAGE_FILE HD_TASK_IMAGES_DIR "ApplicationShortcutAppletPressedScaled.png"

typedef struct _HDTaskShortcut        HDTaskShortcut;
typedef struct _HDTaskShortcutClass   HDTaskShortcutClass;
typedef struct _HDTaskShortcutPrivate HDTaskShortcutPrivate;

/* Default layout size taken from Task navigation layout guide 1.2 */
#define HD_TASK_DEF_SHORTCUT_WIDTH 96
#define HD_TASK_DEF_SHORTCUT_HEIGHT 96
#define HD_TASK_DEF_ICON_WIDTH 64
#define HD_TASK_DEF_ICON_HEIGHT 64

/* Scaling to comply with task navigation layout guide 1.2 */

#define HD_TASK_SHORTCUT_WIDTH task_shortcut_width
#define HD_TASK_SHORTCUT_HEIGHT HD_TASK_SHORTCUT_WIDTH
#define HD_TASK_ICON_WIDTH (SHORTCUT_WIDTH*2/3)
#define HD_TASK_ICON_HEIGHT (SHORTCUT_HEIGHT*2/3)

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

extern HDShortcuts *hd_shortcuts_task_shortcuts;

G_END_DECLS

#endif
