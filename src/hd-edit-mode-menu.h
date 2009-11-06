/*
 * This file is part of hildon-home
 *
 * Copyright (C) 2009 Nokia Corporation.
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

#ifndef __HD_EDIT_MODE_MENU_H__
#define __HD_EDIT_MODE_MENU_H__

#include <hildon/hildon.h>

G_BEGIN_DECLS

#define HD_TYPE_EDIT_MODE_MENU             (hd_edit_mode_menu_get_type ())
#define HD_EDIT_MODE_MENU(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_EDIT_MODE_MENU, HDEditModeMenu))
#define HD_EDIT_MODE_MENU_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_EDIT_MODE_MENU, HDEditModeMenuClass))
#define HD_IS_EDIT_MODE_MENU(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_EDIT_MODE_MENU))
#define HD_IS_EDIT_MODE_MENU_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_EDIT_MODE_MENU))
#define HD_EDIT_MODE_MENU_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_EDIT_MODE_MENU, HDEditModeMenuClass))

typedef struct _HDEditModeMenu        HDEditModeMenu;
typedef struct _HDEditModeMenuClass   HDEditModeMenuClass;
typedef struct _HDEditModeMenuPrivate HDEditModeMenuPrivate;

/** HDEditModeMenu:
 *
 * A picker dialog for bookmarks
 */
struct _HDEditModeMenu
{
  HildonAppMenu parent;

  HDEditModeMenuPrivate *priv;
};

struct _HDEditModeMenuClass
{
  HildonAppMenuClass parent;
};

GType      hd_edit_mode_menu_get_type (void);

GtkWidget *hd_edit_mode_menu_new      (void);

void       hd_edit_mode_menu_set_current_view (HDEditModeMenu *menu,
                                               guint           current_view);

G_END_DECLS

#endif

