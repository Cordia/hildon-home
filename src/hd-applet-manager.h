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

#ifndef __HD_APPLET_MANAGER_H__
#define __HD_APPLET_MANAGER_H__

#include "hd-widgets.h"

G_BEGIN_DECLS

#define HD_TYPE_APPLET_MANAGER            (hd_applet_manager_get_type ())
#define HD_APPLET_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_APPLET_MANAGER, HDAppletManager))
#define HD_APPLET_MANAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  HD_TYPE_APPLET_MANAGER, HDAppletManagerClass))
#define HD_IS_APPLET_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_APPLET_MANAGER))
#define HD_IS_APPLET_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  HD_TYPE_APPLET_MANAGER))
#define HD_APPLET_MANAGER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  HD_TYPE_APPLET_MANAGER, HDAppletManagerClass))

typedef struct _HDAppletManager        HDAppletManager;
typedef struct _HDAppletManagerClass   HDAppletManagerClass;
typedef struct _HDAppletManagerPrivate HDAppletManagerPrivate;

struct _HDAppletManager 
{
  HDWidgets parent;

  HDAppletManagerPrivate *priv;
};

struct _HDAppletManagerClass 
{
  HDWidgetsClass parent_class;
};

GType      hd_applet_manager_get_type       (void);

HDWidgets *hd_applet_manager_get            (void);

void       hd_applet_manager_remove_applet  (HDAppletManager *manager,
                                             const gchar     *applet_id);
void       hd_applet_manager_throttled      (HDAppletManager *manager,
                                             gboolean         throttled);

G_END_DECLS

#endif /* __HD_APPLET_MANAGER_H__ */
