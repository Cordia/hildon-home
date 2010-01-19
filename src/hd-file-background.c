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

#include <glib/gi18n.h>

#include "hd-backgrounds.h"
#include "hd-desktop.h"
#include "hd-object-vector.h"
#include "hd-pixbuf-utils.h"

#include "hd-file-background.h"

struct _HDFileBackgroundPrivate
{
  GFile *image_file;
};

enum
{
  PROP_0,
  PROP_IMAGE_FILE
};

typedef struct
{
  GFile *file;
  guint  view;
  GCancellable *cancellable;
  gboolean error_dialogs;
  gboolean update_gconf;
} CommandData;

#define HD_FILE_BACKGROUND_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_FILE_BACKGROUND, HDFileBackgroundPrivate))

static void hd_file_background_set_for_current_view (HDBackground   *background,
                                                     guint           view,
                                                     GCancellable   *cancellable);
static GFile * hd_file_background_get_image_file_for_view (HDBackground *background,
                                                           guint         view);
static void create_cached_image_command (CommandData *data);

static void hd_file_background_dispose      (GObject *object);
static void hd_file_background_get_property (GObject      *object,
                                             guint         prop_id,
                                             GValue       *value,
                                             GParamSpec   *pspec);
static void hd_file_background_set_property (GObject      *object,
                                             guint         prop_id,
                                             const GValue *value,
                                             GParamSpec   *pspec);

static CommandData* command_data_new  (GFile        *file,
                                       guint         view,
                                       GCancellable *cancellable,
                                       gboolean      error_dialogs,
                                       gboolean      update_gconf);
static void         command_data_free (CommandData  *data);

G_DEFINE_TYPE (HDFileBackground, hd_file_background, HD_TYPE_BACKGROUND);

