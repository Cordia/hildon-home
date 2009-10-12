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

#ifndef __HD_INSTALL_WIDGETS_DIALOG_H__
#define __HD_INSTALL_WIDGETS_DIALOG_H__

#include <hildon/hildon.h>
#include "hd-widgets.h"

G_BEGIN_DECLS

#define HD_TYPE_INSTALL_WIDGETS_DIALOG             (hd_install_widgets_dialog_get_type ())
#define HD_INSTALL_WIDGETS_DIALOG(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_INSTALL_WIDGETS_DIALOG, HDInstallWidgetsDialog))
#define HD_INSTALL_WIDGETS_DIALOG_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_INSTALL_WIDGETS_DIALOG, HDInstallWidgetsDialogClass))
#define HD_IS_INSTALL_WIDGETS_DIALOG(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_INSTALL_WIDGETS_DIALOG))
#define HD_IS_INSTALL_WIDGETS_DIALOG_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_INSTALL_WIDGETS_DIALOG))
#define HD_INSTALL_WIDGETS_DIALOG_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_INSTALL_WIDGETS_DIALOG, HDInstallWidgetsDialogClass))

typedef struct _HDInstallWidgetsDialog        HDInstallWidgetsDialog;
typedef struct _HDInstallWidgetsDialogClass   HDInstallWidgetsDialogClass;
typedef struct _HDInstallWidgetsDialogPrivate HDInstallWidgetsDialogPrivate;

/** HDInstallWidgetsDialog:
 *
 * A picker dialog for bookmarks
 */
struct _HDInstallWidgetsDialog
{
  HildonPickerDialog parent;

  HDInstallWidgetsDialogPrivate *priv;
};

struct _HDInstallWidgetsDialogClass
{
  HildonPickerDialogClass parent;
};

GType      hd_install_widgets_dialog_get_type (void);

GtkWidget *hd_install_widgets_dialog_new      (HDWidgets *widgets);

G_END_DECLS

#endif
