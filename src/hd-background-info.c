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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "hd-desktop.h"
#include "hd-object-vector.h"

#include "hd-background-info.h"
#include "hd-backgrounds.h"

/* background info keyfile */
#define BACKGROUND_INFO_GROUP "Background-Info"
#define BACKGROUND_INFO_KEY_VERSION "Version"
#define BACKGROUND_INFO_KEY_FILE_FMT "File-%u"
#define BACKGROUND_INFO_KEY_ETAG_FMT "Etag-%u"

#define HD_BACKGROUND_INFO_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_BACKGROUND_INFO, HDBackgroundInfoPrivate))

struct _HDBackgroundInfoPrivate
{
  GPtrArray *etags;
  HDObjectVector *files;
};

static void hd_background_info_dispose (GObject *object);

static inline GFile *get_background_info_file (void);
static inline char *get_background_info_path (void);
static void load_background_info_file (GFile              *file,
                                       GAsyncResult       *result,
                                       GSimpleAsyncResult *init_result);
static gboolean load_background_info_from_key_file (HDBackgroundInfo  *info,
                                                    char              *file_contents,
                                                    gsize              file_size);
static guint get_background_info_key_file_version (GKeyFile  *keyfile,
                                                   GError   **error);
static void load_backgrounds_from_key_file (HDBackgroundInfo *info,
                                            GKeyFile         *key_file);
static void load_background_from_key_file (HDBackgroundInfo *info,
                                           GKeyFile         *key_file,
                                           guint             desktop);
static GFile *get_file_from_key_file (GKeyFile *key_file,
                                      guint     i);
static char *get_etag_from_key_file (GKeyFile *key_file,
                                     guint     i);
static void load_background_info_legacy (HDBackgroundInfo *info,
                                         char             *file_contents,
                                         gsize             file_size);
static void save_background_info_file (HDBackgroundInfo *info);

G_DEFINE_TYPE (HDBackgroundInfo, hd_background_info, G_TYPE_OBJECT);

static void
hd_background_info_class_init (HDBackgroundInfoClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = hd_background_info_dispose;

  g_type_class_add_private (klass, sizeof (HDBackgroundInfoPrivate));
}

static void
hd_background_info_init (HDBackgroundInfo *info)
{
  HDBackgroundInfoPrivate *priv;

  priv = info->priv = HD_BACKGROUND_INFO_GET_PRIVATE (info);

  guint max = HD_DESKTOP_VIEWS;
  if(hd_backgrounds_is_portrait_wallpaper_enabled (hd_backgrounds_get ()))
    max += HD_DESKTOP_VIEWS;

  priv->etags = g_ptr_array_sized_new (max);
  g_ptr_array_set_size (priv->etags, max);

  priv->files = hd_object_vector_new_at_size (max, NULL);
}

HDBackgroundInfo *
hd_background_info_new (void)
{
  return g_object_new (HD_TYPE_BACKGROUND_INFO,
                       NULL);
}

void
hd_background_info_init_async (HDBackgroundInfo    *info,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  GSimpleAsyncResult *result;
  GFile *background_info_file;

  result = g_simple_async_result_new (G_OBJECT (info),
                                      callback,
                                      user_data,
                                      hd_background_info_init_async);

  background_info_file = get_background_info_file ();

  g_file_load_contents_async (background_info_file,
                              cancellable,
                              (GAsyncReadyCallback) load_background_info_file,
                              result);

  g_object_unref (background_info_file);
}

static inline GFile *
get_background_info_file (void)
{
  GFile *background_info_file;
  gchar *path;

  path = get_background_info_path ();
  background_info_file = g_file_new_for_path (path);
  g_free (path);

  return background_info_file;
}

static inline char *
get_background_info_path (void)
{
  return g_build_filename (g_get_home_dir (),
                           ".backgrounds",
                           "cache.info",
                           NULL);
}

static void
load_background_info_file (GFile              *file,
                           GAsyncResult       *result,
                           GSimpleAsyncResult *init_result)
{
  HDBackgroundInfo *info = 
    HD_BACKGROUND_INFO (g_async_result_get_source_object (G_ASYNC_RESULT (init_result)));
  char *file_contents = NULL;
  gsize file_size;
  GError *error = NULL;

  g_file_load_contents_finish (file,
                               result,
                               &file_contents,
                               &file_size,
                               NULL,
                               &error);

  if (error)
    {
      g_simple_async_result_set_from_error (init_result,
                                            error);
      g_error_free (error);
      goto complete;
    }

  if (!load_background_info_from_key_file (info,
                                           file_contents,
                                           file_size))
    {
      load_background_info_legacy (info,
                                   file_contents,
                                   file_size);
    }

  g_simple_async_result_set_op_res_gboolean (G_SIMPLE_ASYNC_RESULT (init_result),
                                             TRUE);

complete:
  g_free (file_contents);

  g_simple_async_result_complete (init_result);  
}

