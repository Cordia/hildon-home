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
#include "hd-pixbuf-utils.h"

#include "hd-backgrounds.h"

#define CACHED_DIR        ".backgrounds"
#define BACKGROUND_CACHED_PNG CACHED_DIR "/background-%u.png"
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

typedef struct
{
  GFile *file;
  gboolean error_dialogs;
  GCancellable *cancellable;
} CacheImageRequestData;

struct _HDBackgroundsPrivate
{
  GConfClient *gconf_client;

  /* GConf notify handlers */
  guint bg_image_notify[VIEWS];

  /* Data used for the thread which creates the cached images */
  HDCommandThreadPool *thread_pool;
  GMutex              *mutex;

  GPtrArray *requests;

  HDBackgroundData *current_requests[VIEWS];
  GFile            *current_bg_image[VIEWS];

  /* Theme change support */
  gchar *current_theme;
  guint set_theme_idle_id;

  GVolumeMonitor *volume_monitor;
  GnomeVFSVolumeMonitor *volume_monitor2;
};

static void create_cached_image (HDBackgroundData *data);

static CacheImageRequestData *cache_image_request_data_new (GFile        *file,
                                                            gboolean      error_dialogs,
                                                            GCancellable *cancellable);
static void cache_image_request_data_free (CacheImageRequestData *data);

