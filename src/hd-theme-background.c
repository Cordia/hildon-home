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
#include "hd-object-vector.h"
#include "hd-pixbuf-utils.h"

#include "hd-theme-background.h"

struct _HDThemeBackgroundPrivate
{
  GFile *theme_file;
};

enum
{
  PROP_0,
  PROP_THEME_FILE,
};

/* folders */
#define FOLDER_SHARE_THEMES "/usr/share/themes"

#define HD_THEME_BACKGROUND_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_THEME_BACKGROUND, HDThemeBackgroundPrivate))

static void hd_theme_background_dispose      (GObject *object);
static void hd_theme_background_get_property (GObject      *object,
                                              guint         prop_id,
                                              GValue       *value,
                                              GParamSpec   *pspec);
static void hd_theme_background_set_property (GObject      *object,
                                              guint         prop_id,
                                              const GValue *value,
                                              GParamSpec   *pspec);

static void get_themes_from_themes_dir (GFile                  *themes_dir,
                                        HDAvailableBackgrounds *backgrounds);
static void theme_loaded (HDImagesetBackground   *background,
                          GAsyncResult           *result,
                          HDAvailableBackgrounds *backgrounds);

G_DEFINE_TYPE (HDThemeBackground, hd_theme_background, HD_TYPE_IMAGESET_BACKGROUND);

