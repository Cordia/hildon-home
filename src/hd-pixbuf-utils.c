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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "hd-pixbuf-utils.h"

static void
size_prepared_cb (GdkPixbufLoader *loader,
                  gint             width,
                  gint             height,
                  GdkRectangle    *desired_size)
{
  double desired_ratio, ratio, w, h;

  /*
   * Background image should be resized and cropped. That means the image
   * is centered and scaled to make sure the shortest side fit the home 
   * background exactly with keeping the aspect ratio of the original image
   */

  desired_ratio = (1. * desired_size->width) / desired_size->height;
  ratio = (1. * width) / height;

  if (ratio > desired_ratio)
    {
      h = desired_size->height;
      w = ratio * desired_size->height;
    }
  else
    {
      h = desired_size->width / ratio;
      w = desired_size->width;
    }

  gdk_pixbuf_loader_set_size (loader, w, h);
}

GdkPixbuf *
hd_pixbuf_utils_load_scaled_and_cropped (GFile         *file,
                                         gint           width,
                                         gint           height,
                                         GCancellable  *cancellable,
                                         GError       **error)
{
  GFileInputStream *stream = NULL;
  GdkPixbufLoader *loader = NULL;
  guchar buffer[8192];
  gssize read_bytes;
  GdkPixbuf *pixbuf = NULL;
  GdkRectangle *desired_size;

  /* Open file for read */
  stream = g_file_read (file, cancellable, error);

  if (!stream)
    goto cleanup;

  /* Create pixbuf loader */
  desired_size = g_new0 (GdkRectangle, 1);
  desired_size->width = width;
  desired_size->height = height;

  loader = gdk_pixbuf_loader_new ();
  g_signal_connect_data (loader, "size-prepared",
                         G_CALLBACK (size_prepared_cb),
                         desired_size,
                         (GClosureNotify) g_free,
                         0);

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
    {
      pixbuf = gdk_pixbuf_new_subpixbuf (pixbuf,
                                         (gdk_pixbuf_get_width (pixbuf) - width) / 2,
                                         (gdk_pixbuf_get_height (pixbuf) - height) / 2,
                                         width,
                                         height);
    }
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

gboolean
hd_pixbuf_utils_save (GFile         *file,
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
size_prepared_exact_cb (GdkPixbufLoader *loader,
                        gint             width,
                        gint             height,
                        GdkRectangle    *desired_size)
{
  if (width == desired_size->width &&
      height == desired_size->height)
    {
      gdk_pixbuf_loader_set_size (loader, width, height);
    }
  else
    {
      gdk_pixbuf_loader_set_size (loader,
                                  MIN (width, desired_size->width - 1),
                                  MIN (height, desired_size->height - 1));
    }
}


GdkPixbuf *
hd_pixbuf_utils_load_at_size (GFile         *file,
                              gint           width,
                              gint           height,
                              GCancellable  *cancellable,
                              GError       **error)
{
  GFileInputStream *stream = NULL;
  GdkPixbufLoader *loader = NULL;
  guchar buffer[8192];
  gssize read_bytes;
  GdkPixbuf *pixbuf = NULL;
  GdkRectangle *desired_size;

  /* Open file for read */
  stream = g_file_read (file, cancellable, error);

  if (!stream)
    goto cleanup;

  /* Create pixbuf loader */
  desired_size = g_new0 (GdkRectangle, 1);
  desired_size->width = width;
  desired_size->height = height;

  loader = gdk_pixbuf_loader_new ();
  g_signal_connect_data (loader, "size-prepared",
                         G_CALLBACK (size_prepared_exact_cb),
                         desired_size,
                         (GClosureNotify) g_free,
                         0);

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
  if (pixbuf && 
      gdk_pixbuf_get_width (pixbuf) == width &&
      gdk_pixbuf_get_height (pixbuf) == height)
    {
      g_object_ref (pixbuf);
    }
  else
    g_set_error_literal (error,
                         GDK_PIXBUF_ERROR,
                         GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                         "No pixbuf in the correct size");

cleanup:
  if (stream)
    g_object_unref (stream);
  if (loader)
    g_object_unref (loader);

  return pixbuf;
}
