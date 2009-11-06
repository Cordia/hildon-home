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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <gio/gio.h>

#include <hildon/hildon.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdkx.h>

#include <gconf/gconf-client.h>

#include <libgnomevfs/gnome-vfs.h>

#include <unistd.h>

#include "hd-command-thread-pool.h"

#include "hd-backgrounds.h"

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 480
#define ASPECT_RATIO ((1. * SCREEN_WIDTH) / SCREEN_HEIGHT)

#define CACHED_DIR        ".backgrounds"
#define BACKGROUND_CACHED_PVR CACHED_DIR "/background-%u.pvr"
#define BACKGROUND_CACHED_PNG CACHED_DIR "/background-%u.png"
#define THUMBNAIL_CACHED  CACHED_DIR "/thumbnail-%u.png"
#define INFO_CACHED       CACHED_DIR "/background-%u.info"

#define CACHE_INFO_FILE   CACHED_DIR "/cache.info"

/* Background GConf key */
#define GCONF_DIR                 "/apps/osso/hildon-desktop/views"
#define GCONF_BACKGROUND_KEY      GCONF_DIR "/%u/bg-image"
#define GCONF_CURRENT_DESKTOP_KEY "/apps/osso/hildon-desktop/views/current"

/* Background from theme */
#define CURRENT_THEME_DIR            "/etc/hildon/theme"
#define CURRENT_BACKGROUNDS_DESKTOP  CURRENT_THEME_DIR "/backgrounds/theme_bg.desktop"
#define DEFAULT_THEME_DIR            "/usr/share/themes/default"
#define DEFAULT_BACKGROUNDS_DESKTOP  DEFAULT_THEME_DIR "/backgrounds/theme_bg.desktop"
#define BACKGROUNDS_DESKTOP_KEY_FILE "X-File%u"

#define VIEWS 4

#define THUMBNAIL_WIDTH 125
#define THUMBNAIL_HEIGHT 75
#define THUMBNAIL_TYPE "png"

#define HD_BACKGROUNDS_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_BACKGROUNDS, HDBackgroundsPrivate))

typedef struct
{
  GFile *file;
  guint view;
  GCancellable *cancellable;
  gboolean write_to_gconf : 1;
  gboolean display_banner : 1;
  gboolean image_set : 1;
} HDBackgroundData;

struct _HDBackgroundsPrivate
{
  GConfClient *gconf_client;

  /* GConf notify handlers */
  guint bg_image_notify[VIEWS];

  /* Data used for the thread which creates the cached images */
  HDCommandThreadPool *thread_pool;
  GMutex              *mutex;

  HDBackgroundData *current_requests[VIEWS];
  GFile            *current_bg_image[VIEWS];

  GSourceFunc       change_request_cb;
  gpointer          cb_data;

  /* Theme change support */
  gchar *current_theme;
  guint set_theme_idle_id;

  GVolumeMonitor *volume_monitor;
  GnomeVFSVolumeMonitor *volume_monitor2;
};

static void create_cached_image (HDBackgroundData *data);

G_DEFINE_TYPE (HDBackgrounds, hd_backgrounds, G_TYPE_OBJECT);

static void
background_data_free (HDBackgroundData *data)
{
  if (!data)
    return;

  g_object_unref (data->file);
  g_object_unref (data->cancellable);
  g_slice_free (HDBackgroundData, data);
}

static void
create_cached_background (HDBackgrounds *backgrounds,
                          GFile         *file,
                          guint          view,
                          gboolean       display_note,
                          gboolean       write_to_gconf)
{
  HDBackgroundsPrivate *priv = backgrounds->priv;
  HDBackgroundData *data, *current;
  
  data = g_slice_new0 (HDBackgroundData);

  data->file = g_object_ref (file);
  data->view = view;
  data->write_to_gconf = write_to_gconf;
  data->display_banner = display_note;
  data->cancellable = g_cancellable_new ();

  g_mutex_lock (priv->mutex);
  current = priv->current_requests[view];
  if (current)
    {
      if (g_file_equal (current->file, file))
        {
          background_data_free (data);
          data = NULL;
        }
      else
        {
          g_cancellable_cancel (current->cancellable);
          priv->current_requests[view] = NULL;
        }
    }

  if (data)
    {
      priv->current_requests[view] = data;

      hd_command_thread_pool_push (priv->thread_pool,
                                   (HDCommandCallback) create_cached_image,
                                   data,
                                   NULL);
    }
  g_mutex_unlock (priv->mutex);
}

static void
size_prepared_cb (GdkPixbufLoader *loader,
                  gint             width,
                  gint             height,
                  gboolean        *scaling_required)
{
  double ratio, w, h;

  /*
   * Background image should be resized and cropped. That means the image
   * is centered and scaled to make sure the shortest side fit the home 
   * background exactly with keeping the aspect ratio of the original image
   */

  ratio = (1. * width) / height;

  if (ratio > ASPECT_RATIO)
    {
      h = SCREEN_HEIGHT;
      w = ratio * SCREEN_HEIGHT;
    }
  else
    {
      h = SCREEN_WIDTH / ratio;
      w = SCREEN_WIDTH;
    }

  if (scaling_required)
    {
      *scaling_required = (width != SCREEN_WIDTH) ||
                          (height != SCREEN_HEIGHT);
    }

/*  w = SCREEN_WIDTH;
  h = SCREEN_HEIGHT;*/

  gdk_pixbuf_loader_set_size (loader, w, h);
}