static gboolean remove_request (CacheImageRequestData *request);

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
  GError *error = NULL;
  HDImageSize screen_size = {SCREEN_WIDTH, SCREEN_HEIGHT};

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
  pixbuf = hd_pixbuf_utils_load_scaled_and_cropped (data->file,
                                                    &screen_size,
                                                    data->cancellable,
                                                    &error);

  if (error)
    {
      gchar *uri;

      /* Display corrupt image notification banner */
      if (data->display_banner)
        hd_backgrounds_report_corrupt_image (error);

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

  if (!hd_backgrounds_save_cached_image (backgrounds,
                                         pixbuf,
                                         data->view,
                                         data->file,
                                         data->display_banner,
                                         data->write_to_gconf,
                                         data->cancellable,
                                         &error))
    {
      g_error_free (error);

      goto cleanup;
    }

  /* Check if cancelled */
  if (g_cancellable_is_cancelled (data->cancellable))
    goto cleanup;

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

static char *
get_current_theme (void)
{
  gchar *current_theme;

  current_theme = g_new0 (gchar, 2048);
  if (readlink (CURRENT_THEME_DIR, current_theme, 2047) == -1)
    {
      g_warning ("%s. Could not read current theme. %s",
                 __FUNCTION__,
                 gnome_vfs_result_to_string (gnome_vfs_result_from_errno ()));
      g_free (current_theme);
      return NULL;
    }

  return current_theme;
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

  current_theme = get_current_theme ();
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

  /* Get current theme */
  priv->current_theme = get_current_theme ();

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

  for (i = 0; i < priv->requests->len; i++)
    {
      CacheImageRequestData *request = g_ptr_array_index (priv->requests, i);

      if (g_file_has_prefix (request->file,
                             mount_root))
        {
          display_banner = display_banner || request->error_dialogs;

          g_cancellable_cancel (request->cancellable);
        }
    }

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

  for (i = 0; i < priv->requests->len; i++)
    {
      CacheImageRequestData *request = g_ptr_array_index (priv->requests, i);
      gchar *path;
      GnomeVFSVolume *other;

      path = g_file_get_path (request->file);
      other = gnome_vfs_volume_monitor_get_volume_for_path (priv->volume_monitor2,
                                                            path);
      g_free (path);

      if (other && !gnome_vfs_volume_compare (volume, other))
        {
          display_banner = display_banner || request->error_dialogs;

          g_cancellable_cancel (request->cancellable);
        }
    }

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

  priv->requests = g_ptr_array_new ();

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
hd_backgrounds_add_done_cb (HDBackgrounds *backgrounds,
                            GSourceFunc    done_callback,
                            gpointer       cb_data,
                            GDestroyNotify destroy_data)
{
  HDBackgroundsPrivate *priv = backgrounds->priv;

  hd_command_thread_pool_push_idle (priv->thread_pool,
                                    G_PRIORITY_HIGH_IDLE,
                                    done_callback,
                                    cb_data,
                                    destroy_data);
}

GFile *
hd_backgrounds_get_background (HDBackgrounds *backgrounds,
                               guint          view)
{
  HDBackgroundsPrivate *priv;

  g_return_val_if_fail (HD_IS_BACKGROUNDS (backgrounds), NULL);
  g_return_val_if_fail (view < VIEWS, NULL);

  priv = backgrounds->priv;

  return priv->current_bg_image[view];
}

void
hd_backgrounds_add_create_cached_image (HDBackgrounds     *backgrounds,
                                        GFile             *source_file,
                                        gboolean           error_dialogs,
                                        GCancellable      *cancellable,
                                        HDCommandCallback  command,
                                        gpointer           data,
                                        GDestroyNotify     destroy_data)
{
  HDBackgroundsPrivate *priv;
  CacheImageRequestData *request;

  g_return_if_fail (HD_IS_BACKGROUNDS (backgrounds));

  priv = backgrounds->priv;

  request = cache_image_request_data_new (source_file,
                                          error_dialogs,
                                          cancellable);
  g_ptr_array_add (priv->requests,
                   request);

  hd_command_thread_pool_push (priv->thread_pool,
                               command,
                               data,
                               destroy_data);

  hd_command_thread_pool_push_idle (priv->thread_pool,
                                    G_PRIORITY_HIGH_IDLE,
                                    (GSourceFunc) remove_request,
                                    request,
                                    (GDestroyNotify) cache_image_request_data_free);
}

static gboolean
remove_request (CacheImageRequestData *request)
{
  HDBackgrounds *backgrounds = hd_backgrounds_get ();
  HDBackgroundsPrivate *priv = backgrounds->priv;

  g_ptr_array_remove_fast (priv->requests,
                           request);

  return FALSE;
}

gboolean
hd_backgrounds_save_cached_image (HDBackgrounds  *backgrounds,
                                  GdkPixbuf      *pixbuf,
                                  guint           view,
                                  GFile          *source_file,
                                  gboolean        error_dialogs,
                                  gboolean        update_gconf,
                                  GCancellable   *cancellable,
                                  GError        **error)
{
  HDBackgroundsPrivate *priv = backgrounds->priv;
  char *dest_filename;
  GFile *dest_file;
  GError *local_error = NULL;

  /* Create the file objects for the cached background image */
  dest_filename = g_strdup_printf ("%s/" BACKGROUND_CACHED_PNG,
                                   g_get_home_dir (),
                                   view + 1);
  dest_file = g_file_new_for_path (dest_filename);
  g_free (dest_filename);

  /* Create the cached background image */
  /* FIXME: wrong */
  if (!hd_pixbuf_utils_save (dest_file,
                             pixbuf,
                             "png",
                             cancellable,
                             &local_error))
    {
      /* Display not enough space notification banner */
      if (error_dialogs &&
          g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_NO_SPACE))
        {
          show_banner (dgettext ("hildon-common-strings",
                                 "sfil_ni_not_enough_memory"));
        }

      g_warning ("%s. Could not save cached image. %s",
                 __FUNCTION__,
                 local_error->message);

      g_propagate_error (error,
                         local_error);

      return FALSE;
    }

  /* Update cache info at this point thumbnail might not be created 
   * yet but background is */
  g_mutex_lock (priv->mutex);
  if (priv->current_bg_image[view])
    g_object_unref (priv->current_bg_image[view]);
  priv->current_bg_image[view] = g_object_ref (source_file);
  g_mutex_unlock (priv->mutex);

  update_cache_info_file (backgrounds,
                          cancellable);

  /* Update GConf if requested */
  if (update_gconf)
    {
      gchar *gconf_key, *path;

      path = g_file_get_path (source_file);

      /* Store background to GConf */
      gconf_key = g_strdup_printf (GCONF_BACKGROUND_KEY, view + 1);
      gconf_client_set_string (priv->gconf_client,
                               gconf_key,
                               path,
                               &local_error);
      if (local_error)
        {
          g_debug ("%s. Could not set background in GConf for view %u. %s",
                   __FUNCTION__,
                   view,
                   local_error->message);
          g_clear_error (&local_error);
        }

      g_free (gconf_key);
      g_free (path);
    }

  return TRUE;
}

void
hd_backgrounds_report_corrupt_image (const GError *error)
{
  if (g_error_matches (error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_CORRUPT_IMAGE) ||
      g_error_matches (error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_UNKNOWN_TYPE))
    {
      show_banner (dgettext ("hildon-common-strings",
                             "ckct_ni_unable_to_open_file_corrupted"));
    }
}

static CacheImageRequestData *
cache_image_request_data_new (GFile        *file,
                              gboolean      error_dialogs,
                              GCancellable *cancellable)
{
  CacheImageRequestData *data = g_slice_new0 (CacheImageRequestData);

  data->file = g_object_ref (file);
  data->error_dialogs = error_dialogs;
  if (cancellable)
    data->cancellable = g_object_ref (cancellable);

  return data;
}

static void
cache_image_request_data_free (CacheImageRequestData *data)
{
  if (!data)
    return;

  if (data->file)
    g_object_unref (data->file);

  if (data->cancellable)
    g_object_unref (data->cancellable);

  g_slice_free (CacheImageRequestData, data);
}