static void
hd_theme_background_class_init (HDThemeBackgroundClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = hd_theme_background_dispose;
  object_class->get_property = hd_theme_background_get_property;
  object_class->set_property = hd_theme_background_set_property;

  g_object_class_install_property (object_class,
                                   PROP_THEME_FILE,
                                   g_param_spec_object ("theme-file",
                                                        "Theme File",
                                                        "Theme's index.theme file",
                                                        G_TYPE_FILE,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private (klass, sizeof (HDThemeBackgroundPrivate));
}

static void
hd_theme_background_init (HDThemeBackground *background)
{
  background->priv = HD_THEME_BACKGROUND_GET_PRIVATE (background);
}

HDBackground *
hd_theme_background_new (GFile *desktop_file,
                         GFile *theme_file)
{
  return g_object_new (HD_TYPE_THEME_BACKGROUND,
                       "desktop-file", desktop_file,
                       "theme-file", theme_file,
                       NULL);
}

void
hd_theme_background_get_available (HDAvailableBackgrounds *backgrounds)
{
  GFile *themes_dir;

  themes_dir = g_file_new_for_path (FOLDER_SHARE_THEMES);
  get_themes_from_themes_dir (themes_dir,
                              backgrounds);
  g_object_unref (themes_dir);
}

static void
get_themes_from_themes_dir (GFile                  *themes_dir,
                            HDAvailableBackgrounds *backgrounds)
{
  GFileEnumerator *enumerator;
  GFileInfo *info;
  GError *error = NULL;

  enumerator = g_file_enumerate_children (themes_dir,
                                          G_FILE_ATTRIBUTE_STANDARD_NAME,
                                          G_FILE_QUERY_INFO_NONE,
                                          NULL,
                                          &error);

  if (error)
    {
      char *path = g_file_get_path (themes_dir);
      g_warning ("%s. Could not enumerate themes_dir %s. %s",
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
          char *path = g_file_get_path (themes_dir);
          g_warning ("%s. Error enumerating themes_dir %s. %s",
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

          if (g_strcmp0 (name, "default") != 0)
            {
              HDBackground *background;
              GFile *theme_dir = g_file_get_child (themes_dir, name);
              GFile *desktop_file = g_file_resolve_relative_path (theme_dir,
                                                                  "./backgrounds/theme_bg.desktop");
              GFile *theme_file = g_file_get_child (theme_dir,
                                                    "index.theme");
              background = hd_theme_background_new (desktop_file,
                                                    theme_file);
              hd_imageset_background_init_async (HD_IMAGESET_BACKGROUND (background),
                                                 NULL,
                                                 (GAsyncReadyCallback) theme_loaded,
                                                 backgrounds);
              g_object_unref (theme_dir);
              g_object_unref (desktop_file);
              g_object_unref (theme_file);
            }

          g_object_unref (info);
        }
    }
  while (info);

cleanup:
  g_object_unref (enumerator);
}

static GKeyFile *
get_theme_key_file (HDThemeBackground *background)
{
  HDThemeBackgroundPrivate *priv = background->priv;
  GKeyFile *key_file = g_key_file_new ();
  char *contents = NULL;
  gsize length;
  GError *error = NULL;

  g_file_load_contents (priv->theme_file,
                        NULL,
                        &contents,
                        &length,
                        NULL,
                        &error);

  if (error)
    {
      g_warning ("%s. Could not load index.theme file. %s",
                 __FUNCTION__,
                 error->message);
      g_error_free (error);
      goto cleanup;
    }

  g_key_file_load_from_data (key_file,
                             contents,
                             length,
                             G_KEY_FILE_NONE,
                             &error);

  if (error)
    {
      g_warning ("%s. Could not load index.theme file. %s",
                 __FUNCTION__,
                 error->message);
      g_error_free (error);
      goto cleanup;
    }

cleanup:
  g_free (contents);

  return key_file;
}

static char *
get_display_label (const char *name)
{
  char *display_label;

  display_label = g_strdup_printf ("%s - %s",
                                   dgettext ("maemo-af-desktop",
                                             "home_ia_theme_prefix"),
                                   name);

  return display_label;
}

static char *
get_theme_name (GKeyFile *key_file)
{
  char *name;
  GError *error = NULL;

  name = g_key_file_get_string (key_file,
                                G_KEY_FILE_DESKTOP_GROUP,
                                G_KEY_FILE_DESKTOP_KEY_NAME,
                                &error);

  if (error)
    {
      g_warning ("%s. Could not get name from index.theme file. %s",
                 __FUNCTION__,
                 error->message);
      g_error_free (error);
    }

  return name;
}

static char *
get_theme_icon_path (GKeyFile *key_file)
{
  char *icon_path;
  GError *error = NULL;

  icon_path = g_key_file_get_string (key_file,
                                     G_KEY_FILE_DESKTOP_GROUP,
                                     G_KEY_FILE_DESKTOP_KEY_ICON,
                                     &error);

  if (error)
    {
      g_warning ("%s. Could not get icon from index.theme file. %s",
                 __FUNCTION__,
                 error->message);
      g_error_free (error);
    }

  return icon_path;
}

static GdkPixbuf *
get_theme_icon (GKeyFile *key_file)
{
  GdkPixbuf *icon = NULL;
  char *icon_path;
  GError *error = NULL;

  icon_path = get_theme_icon_path (key_file);

  if (!icon_path)
    return NULL;

  icon = gdk_pixbuf_new_from_file (icon_path,
                                   &error);

  if (error)
    {
      g_warning ("%s. Could not read thumbnail icon %s. %s",
                 __FUNCTION__,
                 icon_path,
                 error->message);
      g_error_free (error);
    }

  g_free (icon_path);

  return icon;
}

static void
theme_loaded (HDImagesetBackground   *background,
              GAsyncResult           *result,
              HDAvailableBackgrounds *backgrounds)
{
  GError *error = NULL;

  if (hd_imageset_background_init_finish (background,
                                          result,
                                          &error))
    {
      GKeyFile *key_file;
      char *name, *label;
      GdkPixbuf *icon;

      key_file = get_theme_key_file (HD_THEME_BACKGROUND (background));

      name = get_theme_name (key_file);
      icon = get_theme_icon (key_file);

      g_key_file_free (key_file);

      label = get_display_label (name);

      hd_available_backgrounds_add_with_icon (backgrounds,
                                              HD_BACKGROUND (background),
                                              label,
                                              icon);

      g_free (name);
      if (icon)
        g_object_unref (icon);
      g_free (label);
    }
}

static void
hd_theme_background_dispose (GObject *object)
{
  G_OBJECT_CLASS (hd_theme_background_parent_class)->dispose (object);
}

static void
hd_theme_background_get_property (GObject      *object,
                                  guint         prop_id,
                                  GValue       *value,
                                  GParamSpec   *pspec)
{
  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
hd_theme_background_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  HDThemeBackgroundPrivate *priv = HD_THEME_BACKGROUND (object)->priv;

  switch (prop_id)
    {
    case PROP_THEME_FILE:
      priv->theme_file = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

