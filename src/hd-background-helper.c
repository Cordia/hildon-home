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

#include "hd-background-helper.h"

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 480

static gboolean
is_valid_result (GAsyncResult *result,
                 GObject      *source,
                 gpointer      tag)
{
  GSimpleAsyncResult *simple;
  GObject *res_source;

  if (!G_IS_SIMPLE_ASYNC_RESULT (result))
    return FALSE;

  simple = (GSimpleAsyncResult *) result;

  res_source = g_async_result_get_source_object (result);
  if (source != res_source)
    {
      g_object_unref (res_source);
      return FALSE;
    }
  g_object_unref (res_source);

  return tag == g_simple_async_result_get_source_tag (simple);
}

static void
size_prepared_cb (GdkPixbufLoader *loader,
                  gint             width,
                  gint             height,
                  gpointer         user_data)
{
  double w, h;

  /*
   * Background image should be resized and cropped. That means the image
   * is centered and scaled to make sure the shortest side fit the home 
   * background exactly with keeping the aspect ratio of the original image
   */

  /* FIXME
  w = 1. * width / SCREEN_WIDTH;
  h = 1. * height / SCREEN_HEIGHT;

  if (w >= h)
    {
      width = SCREEN_WIDTH * 1. / h;
      height = SCREEN_HEIGHT;
    }
  else
    {
      width = SCREEN_WIDTH;
      height = SCREEN_HEIGHT * 1. / w;
    }
    */

  w = SCREEN_WIDTH;
  h = SCREEN_HEIGHT;

  gdk_pixbuf_loader_set_size (loader, w, h);
}

static void
read_pixbuf_async_thread (GSimpleAsyncResult *res,
                          GObject            *object,
                          GCancellable       *cancellable)
{
  GFileInputStream *stream = NULL;
  GdkPixbufLoader *loader = NULL;
  guchar buffer[8192];
  gssize read_bytes;
  GdkPixbuf *pixbuf;
  GError *error = NULL;

  /* Open file for read */
  stream = g_file_read (G_FILE (object), cancellable, &error);

  if (!stream)
    {
      g_simple_async_result_set_from_error (res, error);
      g_error_free (error);
      goto cleanup;
    }

  /* Create pixbuf loader */
  loader = gdk_pixbuf_loader_new ();
  g_signal_connect (loader, "size-prepared",
                    G_CALLBACK (size_prepared_cb), NULL);

  /* Parse input stream into the loader */
  do
    {
      read_bytes = g_input_stream_read (G_INPUT_STREAM (stream),
                                        buffer,
                                        sizeof (buffer),
                                        cancellable,
                                        &error);
      if (read_bytes < 0)
        {
          g_simple_async_result_set_from_error (res, error);
          g_error_free (error);
          gdk_pixbuf_loader_close (loader, NULL);
          goto cleanup;
        }

      if (!gdk_pixbuf_loader_write (loader,
                                    buffer,
                                    read_bytes,
                                    &error))
        {
          g_simple_async_result_set_from_error (res, error);
          g_error_free (error);
          gdk_pixbuf_loader_close (loader, NULL);
          goto cleanup;
        }
    } while (read_bytes > 0);

  /* Close pixbuf loader */
  if (!gdk_pixbuf_loader_close (loader, &error))
    {
      g_simple_async_result_set_from_error (res, error);
      g_error_free (error);
      goto cleanup;
    }

  /* Close input stream */
  if (!g_input_stream_close (G_INPUT_STREAM (stream),
                             cancellable,
                             &error))
    {
      g_simple_async_result_set_from_error (res, error);
      g_error_free (error);
      goto cleanup;
    }

  /* Set resulting pixbuf */
  pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
  if (pixbuf)
    g_object_ref (pixbuf);
  g_simple_async_result_set_op_res_gpointer (res,
                                             pixbuf,
                                             g_object_unref);

cleanup:
  if (stream)
    g_object_unref (stream);
  if (loader)
    g_object_unref (loader);
}

void
hd_background_helper_read_pixbuf_async (GFile               *file,
                                        int                  io_priority,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  GSimpleAsyncResult *res;

  res = g_simple_async_result_new (G_OBJECT (file),
                                   callback,
                                   user_data,
                                   hd_background_helper_read_pixbuf_async);

  g_simple_async_result_set_handle_cancellation (res, TRUE);

  g_simple_async_result_run_in_thread (res,
                                       read_pixbuf_async_thread,
                                       io_priority,
                                       cancellable);
  g_object_unref (res);
}