static GdkPixbuf *
read_pixbuf (GFile         *file,
             GCancellable  *cancellable,
             gboolean      *scaling_required,
             GError       **error)
{
  GFileInputStream *stream = NULL;
  GdkPixbufLoader *loader = NULL;
  guchar buffer[8192];
  gssize read_bytes;
  GdkPixbuf *pixbuf = NULL;

  /* Open file for read */
  stream = g_file_read (file, cancellable, error);

  if (!stream)
    {
      goto cleanup;
    }

  /* Create pixbuf loader */
  loader = gdk_pixbuf_loader_new ();
  g_signal_connect (loader, "size-prepared",
                    G_CALLBACK (size_prepared_cb), scaling_required);

  /* Parse input stream into the loader */
  do
    {
      read_bytes = g_input_stream_read (G_INPUT_STREAM (stream),
                                        buffer,
                                        sizeof (buffer),
                                        cancellable,
                                        error);
      if (read_bytes < 0)
        {
          gdk_pixbuf_loader_close (loader, NULL);
          goto cleanup;
        }

      if (!gdk_pixbuf_loader_write (loader,
                                    buffer,
                                    read_bytes,
                                    error))
        {
          gdk_pixbuf_loader_close (loader, NULL);
          goto cleanup;
        }
    } while (read_bytes > 0);

  /* Close pixbuf loader */
  if (!gdk_pixbuf_loader_close (loader, error))
    {
      goto cleanup;
    }

  /* Close input stream */
  if (!g_input_stream_close (G_INPUT_STREAM (stream),
                             cancellable,
                             error))
    {
      goto cleanup;
    }

  /* Set resulting pixbuf */
  pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
  if (pixbuf)
    g_object_ref (pixbuf);
  else
    g_set_error_literal (error,
                         GDK_PIXBUF_ERROR,
                         GDK_PIXBUF_ERROR_FAILED,
                         "NULL Pixbuf returned from loader");

cleanup:
  if (stream)
    g_object_unref (stream);
  if (loader)
    g_object_unref (loader);

  return pixbuf;
}

static GFile *
get_precreated_thumbnail_file (GFile *file)
{
  gchar *uri, *thumbnail_uri, *p;
  GFile *thumbnail_file;

  uri = g_file_get_uri (file);
  p = g_strrstr (uri, ".");
  if (p)
    *p = '\0';

  thumbnail_uri = g_strdup_printf ("%s-thumb.png", uri);
  thumbnail_file = g_file_new_for_uri (thumbnail_uri);

  g_free (uri);
  g_free (thumbnail_uri);

  return thumbnail_file;
}

static gboolean
save_pixbuf_to_file (GFile         *file,
                     GdkPixbuf     *pixbuf,
                     const gchar   *type,
                     GCancellable  *cancellable,
                     GError       **error)
{
  gchar *buffer = NULL;
  gsize buffer_size;
  gboolean result;

  if (!gdk_pixbuf_save_to_buffer (pixbuf,
                                  &buffer,
                                  &buffer_size,
                                  type,
                                  error,
                                  NULL))
    return FALSE;

  result = g_file_replace_contents (file,
                                    buffer,
                                    buffer_size,
                                    NULL,
                                    FALSE,
                                    G_FILE_CREATE_NONE,
                                    NULL,
                                    cancellable,
                                    error);

  g_free (buffer);

  return result;
}

static void
update_cache_info_file (HDBackgrounds *backgrounds,
                        GCancellable  *cancellable)
{
  HDBackgroundsPrivate *priv = backgrounds->priv;
  gchar *cache_info_filename, *contents;
  GFile *cache_info_file;
  GString *buffer;
  guint i;
  GError *error = NULL;

  cache_info_filename = g_strdup_printf ("%s/" CACHE_INFO_FILE,
                                         g_get_home_dir ());
  cache_info_file = g_file_new_for_path (cache_info_filename);

  buffer = g_string_sized_new (512);
  g_mutex_lock (priv->mutex);
  for (i = 0; i < VIEWS; i++)
    {
      gchar *uri = priv->current_bg_image[i] ? g_file_get_uri (priv->current_bg_image[i]) : NULL;
      if (uri)
        g_string_append (buffer, uri);
      g_free (uri);
      g_string_append_c (buffer, '\n');
    }
  g_mutex_unlock (priv->mutex);
  contents = g_string_free (buffer, FALSE);

  if (!g_file_replace_contents (cache_info_file,
                                contents,
                                strlen (contents),
                                NULL,
                                FALSE,
                                G_FILE_CREATE_NONE,
                                NULL,
                                cancellable,
                                &error))
    {
      g_warning ("%s. Could not write cache info file. %s",
                 __FUNCTION__,
                 error->message);
      g_error_free (error); 
    }

  g_free (contents);
  g_free (cache_info_filename);
  g_object_unref (cache_info_file);
}

