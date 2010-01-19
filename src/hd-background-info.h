/*
 * This file is part of hildon-home
 *
 * Copyright (C) 2010 Nokia Corporation.
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

#ifndef __HD_BACKGROUND_INFO_H__
#define __HD_BACKGROUND_INFO_H__

#include <gio/gio.h>

G_BEGIN_DECLS

#define HD_TYPE_BACKGROUND_INFO            (hd_background_info_get_type ())
#define HD_BACKGROUND_INFO(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_BACKGROUND_INFO, HDBackgroundInfo))
#define HD_BACKGROUND_INFO_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_BACKGROUND_INFO, HDBackgroundInfoClass))
#define HD_IS_BACKGROUND_INFO(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_BACKGROUND_INFO))
#define HD_IS_BACKGROUND_INFO_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_BACKGROUND_INFO))
#define HD_BACKGROUND_INFO_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_BACKGROUND_INFO, HDBackgroundInfoClass))

typedef struct _HDBackgroundInfo        HDBackgroundInfo;
typedef struct _HDBackgroundInfoClass   HDBackgroundInfoClass;
typedef struct _HDBackgroundInfoPrivate HDBackgroundInfoPrivate;

struct _HDBackgroundInfo
{
  GObject parent;

  HDBackgroundInfoPrivate *priv;
};

struct _HDBackgroundInfoClass
{
  GObjectClass parent;
};

GType             hd_background_info_get_type (void);

HDBackgroundInfo *hd_background_info_new      (void);

void              hd_background_info_init_async  (HDBackgroundInfo     *info,
                                                  GCancellable         *cancellable,
                                                  GAsyncReadyCallback   callback,
                                                  gpointer              user_data);
gboolean          hd_background_info_init_finish (HDBackgroundInfo     *info,
                                                  GAsyncResult         *result,
                                                  GError              **error);

GFile            *hd_background_info_get_file (HDBackgroundInfo *info,
                                               guint             desktop);
const char       *hd_background_info_get_etag (HDBackgroundInfo *info,
                                               guint             desktop);

void              hd_background_info_set      (HDBackgroundInfo *info,
                                               guint             desktop,
                                               GFile            *file,
                                               const char       *etag);



G_END_DECLS

#endif