static gboolean
load_background_info_from_key_file (HDBackgroundInfo  *info,
                                    char              *file_contents,
                                    gsize              file_size)
{
  GKeyFile *key_file = g_key_file_new ();
  guint version;
  GError *error = NULL;
  gboolean result = TRUE;

  g_key_file_load_from_data (key_file,
                             file_contents,
                             file_size,
                             G_KEY_FILE_NONE,
                             &error);

  if (error)
    {
      g_debug ("%s. Could not load Keyfile: %s", __FUNCTION__, error->message);
      g_error_free (error);
      result = FALSE;
      goto cleanup;
    }

  version = get_background_info_key_file_version (key_file,
                                                  &error);

  if (error)
    {
      g_debug ("%s. Could not load Keyfile: %s", __FUNCTION__, error->message);
      g_error_free (error);
      goto cleanup;
    }

  if (version != 1)
    {
      g_debug ("%s. Version %u is not supported.", __FUNCTION__, version);
      goto cleanup;
    }

  load_backgrounds_from_key_file (info,
                                  key_file);

cleanup:
  g_key_file_free (key_file);

  return result;
}

static guint
get_background_info_key_file_version (GKeyFile  *keyfile,
                                      GError   **error)
{
  return g_key_file_get_integer (keyfile,
                                 BACKGROUND_INFO_GROUP,
                                 BACKGROUND_INFO_KEY_VERSION,
                                 error);
}

static void
load_backgrounds_from_key_file (HDBackgroundInfo *info,
                                GKeyFile         *key_file)
{
  guint i;
  guint max = HD_DESKTOP_VIEWS;
  if(hd_backgrounds_is_portrait_wallpaper_enabled (hd_backgrounds_get ()))
    max += HD_DESKTOP_VIEWS;

  for (i = 0; i < max; i++)
    {
      load_background_from_key_file (info,
                                     key_file,
                                     i);
    }
}

static void
load_background_from_key_file (HDBackgroundInfo *info,
                               GKeyFile         *key_file,
                               guint             desktop)
{
  HDBackgroundInfoPrivate *priv = info->priv;
  GFile *file;
  char *etag;

  file = get_file_from_key_file (key_file,
                                 desktop);
  etag = get_etag_from_key_file (key_file,
                                 desktop);

  hd_object_vector_set_at (priv->files,
                           desktop,
                           file);
  g_object_unref (file);
  g_ptr_array_index (priv->etags, desktop) = etag;
}

static GFile*
get_file_from_key_file (GKeyFile *key_file,
                        guint     i)
{
  GFile *file = NULL;
  char *key;
  char *uri;

  key = g_strdup_printf (BACKGROUND_INFO_KEY_FILE_FMT, i);
  uri = g_key_file_get_string (key_file,
                               BACKGROUND_INFO_GROUP,
                               key,
                               NULL);

  if (uri)
    file = g_file_new_for_uri (uri);

  g_free (key);
  g_free (uri);

  return file;
}

static char*
get_etag_from_key_file (GKeyFile *key_file,
                        guint     i)
{
  char *key;
  char *etag;

  key = g_strdup_printf (BACKGROUND_INFO_KEY_ETAG_FMT, i);
  etag = g_key_file_get_string (key_file,
                                BACKGROUND_INFO_GROUP,
                                key,
                                NULL);

  g_free (key);

  return etag;
}

static void
load_background_info_legacy (HDBackgroundInfo *info,
                             char             *file_contents,
                             gsize             file_size)
{
  HDBackgroundInfoPrivate *priv = info->priv;
  char **cache_info = g_strsplit (file_contents, "\n", 0);

  if (cache_info)
    {
      guint i;
      guint max = HD_DESKTOP_VIEWS;
      if(hd_backgrounds_is_portrait_wallpaper_enabled (hd_backgrounds_get ()))
        max += HD_DESKTOP_VIEWS;

      for (i = 0; i < max && cache_info[i]; i++)
        {
          GFile *file = g_file_new_for_uri (cache_info[i]);
          hd_object_vector_set_at (priv->files,
                                   i,
                                   file);
          g_object_unref (file);
        }
      g_strfreev (cache_info);
    }
}