static gboolean
idle_show_banner (gchar *summary)
{
  GtkWidget *note;

  g_debug ("%s. %s", __FUNCTION__, summary);

  note = hildon_note_new_information (NULL,
                                      summary);
  gtk_widget_show (note);

  return FALSE;
}

static void
show_banner (const gchar *summary)
{
  gdk_threads_add_idle_full (G_PRIORITY_HIGH_IDLE,
                             (GSourceFunc) idle_show_banner,
                             g_strdup (summary),
                             g_free);
}

static void
create_cached_image (HDBackgroundData *data)
{
  HDBackgrounds *backgrounds = hd_backgrounds_get ();
  HDBackgroundsPrivate *priv = backgrounds->priv;
  GdkPixbuf *pixbuf = NULL;
  gboolean scaling_required = FALSE, thumbnail_created = FALSE;
  gchar *dest_filename;
  GFile *dest_file, *dest_thumbnail = NULL;
  GError *error = NULL;

  /* Cancel if it is already the current background image */
  g_mutex_lock (priv->mutex);
  if (priv->current_bg_image[data->view] &&
      g_file_equal (data->file,
                    priv->current_bg_image[data->view]))
    g_cancellable_cancel (data->cancellable);
  g_mutex_unlock (priv->mutex);

  /* Check if the queued request was already cancelled */
  if (g_cancellable_is_cancelled (data->cancellable))
    goto cleanup;

  /* Read pixbuf from background image, check if the
   * image needs to be scaled */
  pixbuf = read_pixbuf (data->file,
                        data->cancellable,
                        &scaling_required,
                        &error);

  if (error)
    {
      gchar *uri;

      /* Display corrupt image notification banner */
      if (data->display_banner &&
          (g_error_matches (error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_CORRUPT_IMAGE) ||
           g_error_matches (error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_UNKNOWN_TYPE)))
        {
          show_banner (dgettext ("hildon-common-strings",
                                 "ckct_ni_unable_to_open_file_corrupted"));
        }

      /* Print warning to syslog */
      uri = g_file_get_uri (data->file);
      g_warning ("%s. Could not read pixbuf from file %s. %s",
                 __FUNCTION__,
                 uri,
                 error->message);
      g_free (uri);

      g_error_free (error);
      goto cleanup;
    }

  /* Create the file objects for the cached background image and thumbnail */
  dest_filename = g_strdup_printf ("%s/" BACKGROUND_CACHED_PNG,
                                   g_get_home_dir (),
                                   data->view + 1);
  dest_file = g_file_new_for_path (dest_filename);
  g_free (dest_filename);

  dest_filename = g_strdup_printf ("%s/" THUMBNAIL_CACHED,
                                   g_get_home_dir (),
                                   data->view + 1);
  dest_thumbnail = g_file_new_for_path (dest_filename);
  g_free (dest_filename);

  /* Create the cached background image */
  if (!scaling_required)
    {
      /* If no scaling is required just copy the original background
       * image to the cached background image */
      if (!g_file_copy (data->file,
                        dest_file,
                        G_FILE_COPY_OVERWRITE,
                        data->cancellable,
                        NULL,
                        NULL,
                        &error))
        {
          gchar *uri, *dest_uri;

          /* Display not enough space notification banner */
          if (data->display_banner &&
              g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NO_SPACE))
            {
              show_banner (dgettext ("hildon-common-strings",
                                     "sfil_ni_not_enough_memory"));
            }

          uri = g_file_get_uri (data->file);
          dest_uri = g_file_get_uri (dest_file);

          g_warning ("%s. Could not copy file %s to file %s. %s",
                     __FUNCTION__,
                     uri,
                     dest_uri,
                     error->message);

          g_free (uri);
          g_free (dest_uri);
          g_error_free (error);
          goto cleanup;
        }
    }
  else
    {
      /* Else scale the pixbuf and write it to the cached
       * background image */
      GdkPixbuf *sub;

      sub = gdk_pixbuf_new_subpixbuf (pixbuf,
                                      (gdk_pixbuf_get_width (pixbuf) - SCREEN_WIDTH) / 2,
                                      (gdk_pixbuf_get_height (pixbuf) - SCREEN_HEIGHT) / 2,
                                      SCREEN_WIDTH,
                                      SCREEN_HEIGHT);
      g_object_unref (pixbuf);
      pixbuf = sub;

      /* FIXME: wrong */
      if (!save_pixbuf_to_file (dest_file,
                                pixbuf,
                                "png",
                                data->cancellable,
                                &error))
        {
          gchar *uri = g_file_get_uri (dest_file);

          /* Display not enough space notification banner */
          if (data->display_banner &&
              g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NO_SPACE))
            {
              show_banner (dgettext ("hildon-common-strings",
                                     "sfil_ni_not_enough_memory"));
            }

          g_warning ("%s. Could not create cached background image %s. %s",
                     __FUNCTION__,
                     uri,
                     error->message);
          g_free (uri);
          g_error_free (error);
          goto cleanup;
        }
    }

  /* Remove not uptodate thumbnail it will be recreated later */
  if (!g_file_delete (dest_thumbnail,
                      data->cancellable,
                      &error))
    {
      gchar *uri = g_file_get_uri (dest_thumbnail);
      g_warning ("%s. Could not delete old cached thumbnail %s. %s",
                 __FUNCTION__,
                 uri,
                 error->message);
      g_free (uri);
      g_clear_error (&error);
    }

  /* Update cache info at this point thumbnail might not be created 
   * yet but background is */
  g_mutex_lock (priv->mutex);
  if (priv->current_bg_image[data->view])
    g_object_unref (priv->current_bg_image[data->view]);
  priv->current_bg_image[data->view] = g_object_ref (data->file);
  g_mutex_unlock (priv->mutex);

  update_cache_info_file (backgrounds,
                          data->cancellable);

  /* Update GConf if requested */
  if (data->write_to_gconf)
    {
      gchar *gconf_key, *path;

      path = g_file_get_path (data->file);

      /* Store background to GConf */
      gconf_key = g_strdup_printf (GCONF_BACKGROUND_KEY, data->view + 1);
      gconf_client_set_string (priv->gconf_client,
                               gconf_key,
                               path,
                               &error);
      if (error)
        {
          g_debug ("%s. Could not set background in GConf for view %u. %s",
                   __FUNCTION__,
                   data->view,
                   error->message);
          g_clear_error (&error);
        }

      g_free (gconf_key);
      g_free (path);
    }

  /* Check if cancelled */
  if (g_cancellable_is_cancelled (data->cancellable))
    goto cleanup;

  /* Try to copy a precreated thumbnail to cache */
  if (data->image_set)
    {
      GFile *thumbnail;

      thumbnail = get_precreated_thumbnail_file (data->file);

      if (g_file_copy (thumbnail,
                       dest_thumbnail,
                       G_FILE_COPY_OVERWRITE,
                       data->cancellable,
                       NULL,
                       NULL,
                       &error))
        {
          thumbnail_created = TRUE;
        }
      else if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NO_SPACE))
        {
          gchar *uri, *dest_uri;

          if (data->display_banner)
            {
              show_banner (dgettext ("hildon-common-strings",
                                     "sfil_ni_not_enough_memory"));
            }

          uri = g_file_get_uri (data->file);
          dest_uri = g_file_get_uri (dest_file);

          g_warning ("%s. Could not copy thumbnail file %s to file %s. %s",
                     __FUNCTION__,
                     uri,
                     dest_uri,
                     error->message);

          g_free (uri);
          g_free (dest_uri);
          g_error_free (error);
          goto cleanup;
        }
      else if (!g_error_matches (error,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_FOUND))
        {
          gchar *uri, *dest_uri;

          uri = g_file_get_uri (data->file);
          dest_uri = g_file_get_uri (dest_file);

          g_warning ("%s. Could not copy thumbnail file %s to file %s. %s",
                     __FUNCTION__,
                     uri,
                     dest_uri,
                     error->message);

          g_free (uri);
          g_free (dest_uri);
          g_error_free (error);
          goto cleanup;
        }
    }

  /* Create a cached thumbnail from parsed pixbuf */
  if (!thumbnail_created)
    {
      GdkPixbuf *thumbnail_pixbuf;

      thumbnail_pixbuf = gdk_pixbuf_scale_simple (pixbuf,
                                                  THUMBNAIL_WIDTH,
                                                  THUMBNAIL_HEIGHT,
                                                  GDK_INTERP_BILINEAR);

      if (!save_pixbuf_to_file (dest_thumbnail,
                                thumbnail_pixbuf,
                                THUMBNAIL_TYPE,
                                data->cancellable,
                                &error))
        {
          gchar *uri = g_file_get_uri (dest_thumbnail);
          g_warning ("%s. Could not create cached thumbnail file %s. %s",
                     __FUNCTION__,
                     uri,
                     error->message);
          g_free (uri);
          g_error_free (error);
          goto cleanup;
        }

      g_object_unref (thumbnail_pixbuf);
    }