static void
hd_file_background_class_init (HDFileBackgroundClass *klass)
{
  HDBackgroundClass *background_class = HD_BACKGROUND_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  background_class->set_for_current_view = hd_file_background_set_for_current_view;
  background_class->get_image_file_for_view = hd_file_background_get_image_file_for_view;

  object_class->dispose = hd_file_background_dispose;
  object_class->get_property = hd_file_background_get_property;
  object_class->set_property = hd_file_background_set_property;

  g_object_class_install_property (object_class,
                                   PROP_IMAGE_FILE,
                                   g_param_spec_object ("image-file",
                                                        "Image File",
                                                        "The background file",
                                                        G_TYPE_FILE,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private (klass, sizeof (HDFileBackgroundPrivate));
}

static void
hd_file_background_set_for_current_view (HDBackground   *background,
                                         guint           current_view,
                                         GCancellable   *cancellable)
{
  hd_file_background_set_for_view_full (HD_FILE_BACKGROUND (background),
                                        current_view,
                                        cancellable,
                                        TRUE,
                                        TRUE);
}

void
hd_file_background_set_for_view_full (HDFileBackground *background,
                                      guint             current_view,
                                      GCancellable     *cancellable,
                                      gboolean          error_dialogs,
                                      gboolean          update_gconf)
{
  HDFileBackgroundPrivate *priv = HD_FILE_BACKGROUND (background)->priv;
  CommandData *data;

  data = command_data_new (priv->image_file,
                           current_view,
                           cancellable,
                           error_dialogs,
                           update_gconf);

  hd_backgrounds_add_create_cached_image (hd_backgrounds_get (),
                                          priv->image_file,
                                          data->error_dialogs,
                                          cancellable,
                                          (HDCommandCallback) create_cached_image_command,
                                          data,
                                          (GDestroyNotify) command_data_free);
}

static GFile *
hd_file_background_get_image_file_for_view (HDBackground *background,
                                            guint         view)
{
  HDFileBackgroundPrivate *priv = HD_FILE_BACKGROUND (background)->priv;

  return priv->image_file;
}

static void
create_cached_image_command (CommandData *data)
{
  HDImageSize screen_size = {HD_SCREEN_WIDTH, HD_SCREEN_HEIGHT};
  GdkPixbuf *pixbuf = NULL;
  char *etag = NULL;
  GError *error = NULL;

  if (g_cancellable_is_cancelled (data->cancellable))
    goto cleanup;

  pixbuf = hd_pixbuf_utils_load_scaled_and_cropped (data->file,
                                                    &screen_size,
                                                    &etag,
                                                    data->cancellable,
                                                    &error);
  if (error)
    {
      char *uri;

      if (data->error_dialogs)
        hd_backgrounds_report_corrupt_image (error);

      uri = g_file_get_uri (data->file);
      g_warning ("%s. Could not load pixbuf from file %s. %s",
                 __FUNCTION__,
                 uri,
                 error->message);
      g_free (uri);
      g_error_free (error);
      goto cleanup;
    }

  if (!pixbuf)
    goto cleanup;

  if (g_cancellable_is_cancelled (data->cancellable))
    goto cleanup;

  hd_backgrounds_save_cached_image (hd_backgrounds_get (),
                                    pixbuf,
                                    data->view,
                                    data->file,
                                    etag,
                                    data->error_dialogs,
                                    data->update_gconf,
                                    data->cancellable,
                                    &error);

  if (error)
    g_error_free (error);

cleanup:
  if (pixbuf)
    g_object_unref (pixbuf);
  g_free (etag);
}

static void
hd_file_background_init (HDFileBackground *background)
{
  background->priv = HD_FILE_BACKGROUND_GET_PRIVATE (background);
}

HDBackground *
hd_file_background_new (GFile *image_file)
{
  return g_object_new (HD_TYPE_FILE_BACKGROUND,
                       "image-file", image_file,
                       NULL);
}

static char *
get_display_label (GFile *file)
{
  char *path, *basename, *name_without_extension;
  const char *last_dot;

  path = g_file_get_path (file);
  basename = g_filename_display_basename (path);

  last_dot = g_strrstr (basename, ".");
  name_without_extension = g_strndup (basename,
                                      last_dot - basename);

  g_free (path);
  g_free (basename);

  return name_without_extension;
}

void
hd_file_background_add_to_store (HDFileBackground       *background,
                                 HDAvailableBackgrounds *backgrounds)
{
  HDFileBackgroundPrivate *priv = background->priv;
  char *label;

  label = get_display_label (priv->image_file);

  hd_available_backgrounds_add_with_file (backgrounds,
                                          HD_BACKGROUND (background),
                                          label,
                                          priv->image_file);

  g_free (label);
}

char *
hd_file_background_get_label (HDFileBackground *background)
{
  HDFileBackgroundPrivate *priv = background->priv;

  return get_display_label (priv->image_file);
}

GFile *
hd_file_background_get_image_file (HDFileBackground *background)
{
  HDFileBackgroundPrivate *priv = background->priv;

  return priv->image_file;
}

static void
hd_file_background_dispose (GObject *object)
{
  HDFileBackgroundPrivate *priv = HD_FILE_BACKGROUND (object)->priv;

  if (priv->image_file)
    priv->image_file = (g_object_unref (priv->image_file), NULL);

  G_OBJECT_CLASS (hd_file_background_parent_class)->dispose (object);
}

static void
hd_file_background_get_property (GObject      *object,
                                 guint         prop_id,
                                 GValue       *value,
                                 GParamSpec   *pspec)
{
  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
hd_file_background_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  HDFileBackgroundPrivate *priv = HD_FILE_BACKGROUND (object)->priv;

  switch (prop_id)
    {
    case PROP_IMAGE_FILE:
      priv->image_file = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static CommandData*
command_data_new  (GFile        *file,
                   guint         view,
                   GCancellable *cancellable,
                   gboolean      error_dialogs,
                   gboolean      update_gconf)
{
  CommandData *data = g_slice_new0 (CommandData);

  data->file = g_object_ref (file);
  data->view = view;
  data->cancellable = cancellable ? g_object_ref (cancellable) : NULL;

  data->error_dialogs = error_dialogs;
  data->update_gconf = update_gconf;

  return data;
}

static void
command_data_free (CommandData *data)
{
  if (!data)
    return;

  g_object_unref (data->file);

  if (data->cancellable)
    g_object_unref (data->cancellable);

  g_slice_free (CommandData, data);
}

