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
#include "hd-pixbuf-utils.h"
#include "hd-search-service.h"

#include "hd-wallpaper-background.h"

#define WALLPAPER_WIDTH  (HD_DESKTOP_VIEWS * HD_SCREEN_WIDTH)
#define WALLPAPER_HEIGHT HD_SCREEN_HEIGHT

#define QUERY_RDFQ                      "" \
  "<rdfq:Condition>"                       \
  "<rdfq:equals>"                          \
  "<rdfq:Property name=\"Image:Width\" />" \
  "<rdf:Integer>3200</rdf:Integer>"        \
  "</rdfq:equals>"                         \
  "</rdfq:Condition>"

#define QUERY_SERVICE "Images"

struct _HDWallpaperBackgroundPrivate
{
  GFile *file;
};

enum
{
  PROP_0,
  PROP_FILE
};

typedef struct
{
  GFile *file;
  GCancellable *cancellable;
} CommandData;

#define HD_WALLPAPER_BACKGROUND_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_WALLPAPER_BACKGROUND, HDWallpaperBackgroundPrivate))

static void hd_wallpaper_background_set_for_current_view (HDBackground   *background,
                                                          guint           view,
                                                          GCancellable   *cancellable);
static GFile *hd_wallpaper_background_get_image_file_for_view (HDBackground *background,
                                                               guint         view);
static void create_cached_image_command (CommandData *data);

static void hd_wallpaper_background_dispose      (GObject *object);
static void hd_wallpaper_background_get_property (GObject      *object,
                                                  guint         prop_id,
                                                  GValue       *value,
                                                  GParamSpec   *pspec);
static void hd_wallpaper_background_set_property (GObject      *object,
                                                  guint         prop_id,
                                                  const GValue *value,
                                                  GParamSpec   *pspec);

static CommandData* command_data_new  (GFile        *file,
                                       GCancellable *cancellable);
static void         command_data_free (CommandData  *data);

G_DEFINE_TYPE (HDWallpaperBackground, hd_wallpaper_background, HD_TYPE_BACKGROUND);

