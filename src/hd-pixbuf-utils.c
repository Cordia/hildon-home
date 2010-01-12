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

/*
 * Background image should be resized and cropped. That means the image
 * is centered and scaled to make sure the shortest side fit the home 
 * background exactly with keeping the aspect ratio of the original image
 */

static double
get_scale_for_aspect_ratio (HDImageSize *image_size,
                            HDImageSize *destination_size)
{
  double destination_ratio, ratio, scale;

  destination_ratio = (1. * destination_size->width) / destination_size->height;
  ratio = (1. * image_size->width) / image_size->height;

  if (ratio > destination_ratio)
    scale = ((double) destination_size->height) / ((double) image_size->height);
  else
    scale = ((double) destination_size->width) / ((double) image_size->width);

  return scale;
}

static void
size_prepared_cb (GdkPixbufLoader *loader,
                  gint             width,
                  gint             height,
                  HDImageSize     *size)
{
  HDImageSize image_size = {width, height};
  HDImageSize minimum_size;
  double scale;

  minimum_size.width = minimum_size.height = MAX (size->width, size->height);

  scale = get_scale_for_aspect_ratio (&image_size, &minimum_size);

  /* Do not load images bigger than neccesary */
  if (scale < 1)
    {
      image_size.width *= scale;
      image_size.height *= scale;
    }

  gdk_pixbuf_loader_set_size (loader, image_size.width, image_size.height);
}

static GdkPixbuf *
scale_and_crop_pixbuf (const GdkPixbuf *source,
                       HDImageSize     *destination_size)
{
  HDImageSize image_size;
  double scale;
  GdkPixbuf *pixbuf;

  image_size.width = gdk_pixbuf_get_width (source);
  image_size.height = gdk_pixbuf_get_height (source);

  scale = get_scale_for_aspect_ratio (&image_size, destination_size);

  pixbuf = gdk_pixbuf_new (gdk_pixbuf_get_colorspace (source),
                           gdk_pixbuf_get_has_alpha (source),
                           gdk_pixbuf_get_bits_per_sample (source),
                           destination_size->width,
                           destination_size->height);

  gdk_pixbuf_scale (source,
                    pixbuf,
                    0, 0,
                    destination_size->width,
                    destination_size->height,
                    - (image_size.width * scale - destination_size->width) / 2,
                    - (image_size.height * scale - destination_size->height) / 2,
                    scale,
                    scale,
                    GDK_INTERP_BILINEAR);

  return pixbuf;
}

static gboolean
read_from_input_stream_into_pixbuf_loader (GInputStream     *stream,
                                           GdkPixbufLoader  *loader,
                                           GCancellable     *cancellable,
                                           GError          **error)
{
  guchar buffer[8192];
  gssize read_bytes;

  /* Parse input stream into the loader */
  do
    {
      read_bytes = g_input_stream_read (stream,
                                        buffer,
                                        sizeof (buffer),
                                        cancellable,
                                        error);
      if (read_bytes < 0)
        {
          gdk_pixbuf_loader_close (loader, NULL);
          return FALSE;
        }

      if (!gdk_pixbuf_loader_write (loader,
                                    buffer,
                                    read_bytes,
                                    error))
        {
          gdk_pixbuf_loader_close (loader, NULL);
          return FALSE;
        }
    } while (read_bytes > 0);

  if (!gdk_pixbuf_loader_close (loader, error))
    return FALSE;

  if (!g_input_stream_close (stream,
                             cancellable,
                             error))
    return FALSE;

  return TRUE;
}

GdkPixbuf *
hd_pixbuf_utils_load_scaled_and_cropped (GFile         *file,
                                         HDImageSize   *size,
                                         GCancellable  *cancellable,
                                         GError       **error)
{
  GFileInputStream *stream = NULL;
  GdkPixbufLoader *loader = NULL;
  GdkPixbuf *pixbuf = NULL;

  /* Open file for read */
  stream = g_file_read (file, cancellable, error);

  if (!stream)
    goto cleanup;

  /* Create pixbuf loader */
  loader = gdk_pixbuf_loader_new ();
  g_signal_connect (loader, "size-prepared",
                    G_CALLBACK (size_prepared_cb),
                    size);

  if (!read_from_input_stream_into_pixbuf_loader (G_INPUT_STREAM (stream),
                                                  loader,
                                                  cancellable,
                                                  error))
    goto cleanup;

  /* Set resulting pixbuf */
  pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
  if (pixbuf)
    {
      GdkPixbuf *rotated = gdk_pixbuf_apply_embedded_orientation (pixbuf);
      pixbuf = scale_and_crop_pixbuf (rotated, size);
      g_object_unref (rotated);
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
                        HDImageSize     *destination_size)
{
  if (width == destination_size->width &&
      height == destination_size->height)
    {
      gdk_pixbuf_loader_set_size (loader, width, height);
    }
  else
    {
      gdk_pixbuf_loader_set_size (loader,
                                  MIN (width, destination_size->width - 1),
                                  MIN (height, destination_size->height - 1));
    }
}


GdkPixbuf *
hd_pixbuf_utils_load_at_size (GFile         *file,
                              HDImageSize   *size,
                              GCancellable  *cancellable,
                              GError       **error)
{
  GFileInputStream *stream = NULL;
  GdkPixbufLoader *loader = NULL;
  GdkPixbuf *pixbuf = NULL;

  /* Open file for read */
  stream = g_file_read (file, cancellable, error);

  if (!stream)
    goto cleanup;

  /* Create pixbuf loader */
  loader = gdk_pixbuf_loader_new ();
  g_signal_connect (loader, "size-prepared",
                    G_CALLBACK (size_prepared_exact_cb), size);

  if (!read_from_input_stream_into_pixbuf_loader (G_INPUT_STREAM (stream),
                                                  loader,
                                                  cancellable,
                                                  error))
    goto cleanup;

  /* Set resulting pixbuf */
  pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
  if (pixbuf && 
      gdk_pixbuf_get_width (pixbuf) == size->width &&
      gdk_pixbuf_get_height (pixbuf) == size->height)
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