cleanup:
  g_mutex_lock (priv->mutex);
  if (priv->current_requests[data->view] == data)
    priv->current_requests[data->view] = NULL;
  g_mutex_unlock (priv->mutex);

  if (pixbuf)
    g_object_unref (pixbuf);

  background_data_free (data);
}

static GFile *
get_background_for_view_from_theme (HDBackgrounds *backgrounds,
                                    guint          view,
                                    const gchar   *backgrounds_desktop)
{
  GKeyFile *key_file;
  gchar *bg_image[VIEWS];
  GFile *background = NULL;
  guint i;
  GError *error = NULL;

  key_file = g_key_file_new ();
  if (!g_key_file_load_from_file (key_file,
                                  backgrounds_desktop,
                                  G_KEY_FILE_NONE,
                                  &error))
    {
      g_debug ("%s. Could not load background defintion for theme %s. %s",
               __FUNCTION__,
               backgrounds_desktop,
               error->message);
      g_error_free (error);
      g_key_file_free (key_file);
      return NULL;
    }

  for (i = 0; i < VIEWS; i++)
    {
      gchar *key;

      key = g_strdup_printf (BACKGROUNDS_DESKTOP_KEY_FILE, i + 1);

      bg_image[i] = g_key_file_get_string (key_file,
                                           G_KEY_FILE_DESKTOP_GROUP,
                                           key,
                                           &error);

      if (error)
        {
          g_debug ("%s. Could not load background defintion for theme %s. %s",
                   __FUNCTION__,
                   backgrounds_desktop,
                   error->message);
          g_clear_error (&error);
        }

      g_free (key);
    }

  g_key_file_free (key_file);

  if (bg_image[view - 1])
    {
      background = g_file_new_for_path (bg_image[view - 1]);
    }

  for (i = 0; i < VIEWS; i++)
    g_free (bg_image[i]);

  return background;
}

