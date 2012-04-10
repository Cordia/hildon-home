/*
 * This file is part of hildon-home
 *
 * Copyright (C) 2009 Nokia Corporation.
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

#ifndef __HD_BACKGROUNDS_H__
#define __HD_BACKGROUNDS_H__

#include <gdk/gdk.h>
#include <gio/gio.h>

#include "hd-command-thread-pool.h"

G_BEGIN_DECLS

#define HD_TYPE_BACKGROUNDS            (hd_backgrounds_get_type ())
#define HD_BACKGROUNDS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_BACKGROUNDS, HDBackgrounds))
#define HD_BACKGROUNDS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  HD_TYPE_BACKGROUNDS, HDBackgroundsClass))
#define HD_IS_BACKGROUNDS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_BACKGROUNDS))
#define HD_IS_BACKGROUNDS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  HD_TYPE_BACKGROUNDS))
#define HD_BACKGROUNDS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  HD_TYPE_BACKGROUNDS, HDBackgroundsClass))

typedef struct _HDBackgrounds        HDBackgrounds;
typedef struct _HDBackgroundsClass   HDBackgroundsClass;
typedef struct _HDBackgroundsPrivate HDBackgroundsPrivate;

struct _HDBackgrounds 
{
  GObject gobject;

  HDBackgroundsPrivate *priv;
};

struct _HDBackgroundsClass 
{
  GObjectClass parent_class;
};

GType          hd_backgrounds_get_type        (void);

HDBackgrounds *hd_backgrounds_get             (void);

void           hd_backgrounds_startup         (HDBackgrounds  *backgrounds);

void           hd_backgrounds_add_done_cb     (HDBackgrounds  *backgrounds,
                                               GSourceFunc     done_callback,
                                               gpointer        cb_data,
                                               GDestroyNotify  destroy_data);

void           hd_backgrounds_add_create_cached_image (HDBackgrounds      *backgrounds,
                                                       GFile              *source_file,
                                                       gboolean            error_dialogs,
                                                       GCancellable       *cancellable,
                                                       HDCommandCallback   command,
                                                       gpointer            data,
                                                       GDestroyNotify      destroy_data);

void           hd_backgrounds_add_update_current_files (HDBackgrounds  *backgrounds,
                                                        GFile         **files,
                                                        GCancellable   *cancellable);

GFile         *hd_backgrounds_get_background  (HDBackgrounds  *backgrounds,
                                               guint           view);
void           hd_backgrounds_set_current_background (HDBackgrounds *backgrounds,
                                                      const char    *uri);

/* The following functions can be called from the command callback */
gboolean       hd_backgrounds_save_cached_image (HDBackgrounds  *backgrounds,
                                                 GdkPixbuf      *pixbuf,
                                                 guint           view,
                                                 GFile          *source_file,
                                                 const char     *source_etag,
                                                 gboolean        error_dialogs,
                                                 gboolean        update_gconf,
                                                 GCancellable   *cancellable,
                                                 GError        **error);
void hd_backgrounds_report_corrupt_image        (const GError   *error);

gboolean hd_backgrounds_is_portrait_wallpaper_enabled (HDBackgrounds *backgrounds);
void hd_backgrounds_set_portrait (HDBackgrounds *backgrounds, gboolean is_portrait);

G_END_DECLS

#endif /* __HD_BACKGROUNDS_H__ */
