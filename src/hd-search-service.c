/*
 * This file is part of hildon-desktop
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "hd-dbus-utils.h"

#include "hd-search-service.h"

#define TYPE_STRV_ARRAY (dbus_g_type_get_collection ("GPtrArray", G_TYPE_STRV))

#define TRACKER_SERVICE               "org.freedesktop.Tracker"
#define TRACKER_SEARCH_PATH           "/org/freedesktop/Tracker/Search"
#define TRACKER_SEARCH_INTERFACE      "org.freedesktop.Tracker.Search"

#define TRACKER_QUERY_METHOD          "Query"

#define HD_SEARCH_SERVICE_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_SEARCH_SERVICE, HDSearchServicePrivate))

struct _HDSearchServicePrivate
{
  DBusGConnection *session_dbus;
  DBusGProxy *tracker_proxy;
};

static void hd_search_service_dispose     (GObject *object);

static void init_tracker_proxy (HDSearchService *search_service);
static void query_cb (DBusGProxy         *proxy,
                      DBusGProxyCall     *call,
                      GSimpleAsyncResult *result);

G_DEFINE_TYPE (HDSearchService, hd_search_service, G_TYPE_INITIALLY_UNOWNED);

HDSearchService *
hd_search_service_new (void)
{
  HDSearchService *search_service;

  search_service = g_object_new (HD_TYPE_SEARCH_SERVICE,
                                 NULL);

  return search_service;
}

static void
hd_search_service_class_init (HDSearchServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = hd_search_service_dispose;

  g_type_class_add_private (klass, sizeof (HDSearchServicePrivate));
}

static void
hd_search_service_init (HDSearchService *search_service)
{
  HDSearchServicePrivate *priv;

  priv = search_service->priv = HD_SEARCH_SERVICE_GET_PRIVATE (search_service);

  init_tracker_proxy (search_service);
}

static void
init_tracker_proxy (HDSearchService *search_service)
{
  HDSearchServicePrivate *priv = search_service->priv;

  priv->session_dbus = hd_get_session_dbus_connection ();

  if (priv->session_dbus)
    {
      priv->tracker_proxy = dbus_g_proxy_new_for_name (priv->session_dbus,
                                                       TRACKER_SERVICE,
                                                       TRACKER_SEARCH_PATH,
                                                       TRACKER_SEARCH_INTERFACE);
    }
}

static void
hd_search_service_dispose (GObject *object)
{
  HDSearchService *search_service = HD_SEARCH_SERVICE (object);
  HDSearchServicePrivate *priv = search_service->priv;

  if (priv->tracker_proxy)
    priv->tracker_proxy = (g_object_unref (priv->tracker_proxy), NULL);

  if (priv->session_dbus)
    priv->session_dbus = (dbus_g_connection_unref (priv->session_dbus), NULL);

  G_OBJECT_CLASS (hd_search_service_parent_class)->dispose (object);
}

void
hd_search_service_query_async (HDSearchService     *service,
                               const char          *query_service,
                               const char          *query,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  HDSearchServicePrivate *priv;
  GSimpleAsyncResult *result;

  g_return_if_fail (HD_IS_SEARCH_SERVICE (service));

  priv = service->priv;

  result = g_simple_async_result_new (G_OBJECT (service),
                                      callback,
                                      user_data,
                                      hd_search_service_query_async);

  dbus_g_proxy_begin_call (priv->tracker_proxy,
                           TRACKER_QUERY_METHOD,
                           (DBusGProxyCallNotify) query_cb,
                           result,
                           g_object_unref,
                           G_TYPE_INT, -1,
                           G_TYPE_STRING, query_service,
                           G_TYPE_STRV, NULL,
                           G_TYPE_STRING, "",
                           G_TYPE_STRV, NULL,
                           G_TYPE_STRING, query,
                           G_TYPE_BOOLEAN, FALSE,
                           G_TYPE_STRV, NULL,
                           G_TYPE_BOOLEAN, FALSE,
                           G_TYPE_INT, 0,
                           G_TYPE_INT, 512,
                           G_TYPE_INVALID);
}

static void
query_cb (DBusGProxy         *proxy,
          DBusGProxyCall     *call,
          GSimpleAsyncResult *result)
{
  GPtrArray *strv_array;
  GError *error = NULL;

  if (dbus_g_proxy_end_call (proxy,
                             call,
                             &error,
                             TYPE_STRV_ARRAY, &strv_array,
                             G_TYPE_INVALID))
    {
      GStrv filenames;
      guint i;

      filenames = g_new0 (char*, strv_array->len + 1);

      for (i = 0; i < strv_array->len; i++)
        {
          GStrv data = g_ptr_array_index (strv_array, i);

          filenames[i] = g_strdup (data[0]);

          g_strfreev (data);
        }

      g_ptr_array_free (strv_array, TRUE);

      g_simple_async_result_set_op_res_gpointer (result,
                                                 filenames,
                                                 (GDestroyNotify) g_strfreev);
    }
  else
    {
      g_simple_async_result_set_from_error (result,
                                            error);
      g_error_free (error);
    }

  g_simple_async_result_complete (result);
}

GStrv
hd_search_service_query_finish (HDSearchService  *service,
                                GAsyncResult     *result,
                                GError          **error)
{
  GSimpleAsyncResult *simple;

  g_return_val_if_fail (HD_IS_SEARCH_SERVICE (service), NULL);
  g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (result), NULL);

  simple = G_SIMPLE_ASYNC_RESULT (result);

  if (g_simple_async_result_propagate_error (simple, error))
    return NULL;

  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) ==
                  hd_search_service_query_async);

  return g_simple_async_result_get_op_res_gpointer (simple);    
}