static GFile *
get_background_for_view (HDBackgrounds *backgrounds,
                         guint          view)
{
  HDBackgroundsPrivate *priv = backgrounds->priv;
  gchar *gconf_key, *path;
  GFile *bg_image = NULL;
  GError *error = NULL;

  /* First try to get the background image from GConf */
  gconf_key = g_strdup_printf (GCONF_BACKGROUND_KEY, view);
  path = gconf_client_get_string (priv->gconf_client,
                                  gconf_key,
                                  &error);

  if (error)
    {
      g_debug ("%s. Could not get background from GConf for view %u. %s",
               __FUNCTION__,
               view,
               error->message);
      g_error_free (error);
    }

  g_free (gconf_key);

  if (path && *path)
    bg_image = g_file_new_for_path (path);

  /* Return bg image if it was stored in GConf */
  if (bg_image)
    {
      g_free (path);
      return bg_image;
    }

  /* Fallback to current theme */
  bg_image = get_background_for_view_from_theme (backgrounds,
                                                 view,
                                                 CURRENT_BACKGROUNDS_DESKTOP);
  if (bg_image)
    return bg_image;


  /* Fallback to default theme */
  bg_image = get_background_for_view_from_theme (backgrounds,
                                                 view,
                                                 DEFAULT_BACKGROUNDS_DESKTOP);
  if (!bg_image)
    {
      g_warning ("%s. Could not get any background for view %u",
                 __FUNCTION__,
                 view);
    }

  return bg_image;
}

static void 
gconf_bgimage_notify (GConfClient *client,
                      guint        cnxn_id,
                      GConfEntry  *entry,
                      gpointer     user_data)
{
  HDBackgrounds *backgrounds = hd_backgrounds_get ();
  GFile *bg_image;

  bg_image = get_background_for_view (backgrounds,
                                      GPOINTER_TO_UINT (user_data));
  if (bg_image)
    {
      create_cached_background (backgrounds,
                                bg_image,
                                GPOINTER_TO_UINT (user_data) - 1,
                                TRUE,
                                FALSE);
      g_object_unref (bg_image);
    }
}

static void
load_cache_info_cb (GFile         *file,
                    GAsyncResult  *result,
                    HDBackgrounds *backgrounds)
{
  HDBackgroundsPrivate *priv = backgrounds->priv;
  gchar *contents;
  guint current_view, i;
  GFile *bg_image;
  GError *error = NULL;

  if (g_file_load_contents_finish (file, result,
                                   &contents, NULL, NULL,
                                   &error))
    {
      gchar **cache_info;

      g_mutex_lock (priv->mutex);
      for (i = 0; i < VIEWS; i++)
        if (priv->current_bg_image[i])
          priv->current_bg_image[i] = (g_object_unref (priv->current_bg_image[i]), NULL);

      cache_info = g_strsplit (contents, "\n", 0);
      if (cache_info)
        {
          for (i = 0; i < VIEWS && cache_info[i]; i++)
            priv->current_bg_image[i] = g_file_new_for_uri (cache_info[i]);
          g_strfreev (cache_info);
        }
      g_mutex_unlock (priv->mutex);

      g_free (contents);
    }
  else if (error)
    {
      g_debug ("%s. Could not read cache info file. %s",
               __FUNCTION__,
               error->message);
      g_clear_error (&error);
    }

  current_view = gconf_client_get_int (priv->gconf_client,
                                       GCONF_CURRENT_DESKTOP_KEY,
                                       &error);

  if (error)
    {
      g_debug ("%s. Could not get current view. %s",
               __FUNCTION__,
               error->message);
      g_clear_error (&error);
    }

  current_view = CLAMP (current_view, 1, VIEWS);

  /* Update cache for current view */
  bg_image = get_background_for_view (backgrounds,
                                      current_view);
  if (bg_image)
    {
      create_cached_background (backgrounds,
                                bg_image,
                                current_view - 1,
                                FALSE,
                                FALSE);
      g_object_unref (bg_image);
    }

  /* Update cache for other views */
  for (i = 1; i <= VIEWS; i++)
    {
      if (i != current_view)
        {
          bg_image = get_background_for_view (backgrounds,
                                              i);
          if (bg_image)
            {
              create_cached_background (backgrounds,
                                        bg_image,
                                        i - 1,
                                        FALSE,
                                        FALSE);
            }
        }
    }
}

static gboolean
restart_hildon_home (gpointer data)
{
  gtk_main_quit ();

  return FALSE;
}

