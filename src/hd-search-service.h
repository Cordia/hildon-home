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

#ifndef __HD_SEARCH_SERVICE_H__
#define __HD_SEARCH_SERVICE_H__

#include <gio/gio.h>

G_BEGIN_DECLS

#define HD_TYPE_SEARCH_SERVICE            (hd_search_service_get_type ())
#define HD_SEARCH_SERVICE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_SEARCH_SERVICE, HDSearchService))
#define HD_SEARCH_SERVICE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_SEARCH_SERVICE, HDSearchServiceClass))
#define HD_IS_SEARCH_SERVICE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_SEARCH_SERVICE))
#define HD_IS_SEARCH_SERVICE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_SEARCH_SERVICE))
#define HD_SEARCH_SERVICE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_SEARCH_SERVICE, HDSearchServiceClass))

typedef struct _HDSearchService        HDSearchService;
typedef struct _HDSearchServiceClass   HDSearchServiceClass;
typedef struct _HDSearchServicePrivate HDSearchServicePrivate;

struct _HDSearchService
{
  GInitiallyUnowned parent;

  HDSearchServicePrivate *priv;
};

struct _HDSearchServiceClass
{
  GInitiallyUnownedClass parent;
};

GType            hd_search_service_get_type     (void);

HDSearchService *hd_search_service_new          (void);

void             hd_search_service_query_async  (HDSearchService     *service,
                                                 const char          *query_service,
                                                 const char          *query,
                                                 GCancellable        *cancellable,
                                                 GAsyncReadyCallback  callback,
                                                 gpointer             user_data);
GStrv            hd_search_service_query_finish (HDSearchService     *service,
                                                 GAsyncResult        *result,
                                                 GError             **error);

G_END_DECLS

#endif