gboolean
hd_background_info_init_finish (HDBackgroundInfo  *info,
                                GAsyncResult      *result,
                                GError           **error)
{
  g_return_val_if_fail (g_simple_async_result_is_valid (result,
                                                        G_OBJECT (info),
                                                        hd_background_info_init_async),
                        FALSE);

  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
                                             error))
    return FALSE;

  return g_simple_async_result_get_op_res_gboolean (G_SIMPLE_ASYNC_RESULT (result));
}

static void
hd_background_info_dispose (GObject *object)
{
  HDBackgroundInfoPrivate *priv = HD_BACKGROUND_INFO (object)->priv;

  if (priv->etags)
    {
      g_ptr_array_free (priv->etags, FALSE);
      priv->etags = NULL;
    }

  if (priv->files)
    priv->files = (g_object_unref (priv->files), NULL);
  
  G_OBJECT_CLASS (hd_background_info_parent_class)->dispose (object);
}

GFile *
hd_background_info_get_file (HDBackgroundInfo *info,
                             guint             desktop)
{
  HDBackgroundInfoPrivate *priv;

  g_return_val_if_fail (HD_IS_BACKGROUND_INFO (info), NULL);

  priv = HD_BACKGROUND_INFO (info)->priv;

  return hd_object_vector_at (priv->files,
                              desktop);
}

const char *
hd_background_info_get_etag (HDBackgroundInfo *info,
                             guint             desktop)
{
  HDBackgroundInfoPrivate *priv;

  g_return_val_if_fail (HD_IS_BACKGROUND_INFO (info), NULL);

  priv = HD_BACKGROUND_INFO (info)->priv;

  return g_ptr_array_index (priv->etags,
                            desktop);
}

void
hd_background_info_set (HDBackgroundInfo *info,
                        guint             desktop,
                        GFile            *file,
                        const char       *etag)
{
  HDBackgroundInfoPrivate *priv;

  g_return_if_fail (HD_IS_BACKGROUND_INFO (info));

  priv = HD_BACKGROUND_INFO (info)->priv;

  hd_object_vector_set_at (priv->files,
                           desktop,
                           file);
  g_ptr_array_index (priv->etags,
                     desktop) = g_strdup (etag);

  save_background_info_file (info);
}

static void
save_background_info_file (HDBackgroundInfo *info)
{
  HDBackgroundInfoPrivate *priv = info->priv;
  GKeyFile *key_file = g_key_file_new ();
  GFile *background_info_file;
  guint desktop;
  char *contents;
  gsize length;
  GError *error = NULL;

  g_key_file_set_integer (key_file,
                          BACKGROUND_INFO_GROUP,
                          BACKGROUND_INFO_KEY_VERSION,
                          1);

  guint max = HD_DESKTOP_VIEWS;
  if(hd_backgrounds_is_portrait_wallpaper_enabled (hd_backgrounds_get ()))
    max += HD_DESKTOP_VIEWS;

  for (desktop = 0; desktop < max; ++desktop)
    {
      GFile *file;
      const char *etag;

      file = hd_object_vector_at (priv->files, desktop);

      if (file)
        {
          char *key, *value;

          key = g_strdup_printf (BACKGROUND_INFO_KEY_FILE_FMT, desktop);
          value = g_file_get_uri (file);

          g_key_file_set_string (key_file,
                                 BACKGROUND_INFO_GROUP,
                                 key,
                                 value);

          g_free (key);
          g_free (value);
        }

      etag = g_ptr_array_index (priv->etags, desktop);
      if (etag)
        {
          char *key;

          key = g_strdup_printf (BACKGROUND_INFO_KEY_ETAG_FMT, desktop);

          g_key_file_set_string (key_file,
                                 BACKGROUND_INFO_GROUP,
                                 key,
                                 etag);

          g_free (key);
        }
    }

  contents = g_key_file_to_data (key_file,
                                 &length,
                                 NULL);

  background_info_file = get_background_info_file ();
  g_file_replace_contents (background_info_file,
                           contents,
                           length,
                           NULL,
                           FALSE,
                           G_FILE_CREATE_NONE,
                           NULL,
                           NULL,
                           &error);

  if (error)
    {
      g_warning ("%s. Could not write cache info file. %s",
                 __FUNCTION__,
                 error->message);
      g_error_free (error);
    }

  g_key_file_free (key_file);
  g_free (contents);
  g_object_unref (background_info_file);
}