/* Only supports themes with four backgrounds set */
static void
update_backgrounds_from_theme (HDBackgrounds *backgrounds,
                               const gchar   *backgrounds_desktop)
{
  HDBackgroundsPrivate *priv = backgrounds->priv;
  GKeyFile *key_file;
  gchar *current_theme;
  GFile *bg_image[VIEWS];
  gint current_view;
  guint i;
  GError *error = NULL;

  current_theme = g_new0 (gchar, 2048);
  if (readlink (CURRENT_THEME_DIR, current_theme, 2047) > 0)
    {
      if (g_strcmp0 (priv->current_theme, current_theme) == 0)
        {
          g_free (current_theme);
          return;
        }
      else
        {
          g_free (priv->current_theme);
          priv->current_theme = current_theme;
        }
    }
  else
    {
      g_free (current_theme);
      return;
    }

  key_file = g_key_file_new ();
  if (!g_key_file_load_from_file (key_file,
                                  backgrounds_desktop,
                                  G_KEY_FILE_NONE,
                                  &error))
    {
      g_debug ("%s. Could not load background defintion for theme %s. %s",
               __FUNCTION__,
               backgrounds_desktop,
               error->message);
      g_error_free (error);
      g_key_file_free (key_file);
      return;
    }

  for (i = 0; i < VIEWS; i++)
    {
      gchar *key;
      gchar *path;

      key = g_strdup_printf (BACKGROUNDS_DESKTOP_KEY_FILE, i + 1);

      path = g_key_file_get_string (key_file,
                                    G_KEY_FILE_DESKTOP_GROUP,
                                    key,
                                    &error);

      if (error)
        {
          g_debug ("%s. Could not load background defintion for theme %s. %s",
                   __FUNCTION__,
                   backgrounds_desktop,
                   error->message);
          g_clear_error (&error);
          g_free (key);
          g_key_file_free (key_file);
          return;
        }

      if (path)
        bg_image[i] = g_file_new_for_path (path);

      g_free (path);
      g_free (key);
    }

  g_key_file_free (key_file);

  current_view = gconf_client_get_int (priv->gconf_client,
                                       GCONF_CURRENT_DESKTOP_KEY,
                                       &error);

  if (error)
    {
      g_debug ("%s. Could not get current view. %s",
               __FUNCTION__,
               error->message);
      g_clear_error (&error);
    }

  /* Set to 0..3 */
  current_view--;

  if (current_view >= 0 && current_view < VIEWS)
    create_cached_background (backgrounds,
                              bg_image[current_view],
                              current_view,
                              FALSE,
                              TRUE);

  /* Update cache for other views */
  for (i = 0; i < VIEWS; i++)
    {
      if (i != current_view)
        {
          create_cached_background (backgrounds,
                                    bg_image[i],
                                    i,
                                    FALSE,
                                    TRUE);
        }
    }

  for (i = 0; i < VIEWS; i++)
    g_object_unref (bg_image[i]);

  hd_command_thread_pool_push_idle (priv->thread_pool,
                                    G_PRIORITY_HIGH_IDLE,
                                    restart_hildon_home,
                                    NULL,
                                    NULL);
}

static gboolean
set_theme_idle (gpointer data)
{
  HDBackgroundsPrivate *priv = HD_BACKGROUNDS (data)->priv;

  priv->set_theme_idle_id = 0;

  g_debug ("%s", __FUNCTION__);

  update_backgrounds_from_theme (HD_BACKGROUNDS (data),
                                 CURRENT_BACKGROUNDS_DESKTOP);

  return FALSE;
}

static GdkFilterReturn
hd_backgrounds_theme_changed (GdkXEvent *xevent,
                              GdkEvent *event,
                              gpointer data)
{
  HDBackgroundsPrivate *priv = HD_BACKGROUNDS (data)->priv;
  XEvent *ev = (XEvent *) xevent;

  if (ev->type == PropertyNotify)
    {
      if (ev->xproperty.atom == gdk_x11_get_xatom_by_name ("_MB_THEME"))
        {
          if (!priv->set_theme_idle_id)
            priv->set_theme_idle_id = gdk_threads_add_idle (set_theme_idle,
                                                            data);
        }
    }

  return GDK_FILTER_CONTINUE;
}