GdkPixbuf *
hd_background_helper_read_pixbuf_finish (GFile         *file,
                                         GAsyncResult  *res,
                                         GError       **error)
{
  GSimpleAsyncResult *simple;
  gpointer op;

  g_return_val_if_fail (is_valid_result (res,
                                         G_OBJECT (file),
                                         hd_background_helper_read_pixbuf_async),
                        NULL);

  simple = (GSimpleAsyncResult *) res;

  if (g_simple_async_result_propagate_error (simple, error))
    return NULL;

  op = g_simple_async_result_get_op_res_gpointer (simple);
  if (op)
    return g_object_ref (op);

  return NULL;
}

static void
save_pixbuf_async_thread (GSimpleAsyncResult *res,
                          GObject            *object,
                          GCancellable       *cancellable)
{
  GdkPixbuf *pixbuf = NULL;
  GFileOutputStream *stream = NULL;
  GError *error = NULL;
  gchar *buffer;
  gsize buffer_size;

  /* Get Pixbuf */
  pixbuf = g_simple_async_result_get_op_res_gpointer (res);
  g_simple_async_result_set_op_res_gpointer (res, NULL, NULL);

  if (!pixbuf)
    {
      g_simple_async_result_set_error (res,
                                       GDK_PIXBUF_ERROR,
                                       GDK_PIXBUF_ERROR_FAILED,
                                       "Cannot store a NULL pixbuf");
      goto cleanup;
    }

  /* Open file for write */
  stream = g_file_replace (G_FILE (object),
                           NULL,
                           FALSE,
                           G_FILE_CREATE_NONE,
                           cancellable,
                           &error);

  if (!stream)
    {
      g_simple_async_result_set_from_error (res, error);
      g_error_free (error);
      goto cleanup;
    }

  if (!gdk_pixbuf_save_to_buffer (pixbuf,
                                  &buffer,
                                  &buffer_size,
                                  "png",
                                  &error,
                                  NULL))
    {
      g_simple_async_result_set_from_error (res, error);
      g_error_free (error);
      goto cleanup;
    }

  if (!g_output_stream_write (G_OUTPUT_STREAM (stream),
                              buffer,
                              buffer_size,
                              cancellable,
                              &error))
    {
      g_simple_async_result_set_from_error (res, error);
      g_error_free (error);
      goto cleanup;
    }

  /* Close output stream */
  if (!g_output_stream_close (G_OUTPUT_STREAM (stream),
                              cancellable,
                              &error))
    {
      g_simple_async_result_set_from_error (res, error);
      g_error_free (error);
      goto cleanup;
    }

  g_simple_async_result_set_op_res_gboolean (res,
                                             TRUE);

cleanup:
  if (stream)
    g_object_unref (stream);
  if (pixbuf)
    g_object_unref (pixbuf);
}

void
hd_background_helper_save_pixbuf_async  (GFile               *file,
                                         GdkPixbuf           *pixbuf,
                                         int                  io_priority,
                                         GCancellable        *cancellable,
                                         GAsyncReadyCallback  callback,
                                         gpointer             user_data)
{
  GSimpleAsyncResult *res;

  res = g_simple_async_result_new (G_OBJECT (file),
                                   callback,
                                   user_data,
                                   hd_background_helper_save_pixbuf_async);

  if (pixbuf)
    g_simple_async_result_set_op_res_gpointer (res,
                                               g_object_ref (pixbuf),
                                               g_object_unref);

  g_simple_async_result_set_handle_cancellation (res, TRUE);

  g_simple_async_result_run_in_thread (res,
                                       save_pixbuf_async_thread,
                                       io_priority,
                                       cancellable);
  g_object_unref (res); 
}

gboolean
hd_background_helper_save_pixbuf_finish (GFile         *file,
                                         GAsyncResult  *result,
                                         GError       **error)
{
  GSimpleAsyncResult *simple;

  g_return_val_if_fail (is_valid_result (result,
                                         G_OBJECT (file),
                                         hd_background_helper_save_pixbuf_async),
                        FALSE);

  simple = G_SIMPLE_ASYNC_RESULT (result);

  return !g_simple_async_result_propagate_error (simple, error);
}

void
hd_background_helper_check_cache_async  (GFile                *file,
                                         guint                 view,
                                         int                   io_priority,
                                         GCancellable         *cancellable,
                                         GAsyncReadyCallback   callback,
                                         gpointer              user_data)
{
  GSimpleAsyncResult *res;

  res = g_simple_async_result_new (G_OBJECT (file),
                                   callback,
                                   user_data,
                                   hd_background_helper_check_cache_async);
  /* FIXME implement */

  g_simple_async_result_complete (res);
/*  callback (G_OBJECT (file), G_ASYNC_RESULT (res), user_data); */
}

gboolean
hd_background_helper_check_cache_finish (GFile                *file,
                                         GAsyncResult         *res,
                                         GError              **error)
{
  /* FIXME implement */

  return FALSE;
}
