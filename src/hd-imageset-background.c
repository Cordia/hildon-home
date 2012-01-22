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
#include <gconf/gconf-client.h>

#include "hd-backgrounds.h"
#include "hd-desktop.h"
#include "hd-object-vector.h"
#include "hd-pixbuf-utils.h"

#include "hd-imageset-background.h"

struct _HDImagesetBackgroundPrivate
{
  GFile *desktop_file;
  HDObjectVector *image_files;
  char *name;
};

enum
{
  PROP_0,
  PROP_DESKTOP_FILE,
  PROP_IMAGE_FILES
};

typedef struct
{
  GFile *file;
  guint  view;
  GCancellable *cancellable;
} CommandData;

/* folders */
#define FOLDER_USER_IMAGES "MyDocs", ".images"
#define FOLDER_SHARE_BACKGROUND "/usr/share/backgrounds"

/* .desktop file keys */
#define KEY_FILE_BACKGROUND_VALUE_TYPE "Background Image"

/* gconf key */
#define HD_GCONF_KEY_ACTIVE_VIEWS "/apps/osso/hildon-desktop/views/active"

#define HD_IMAGESET_BACKGROUND_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_IMAGESET_BACKGROUND, HDImagesetBackgroundPrivate))

static void hd_imageset_background_set_for_current_view (HDBackground   *background,
                                                         guint           view,
                                                         GCancellable   *cancellable);
static GFile * hd_imageset_background_get_image_file_for_view (HDBackground *background,
                                                               guint         view);
static void create_cached_image_command (CommandData *data);

static void hd_imageset_background_dispose      (GObject *object);
static void hd_imageset_background_get_property (GObject      *object,
                                                 guint         prop_id,
                                                 GValue       *value,
                                                 GParamSpec   *pspec);
static void hd_imageset_background_set_property (GObject      *object,
                                                 guint         prop_id,
                                                 const GValue *value,
                                                 GParamSpec   *pspec);

static void load_desktop_file (GFile        *file,
                               GAsyncResult *result,
                               GAsyncResult *init_result);
static void get_imagesets_from_folder (GFile                  *folder,
                                       HDAvailableBackgrounds *backgrounds);
static void imageset_loaded (HDImagesetBackground   *background,
                             GAsyncResult           *result,
                             HDAvailableBackgrounds *backgrounds);

static CommandData* command_data_new  (GFile        *file,
                                       guint         view,
                                       GCancellable *cancellable);
static void         command_data_free (CommandData  *data);

G_DEFINE_TYPE (HDImagesetBackground, hd_imageset_background, HD_TYPE_BACKGROUND);