void
hd_backgrounds_startup (HDBackgrounds *backgrounds)
{
  HDBackgroundsPrivate *priv = backgrounds->priv;
  GdkWindow *root_win;
  gchar *cached_dir;
  guint i;
  gchar *cache_info_filename;
  GFile *cache_info_file;
  GError *error = NULL;
  gchar *current_theme;

  /* Get current theme */
  current_theme = g_new0 (gchar, 2048);
  if (readlink (CURRENT_THEME_DIR, current_theme, 2047) > 0)
    {
      priv->current_theme = current_theme;
    }
  else
    {
      g_warning ("%s. Could not read current theme", __FUNCTION__);
      g_free (current_theme);
    }

  root_win = gdk_window_foreign_new_for_display (gdk_display_get_default (),
                                                 gdk_x11_get_default_root_xwindow ());
  gdk_window_set_events (root_win,
                         gdk_window_get_events (root_win) |
                         GDK_PROPERTY_CHANGE_MASK);

  gdk_window_add_filter (root_win,
                         hd_backgrounds_theme_changed,
                         backgrounds);


  cached_dir = g_strdup_printf ("%s/" CACHED_DIR,
                                g_get_home_dir ());
  if (g_mkdir_with_parents (cached_dir,
                            S_IRUSR | S_IWUSR | S_IXUSR |
                            S_IRGRP | S_IXGRP | 
                            S_IROTH | S_IXOTH))
    {
      g_warning ("%s, Could not make dir %s",
                 __FUNCTION__,
                 cached_dir);
    }
  g_free (cached_dir);

  gconf_client_add_dir (priv->gconf_client,
                        GCONF_DIR,
                        GCONF_CLIENT_PRELOAD_NONE,
                        &error);
  if (error)
    {
      g_debug ("%s. Could not add dir %s in GConf. %s",
               __FUNCTION__,
               GCONF_DIR,
               error->message);
      g_clear_error (&error);
    }


  /* Listen to GConf changes */
  for (i = 0; i < VIEWS; i++)
    {
      gchar *gconf_key;

      gconf_key = g_strdup_printf (GCONF_BACKGROUND_KEY, i + 1);
      priv->bg_image_notify[i] = gconf_client_notify_add (priv->gconf_client,
                                                          gconf_key,
                                                          (GConfClientNotifyFunc) gconf_bgimage_notify,
                                                          GUINT_TO_POINTER (i + 1),
                                                          NULL,
                                                          &error);
      if (error)
        {
          g_warning ("%s. Could not add notification to GConf %s. %s",
                     __FUNCTION__,
                     gconf_key,
                     error->message);
          g_clear_error (&error);
        }
      g_free (gconf_key);
    }

  /* Load cache info file */
  cache_info_filename = g_strdup_printf ("%s/" CACHE_INFO_FILE,
                                         g_get_home_dir ());
  cache_info_file = g_file_new_for_path (cache_info_filename);
  g_file_load_contents_async (cache_info_file,
                              NULL,
                              (GAsyncReadyCallback) load_cache_info_cb,
                              backgrounds);
  g_free (cache_info_filename);
  g_object_unref (cache_info_file);

}

static void
mount_pre_unmount_cb (GVolumeMonitor *monitor,
                      GMount         *mount,
                      HDBackgrounds  *backgrounds)
{
  HDBackgroundsPrivate *priv = backgrounds->priv;
  GFile *mount_root;
  guint i;
  gboolean display_banner = FALSE;

  mount_root = g_mount_get_root (mount);

  g_warning ("%s. Mount: %s", __FUNCTION__, g_file_get_path (mount_root));

  g_mutex_lock (priv->mutex);
  for (i = 0; i < VIEWS; i++)
    {
      HDBackgroundData *data = priv->current_requests[i];

      if (!data)
        continue;

      if (g_file_has_prefix (data->file,
                             mount_root))
        {
          g_warning ("%s. Mount: %s, File: %s", __FUNCTION__, g_file_get_path (mount_root), g_file_get_path (data->file));

          /* Display banner if the background image was user initiated */
          display_banner = (display_banner || data->display_banner);

          /* Cancel the request */
          g_cancellable_cancel (data->cancellable);
          priv->current_requests[i] = NULL;
        }
    }
  g_mutex_unlock (priv->mutex);

  g_object_unref (mount_root);

  /* Display "Opening interrupted.\n Memory card cover open" banner */
  if (display_banner)
    {
      GtkWidget *note;

      g_warning ("%s. Display banner", __FUNCTION__);

      note = hildon_note_new_information (NULL,
                                          dgettext ("hildon-common-strings",
                                                    "sfil_ni_cannot_open_mmc_cover_open"));
      gtk_widget_show (note);
    }
}

static void
volume_pre_unmount_cb (GnomeVFSVolumeMonitor *monitor,
                       GnomeVFSVolume        *volume,
                       HDBackgrounds         *backgrounds)
{
  HDBackgroundsPrivate *priv = backgrounds->priv;
  gchar *uri;
  guint i;
  gboolean display_banner = FALSE;

  uri = gnome_vfs_volume_get_activation_uri (volume);

  g_warning ("%s. Volume: %s", __FUNCTION__, uri);

  g_mutex_lock (priv->mutex);
  for (i = 0; i < VIEWS; i++)
    {
      HDBackgroundData *data = priv->current_requests[i];
      gchar *path;
      GnomeVFSVolume *other;

      if (!data)
        continue;

      path = g_file_get_path (data->file);
      other = gnome_vfs_volume_monitor_get_volume_for_path (priv->volume_monitor2,
                                                            path);
      g_free (path);

      if (other && !gnome_vfs_volume_compare (volume, other))
        {
          /* Display banner if the background image was user initiated */
          display_banner = (display_banner || data->display_banner);

          /* Cancel the request */
          g_cancellable_cancel (data->cancellable);
          priv->current_requests[i] = NULL;
        }
    }
  g_mutex_unlock (priv->mutex);

  /* Display "Opening interrupted.\n Memory card cover open" banner */
  if (display_banner)
    {
      GtkWidget *note;

      g_warning ("%s. Display banner", __FUNCTION__);

      note = hildon_note_new_information (NULL,
                                          dgettext ("hildon-common-strings",
                                                    "sfil_ni_cannot_open_mmc_cover_open"));
      gtk_widget_show (note);
    }
}
static void
hd_backgrounds_init (HDBackgrounds *backgrounds)
{
  HDBackgroundsPrivate *priv;

  backgrounds->priv = HD_BACKGROUNDS_GET_PRIVATE (backgrounds);
  priv = backgrounds->priv;

  priv->gconf_client = gconf_client_get_default ();

  priv->mutex = g_mutex_new ();

  priv->thread_pool = hd_command_thread_pool_new ();

  priv->volume_monitor = g_volume_monitor_get ();
  g_signal_connect (priv->volume_monitor, "mount-pre-unmount",
                    G_CALLBACK (mount_pre_unmount_cb), backgrounds);

  priv->volume_monitor2 = gnome_vfs_get_volume_monitor ();
  g_signal_connect (priv->volume_monitor2, "volume-pre-unmount",
                    G_CALLBACK (volume_pre_unmount_cb), backgrounds);

}

