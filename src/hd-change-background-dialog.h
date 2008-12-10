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

#ifndef __HD_CHANGE_BACKGROUND_DIALOG_H__
#define __HD_CHANGE_BACKGROUND_DIALOG_H__

#include <hildon/hildon.h>

#include <dbus/dbus-glib.h>

G_BEGIN_DECLS

#define HD_TYPE_CHANGE_BACKGROUND_DIALOG             (hd_change_background_dialog_get_type ())
#define HD_CHANGE_BACKGROUND_DIALOG(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_CHANGE_BACKGROUND_DIALOG, HDChangeBackgroundDialog))
#define HD_CHANGE_BACKGROUND_DIALOG_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_CHANGE_BACKGROUND_DIALOG, HDChangeBackgroundDialogClass))
#define HD_IS_CHANGE_BACKGROUND_DIALOG(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_CHANGE_BACKGROUND_DIALOG))
#define HD_IS_CHANGE_BACKGROUND_DIALOG_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_CHANGE_BACKGROUND_DIALOG))
#define HD_CHANGE_BACKGROUND_DIALOG_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_CHANGE_BACKGROUND_DIALOG, HDChangeBackgroundDialogClass))

typedef struct _HDChangeBackgroundDialog        HDChangeBackgroundDialog;
typedef struct _HDChangeBackgroundDialogClass   HDChangeBackgroundDialogClass;
typedef struct _HDChangeBackgroundDialogPrivate HDChangeBackgroundDialogPrivate;

/** HDChangeBackgroundDialog:
 *
 * A picker dialog for tasks
 */
struct _HDChangeBackgroundDialog
{
  GtkDialog parent;

  HDChangeBackgroundDialogPrivate *priv;
};

struct _HDChangeBackgroundDialogClass
{
  GtkDialogClass parent;
};

GType      hd_change_background_dialog_get_type (void);

GtkWidget *hd_change_background_dialog_new      (DBusGProxy *proxy);

G_END_DECLS

#endif