static void
hd_wallpaper_background_class_init (HDWallpaperBackgroundClass *klass)
{
  HDBackgroundClass *background_class = HD_BACKGROUND_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  background_class->set_for_current_view = hd_wallpaper_background_set_for_current_view;
  background_class->get_image_file_for_view = hd_wallpaper_background_get_image_file_for_view;

  object_class->dispose = hd_wallpaper_background_dispose;
  object_class->get_property = hd_wallpaper_background_get_property;
  object_class->set_property = hd_wallpaper_background_set_property;

  g_object_class_install_property (object_class,
                                   PROP_FILE,
                                   g_param_spec_object ("file",
                                                        "File",
                                                        "Wallpaper background file",
                                                        G_TYPE_FILE,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private (klass, sizeof (HDWallpaperBackgroundPrivate));
}

static void
hd_wallpaper_background_set_for_current_view (HDBackground   *background,
                                              guint           view,
                                              GCancellable   *cancellable)
{
  HDWallpaperBackgroundPrivate *priv = HD_WALLPAPER_BACKGROUND (background)->priv;
  CommandData *data;

  gboolean error_dialogs = TRUE;
 
  data = command_data_new (priv->file,
                           cancellable);

  hd_backgrounds_add_create_cached_image (hd_backgrounds_get (),
                                          priv->file,
                                          error_dialogs,
                                          cancellable,
                                          (HDCommandCallback) create_cached_image_command,
                                          data,
                                          (GDestroyNotify) command_data_free);
}

static void
create_cached_image_command (CommandData *data)
{
  HDImageSize wallpaper_size = {WALLPAPER_WIDTH, WALLPAPER_HEIGHT};
  GdkPixbuf *pixbuf;
  char *etag;
  guint view;
  GError *error = NULL;

  gboolean error_dialogs = TRUE, update_gconf = TRUE;

  pixbuf = hd_pixbuf_utils_load_at_size (data->file,
                                         &wallpaper_size,
                                         &etag,
                                         data->cancellable,
                                         &error);

  if (!pixbuf)
    goto cleanup;

  for (view = 0; view < HD_DESKTOP_VIEWS; view++)
    {
      GdkPixbuf *sub = gdk_pixbuf_new_subpixbuf (pixbuf,
                                                 view * HD_SCREEN_WIDTH,
                                                 0,
                                                 HD_SCREEN_WIDTH,
                                                 HD_SCREEN_HEIGHT);

      hd_backgrounds_save_cached_image (hd_backgrounds_get (),
                                        sub,
                                        view,
                                        data->file,
                                        etag,
                                        error_dialogs,
                                        update_gconf,
                                        data->cancellable,
                                        &error);

      if (error)
        g_clear_error (&error);

      g_object_unref (sub);
    }

cleanup:
  if (pixbuf)
    g_object_unref (pixbuf);
  g_free (etag);
}

static void
hd_wallpaper_background_init (HDWallpaperBackground *background)
{
  background->priv = HD_WALLPAPER_BACKGROUND_GET_PRIVATE (background);
}

HDBackground *
hd_wallpaper_background_new (GFile *file)
{
  return g_object_new (HD_TYPE_WALLPAPER_BACKGROUND,
                       "file", file,
                       NULL);
}

static char *
get_display_label (GFile *file)
{
  char *path, *basename, *name_without_extension, *display_label;
  const char *last_dot;

  path = g_file_get_path (file);
  basename = g_filename_display_basename (path);

  last_dot = g_strrstr (basename, ".");
  name_without_extension = g_strndup (basename,
                                      last_dot - basename);

  display_label = g_strdup_printf ("%s - %s",
                                   dgettext ("maemo-af-desktop",
                                             "home_ia_imageset_prefix"),
                                   name_without_extension);

  g_free (path);
  g_free (basename);
  g_free (name_without_extension);

  return display_label;
}

static void
search_service_cb (HDSearchService        *service,
                   GAsyncResult           *result,
                   HDAvailableBackgrounds *backgrounds)
{
  GStrv filenames;
  gint i;
  GError *error = NULL;

  filenames = hd_search_service_query_finish (service, result, &error);

  if (error)
    {
      g_warning ("%s. Could not retrieve wallpapers from search service. %s",
                 __FUNCTION__,
                 error->message);
      g_error_free (error);
    }

  for (i = 0; filenames && filenames[i]; i++)
    {
      GFile *file;
      HDBackground *background;
      char *label;

      file = g_file_new_for_path (filenames[i]);
      background = hd_wallpaper_background_new (file);
      label = get_display_label (file);

      hd_available_backgrounds_add_with_file (backgrounds,
                                              background,
                                              label,
                                              file);

      g_object_unref (file);
      g_object_unref (background);
      g_free (label);
    }
}

void
hd_wallpaper_background_get_available (HDAvailableBackgrounds *backgrounds)
{
  HDSearchService *service = hd_search_service_new ();

  hd_search_service_query_async (service,
                                 QUERY_SERVICE,
                                 QUERY_RDFQ,
                                 NULL,
                                 (GAsyncReadyCallback) search_service_cb,
                                 backgrounds);
}

static void
hd_wallpaper_background_dispose (GObject *object)
{
  HDWallpaperBackgroundPrivate *priv = HD_WALLPAPER_BACKGROUND (object)->priv;

  if (priv->file)
    priv->file = (g_object_unref (priv->file), NULL);
  
  G_OBJECT_CLASS (hd_wallpaper_background_parent_class)->dispose (object);
}

static void
hd_wallpaper_background_get_property (GObject      *object,
                                      guint         prop_id,
                                      GValue       *value,
                                      GParamSpec   *pspec)
{
  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
hd_wallpaper_background_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  HDWallpaperBackgroundPrivate *priv = HD_WALLPAPER_BACKGROUND (object)->priv;

  switch (prop_id)
    {
    case PROP_FILE:
      priv->file = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static CommandData*
command_data_new  (GFile        *file,
                   GCancellable *cancellable)
{
  CommandData *data = g_slice_new0 (CommandData);

  data->file = g_object_ref (file);
  data->cancellable = cancellable ? g_object_ref (cancellable) : NULL;

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

static GFile *
hd_wallpaper_background_get_image_file_for_view (HDBackground *background,
                                                 guint         view)
{
  HDWallpaperBackgroundPrivate *priv = HD_WALLPAPER_BACKGROUND (background)->priv;

  return priv->file;
}