static void
hd_backgrounds_dipose (GObject *object)
{
  HDBackgroundsPrivate *priv = HD_BACKGROUNDS (object)->priv;
  guint i;

  if (priv->gconf_client)
    {
      for (i = 0; i < VIEWS; i++)
        {
          if (priv->bg_image_notify[i])
            priv->bg_image_notify[i] = (gconf_client_notify_remove (priv->gconf_client,
                                                                    priv->bg_image_notify[i]), 0);
        }
      priv->gconf_client = (g_object_unref (priv->gconf_client), NULL);
    }

  if (priv->thread_pool)
    priv->thread_pool = (g_object_unref (priv->thread_pool), NULL);

  for (i = 0; i < VIEWS; i++)
    priv->current_bg_image[i] = (g_object_unref (priv->current_bg_image[i]), NULL);

  if (priv->mutex)
    priv->mutex = (g_mutex_free (priv->mutex), NULL);

  if (priv->set_theme_idle_id)
    priv->set_theme_idle_id = (g_source_remove (priv->set_theme_idle_id), 0);

  priv->current_theme = (g_free (priv->current_theme), NULL);

  G_OBJECT_CLASS (hd_backgrounds_parent_class)->dispose (object);
}

static void
hd_backgrounds_class_init (HDBackgroundsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = hd_backgrounds_dipose;

  g_type_class_add_private (klass, sizeof (HDBackgroundsPrivate));
}

/* Retuns the singleton HDBackgrounds instance. Should not be refed or unrefed */
HDBackgrounds *
hd_backgrounds_get (void)
{
  static HDBackgrounds *backgrounds = NULL;

  if (G_UNLIKELY (!backgrounds))
    {
      backgrounds = g_object_new (HD_TYPE_BACKGROUNDS, NULL);
    }

  return backgrounds;
}

void
hd_backgrounds_set_background (HDBackgrounds *backgrounds,
                               guint          view,
                               const gchar   *uri,
                               GSourceFunc    done_callback,
                               gpointer       cb_data,
                               GDestroyNotify destroy_data)
{
  HDBackgroundsPrivate *priv = backgrounds->priv;
  GFile *file;

  g_return_if_fail (HD_IS_BACKGROUNDS (backgrounds));
  g_return_if_fail (view < VIEWS);

  if (g_path_is_absolute (uri))
    file = g_file_new_for_path (uri);
  else
    file = g_file_new_for_uri (uri);

  create_cached_background (backgrounds,
                            file,
                            view,
                            TRUE,
                            TRUE);

  g_object_unref (file);

  if (done_callback)
    hd_command_thread_pool_push_idle (priv->thread_pool,
                                      G_PRIORITY_HIGH_IDLE,
                                      done_callback,
                                      cb_data,
                                      destroy_data);
}

void
hd_backgrounds_set_image_set   (HDBackgrounds *backgrounds,
                                gchar        **uris,
                                GSourceFunc    done_callback,
                                gpointer       cb_data,
                                GDestroyNotify destroy_data)
{
  HDBackgroundsPrivate *priv = backgrounds->priv;
  guint i;

  g_return_if_fail (HD_IS_BACKGROUNDS (backgrounds));

  for (i = 0; i < VIEWS; i++)
    {
      GFile *file;

      if (g_path_is_absolute (uris[i]))
        file = g_file_new_for_path (uris[i]);
      else
        file = g_file_new_for_uri (uris[i]);

      create_cached_background (backgrounds,
                                file,
                                i,
                                TRUE,
                                TRUE);

      g_object_unref (file);
    }

  if (done_callback)
    hd_command_thread_pool_push_idle (priv->thread_pool,
                                      G_PRIORITY_HIGH_IDLE,
                                      done_callback,
                                      cb_data,
                                      destroy_data);
}

const gchar *
hd_backgrounds_get_background (HDBackgrounds *backgrounds,
                               guint          view)
{
  HDBackgroundsPrivate *priv;

  g_return_val_if_fail (HD_IS_BACKGROUNDS (backgrounds), NULL);
  g_return_val_if_fail (view < VIEWS, NULL);

  priv = backgrounds->priv;

  return g_file_get_path (priv->current_bg_image[view]);
}
