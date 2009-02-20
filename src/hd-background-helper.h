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

#ifndef __HD_BACKGROUND_HELPER_H__
#define __HD_BACKGROUND_HELPER_H__

#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

void       hd_background_helper_read_pixbuf_async  (GFile                *file,
                                                    int                   io_priority,
                                                    GCancellable         *cancellable,
                                                    GAsyncReadyCallback   callback,
                                                    gpointer              user_data);
GdkPixbuf *hd_background_helper_read_pixbuf_finish (GFile                *file,
                                                    GAsyncResult         *res,
                                                    GError              **error);

void       hd_background_helper_save_pixbuf_async  (GFile                *file,
                                                    GdkPixbuf            *pixbuf,
                                                    int                   io_priority,
                                                    GCancellable         *cancellable,
                                                    GAsyncReadyCallback   callback,
                                                    gpointer              user_data);
gboolean   hd_background_helper_save_pixbuf_finish (GFile                *file,
                                                    GAsyncResult         *res,
                                                    GError              **error);

void       hd_background_helper_check_cache_async  (GFile                *file,
                                                    guint                 view,
                                                    int                   io_priority,
                                                    GCancellable         *cancellable,
                                                    GAsyncReadyCallback   callback,
                                                    gpointer              user_data);
gboolean   hd_background_helper_check_cache_finish (GFile                *file,
                                                    GAsyncResult         *res,
                                                    GError              **error);

G_END_DECLS

#endif