static void
hd_imageset_background_class_init (HDImagesetBackgroundClass *klass)
{
  HDBackgroundClass *background_class = HD_BACKGROUND_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  background_class->set_for_current_view = hd_imageset_background_set_for_current_view;
  background_class->get_image_file_for_view = hd_imageset_background_get_image_file_for_view;

  object_class->dispose = hd_imageset_background_dispose;
  object_class->get_property = hd_imageset_background_get_property;
  object_class->set_property = hd_imageset_background_set_property;

  g_object_class_install_property (object_class,
                                   PROP_DESKTOP_FILE,
                                   g_param_spec_object ("desktop-file",
                                                        "Desktop File",
                                                        ".desktop image set background file",
                                                        G_TYPE_FILE,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private (klass, sizeof (HDImagesetBackgroundPrivate));
}

static void
hd_imageset_background_set_for_current_view (HDBackground   *background,
                                             guint           current_view,
                                             GCancellable   *cancellable)
{
  HDImagesetBackgroundPrivate *priv = HD_IMAGESET_BACKGROUND (background)->priv;
  guint view, size = hd_object_vector_size (priv->image_files);
  CommandData *data;

  gboolean error_dialogs = TRUE;

  for (view = 0; view < size; view++)
    {
      GFile *image_file = hd_object_vector_at (priv->image_files,
                                               view);

      data = command_data_new (image_file,
                               view,
                               cancellable);

      hd_backgrounds_add_create_cached_image (hd_backgrounds_get (),
                                              image_file,
                                              error_dialogs,
                                              cancellable,
                                              (HDCommandCallback) create_cached_image_command,
                                              data,
                                              (GDestroyNotify) command_data_free);
    }
}

static void
create_cached_image_command (CommandData *data)
{
  HDImageSize screen_size = {HD_SCREEN_WIDTH, HD_SCREEN_HEIGHT};
  GdkPixbuf *pixbuf = NULL;
  char *etag = NULL;
  GError *error = NULL;

  gboolean error_dialogs = TRUE, update_gconf = TRUE;

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

      if (error_dialogs)
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
                                    error_dialogs,
                                    update_gconf,
                                    data->cancellable,
                                    &error);

  if (error)
    g_error_free (error);

cleanup:
  if (pixbuf)
    g_object_unref (pixbuf);
  g_free (etag);
}

static GFile *
hd_imageset_background_get_image_file_for_view (HDBackground *background,
                                                guint         view)
{
  HDImagesetBackgroundPrivate *priv = HD_IMAGESET_BACKGROUND (background)->priv;

  return hd_object_vector_at (priv->image_files, view);
}

static void
hd_imageset_background_init (HDImagesetBackground *background)
{
  background->priv = HD_IMAGESET_BACKGROUND_GET_PRIVATE (background);

  background->priv->image_files = hd_object_vector_new ();
}

HDBackground *
hd_imageset_background_new (GFile *file)
{
  return g_object_new (HD_TYPE_IMAGESET_BACKGROUND,
                       "desktop-file", file,
                       NULL);
}

void
hd_imageset_background_get_available (HDAvailableBackgrounds *backgrounds)
{
  GFile *folder;
  char *user_images_path;

  /* /usr/share/backgrounds */
  folder = g_file_new_for_path (FOLDER_SHARE_BACKGROUND);
  get_imagesets_from_folder (folder,
                             backgrounds);
  g_object_unref (folder);
  
  /* /home/user/MyDocs/.images */
  user_images_path = g_build_filename (g_get_home_dir (),
                                       FOLDER_USER_IMAGES,
                                       NULL);
  folder = g_file_new_for_path (user_images_path);
  get_imagesets_from_folder (folder,
                             backgrounds);
  g_free (user_images_path);
  g_object_unref (folder);
}

static void
get_imagesets_from_folder (GFile                  *folder,
                           HDAvailableBackgrounds *backgrounds)
{
  GFileEnumerator *enumerator;
  GFileInfo *info;
  GError *error = NULL;

  enumerator = g_file_enumerate_children (folder,
                                          G_FILE_ATTRIBUTE_STANDARD_NAME,
                                          G_FILE_QUERY_INFO_NONE,
                                          NULL,
                                          &error);

  if (error)
    {
      char *path = g_file_get_path (folder);
      g_warning ("%s. Could not enumerate folder %s. %s",
                 __FUNCTION__,
                 path,
                 error->message);
      g_free (path);
      g_error_free (error);
      return;
    }

  do
    {
      info = g_file_enumerator_next_file (enumerator,
                                          NULL,
                                          &error);
      if (error)
        {
          char *path = g_file_get_path (folder);
          g_warning ("%s. Error enumerating folder %s. %s",
                     __FUNCTION__,
                     path,
                     error->message);
          g_free (path);
          g_error_free (error);
          goto cleanup;
        }

      if (info)
        {
          const char *name;

          name = g_file_info_get_name (info);

          if (g_str_has_suffix (name, ".desktop"))
            {
              HDBackground *background;
              GFile *desktop_file = g_file_get_child (folder,
                                                      name);
              background = hd_imageset_background_new (desktop_file);
              hd_imageset_background_init_async (HD_IMAGESET_BACKGROUND (background),
                                                 NULL,
                                                 (GAsyncReadyCallback) imageset_loaded,
                                                 backgrounds);
            }

          g_object_unref (info);
        }
    }
  while (info);

cleanup:
  g_object_unref (enumerator);
}

static char *
get_display_label (const char *name)
{
  char *display_label;

  display_label = g_strdup_printf ("%s - %s",
                                   dgettext ("maemo-af-desktop",
                                             "home_ia_imageset_prefix"),
                                   name);

  return display_label;
}


static void
imageset_loaded (HDImagesetBackground   *background,
                 GAsyncResult           *result,
                 HDAvailableBackgrounds *backgrounds)
{
  HDImagesetBackgroundPrivate *priv = background->priv;
  GError *error = NULL;

  if (hd_imageset_background_init_finish (background,
                                          result,
                                          &error))
    {
      char *label;

      label = get_display_label (priv->name);

      hd_available_backgrounds_add_with_file (backgrounds,
                                              HD_BACKGROUND (background),
                                              label,
                                              hd_object_vector_at (priv->image_files,
                                                                  0));

      g_free (label);
    }
}

void
hd_imageset_background_init_async (HDImagesetBackground *background,
                                   GCancellable         *cancellable,
                                   GAsyncReadyCallback   callback,
                                   gpointer              user_data)
{
  HDImagesetBackgroundPrivate *priv = background->priv;
  GSimpleAsyncResult *result;

  result = g_simple_async_result_new (G_OBJECT (background),
                                      callback,
                                      user_data,
                                      hd_imageset_background_init_async);

  g_file_load_contents_async (priv->desktop_file,
                              cancellable,
                              (GAsyncReadyCallback) load_desktop_file,
                              result);
}

/*
static guint
get_count_active_views()
{
  GConfClient* gconf_client = gconf_client_get_default();
  guint ret = 1;
  if (GCONF_IS_CLIENT(gconf_client))
  {
    GSList* list = gconf_client_get_list (gconf_client, HD_GCONF_KEY_ACTIVE_VIEWS, GCONF_VALUE_INT, NULL);
    if(list)
    {
      ret = g_slist_length(list);
      g_slist_free(list);
    }
    g_object_unref(gconf_client);
  }
  return ret;
}*/

static void
load_desktop_file (GFile        *file,
                   GAsyncResult *result,
                   GAsyncResult *init_result)
{
  HDImagesetBackground *background = HD_IMAGESET_BACKGROUND (g_async_result_get_source_object (init_result));
  HDImagesetBackgroundPrivate *priv = background->priv;
  char *file_contents = NULL;
  gsize file_size;
  char *etag;
  GKeyFile *key_file = NULL;
  guint i;
  GError *error = NULL;
  char *type = NULL;
  g_file_load_contents_finish (file,
                               result,
                               &file_contents,
                               &file_size,
                               &etag,
                               &error);
  if (error)
    {
      g_simple_async_result_set_from_error (G_SIMPLE_ASYNC_RESULT (init_result),
                                            error);
      g_error_free (error);
      goto complete;
    }
  
  key_file = g_key_file_new ();
  g_key_file_load_from_data (key_file,
                             file_contents,
                             file_size,
                             G_KEY_FILE_NONE,
                             &error);
  if (error)
    {
      g_simple_async_result_set_from_error (G_SIMPLE_ASYNC_RESULT (init_result),
                                            error);
      g_error_free (error);
      goto complete;
    }

  type = g_key_file_get_string (key_file,
                                G_KEY_FILE_DESKTOP_GROUP,
                                G_KEY_FILE_DESKTOP_KEY_TYPE,
                                &error);
  if (error)
    {
      g_simple_async_result_set_from_error (G_SIMPLE_ASYNC_RESULT (init_result),
                                            error);
      g_error_free (error);
      goto complete;
    }
  else if (g_strcmp0 (type, KEY_FILE_BACKGROUND_VALUE_TYPE) != 0)
    {
      g_simple_async_result_set_error (G_SIMPLE_ASYNC_RESULT (init_result),
                                       G_IO_ERROR,
                                       G_IO_ERROR_FAILED,
                                       "Not a valid imageset .desktop file. Type needs to be Background Image");
    }

  priv->name = g_key_file_get_string (key_file,
                                      G_KEY_FILE_DESKTOP_GROUP,
                                      G_KEY_FILE_DESKTOP_KEY_NAME,
                                      &error);
  if (error)
    {
      g_simple_async_result_set_from_error (G_SIMPLE_ASYNC_RESULT (init_result),
                                            error);
      g_error_free (error);
      goto complete;
    }

  int max_value = HD_DESKTOP_VIEWS;  

  if(hd_backgrounds_is_portrait_wallpaper_enabled (hd_backgrounds_get ()))
    max_value += HD_DESKTOP_VIEWS;

  for (i = 0; i < max_value; i++)
    {
      gchar *key, *value;
      GFile *image_file;

      if (i >= HD_DESKTOP_VIEWS)
        key = g_strdup_printf ("X-Portrait-File%u", i + 1 - HD_DESKTOP_VIEWS);
      else
        key = g_strdup_printf ("X-File%u", i + 1);

      value = g_key_file_get_string  (key_file,
                                      G_KEY_FILE_DESKTOP_GROUP,
                                      key,
                                      &error);
      g_free (key);

      if (error)
        {
          g_error_free (error);
          error = NULL;          
          g_free (value);
          continue;
        }

      g_strstrip (value);
      if (g_path_is_absolute (value))
        image_file = g_file_new_for_path (value);
      else
        {
          GFile *desktop_parent = g_file_get_parent (file);
          image_file = g_file_get_child (desktop_parent,
                                         value);
          g_object_unref (desktop_parent);
        }

      g_free (value);

      if (g_file_query_exists (image_file,
                               NULL))
        {
          hd_object_vector_push_back (priv->image_files,
                                      image_file);
          g_object_unref (image_file);
        }
      else
        {
          char *path = g_file_get_path (image_file);
          g_simple_async_result_set_error (G_SIMPLE_ASYNC_RESULT (init_result),
                                           G_IO_ERROR,
                                           G_IO_ERROR_NOT_FOUND,
                                           "Could not find file %s",
                                           path);
          g_object_unref (image_file);
          g_free (path);
          goto complete;
        }
    }

  g_simple_async_result_set_op_res_gboolean (G_SIMPLE_ASYNC_RESULT (init_result),
                                             TRUE);

complete:
  g_free (file_contents);
  g_free (type);
  if (key_file)
    g_key_file_free (key_file);

  g_simple_async_result_complete (G_SIMPLE_ASYNC_RESULT (init_result));
}

gboolean
hd_imageset_background_init_finish (HDImagesetBackground  *background,
                                    GAsyncResult          *result,
                                    GError               **error)
{
  
  g_return_val_if_fail (g_simple_async_result_is_valid (result,
                                                        G_OBJECT (background),
                                                        hd_imageset_background_init_async),
                        FALSE);

  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
                                             error))
    return FALSE;

  return g_simple_async_result_get_op_res_gboolean (G_SIMPLE_ASYNC_RESULT (result));
}

static void
hd_imageset_background_dispose (GObject *object)
{
  HDImagesetBackgroundPrivate *priv = HD_IMAGESET_BACKGROUND (object)->priv;

  if (priv->desktop_file)
    priv->desktop_file = (g_object_unref (priv->desktop_file), NULL);

  if (priv->image_files)
    priv->image_files = (g_object_unref (priv->image_files), NULL);

  G_OBJECT_CLASS (hd_imageset_background_parent_class)->dispose (object);
}

static void
hd_imageset_background_get_property (GObject      *object,
                                     guint         prop_id,
                                     GValue       *value,
                                     GParamSpec   *pspec)
{
  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
hd_imageset_background_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  HDImagesetBackgroundPrivate *priv = HD_IMAGESET_BACKGROUND (object)->priv;

  switch (prop_id)
    {
    case PROP_DESKTOP_FILE:
      priv->desktop_file = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static CommandData*
command_data_new  (GFile        *file,
                   guint         view,
                   GCancellable *cancellable)
{
  CommandData *data = g_slice_new0 (CommandData);

  data->file = g_object_ref (file);
  data->view = view;
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

