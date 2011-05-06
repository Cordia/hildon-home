/*
 * This file is part of hildon-home
 *
 * Copyright (C) 2008, 2009 Nokia Corporation.
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

#include <glib/gi18n.h>

#include <hildon/hildon.h>

#include <gconf/gconf-client.h>
#include <libgnomevfs/gnome-vfs.h>
#include <dbus/dbus-glib.h>

#include <string.h>

#define _XOPEN_SOURCE 500
#include <ftw.h>

#include "hd-shortcut-widgets.h"
#include "hd-task-shortcut.h"

#define HD_SHORTCUT_WIDGETS_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_SHORTCUT_WIDGETS, HDShortcutWidgetsPrivate))

#define TASK_SHORTCUTS_GCONF_KEY "/apps/osso/hildon-home/task-shortcuts"
#define TASK_SHORTCUTS_HIDE_BG_GCONF_KEY "/apps/osso/hildon-home/task-shortcuts-hide-bg"

#define TASK_SHORTCUT_VIEW_GCONF_KEY "/apps/osso/hildon-desktop/applets/TaskShortcut:%s/view"
#define TASK_SHORTCUT_POSITION_GCONF_KEY "/apps/osso/hildon-desktop/applets/TaskShortcut:%s/position"
#define TASK_SHORTCUTS_SIZE_GCONF_KEY "/apps/osso/hildon-home/task-shortcuts-size"

/* Task .desktop file keys */
#define HD_KEY_FILE_DESKTOP_KEY_TRANSLATION_DOMAIN "X-Text-Domain"

/* App mgr D-Bus interface to launch tasks */
#define APP_MGR_DBUS_NAME "com.nokia.HildonDesktop.AppMgr"
#define APP_MGR_DBUS_PATH "/com/nokia/HildonDesktop/AppMgr"
#define APP_MGR_LAUNCH_APPLICATION "LaunchApplication"

struct _HDShortcutWidgetsPrivate
{
  GtkTreeModel *model;
  GtkTreeModel *filtered_model;

  GHashTable *available_tasks;
  GHashTable *installed_shortcuts;

  GHashTable *monitors;

  GConfClient *gconf_client;
};

typedef struct
{
  gchar *label;
  gchar *icon_name;

  GdkPixbuf *icon;

  GtkTreeRowReference *row;
} HDTaskInfo;

enum
{
  DESKTOP_FILE_CHANGED,
  DESKTOP_FILE_DELETED,
  LAST_SIGNAL
};

enum
{
  COL_TITLE,
  COL_ICON,
  COL_DESKTOP,
};

static guint shortcut_widgets_signals [LAST_SIGNAL] = { 0 };

static gboolean hd_shortcut_widgets_scan_for_desktop_files (const gchar *directory);

G_DEFINE_TYPE (HDShortcutWidgets, hd_shortcut_widgets, HD_TYPE_WIDGETS);

/** hd_task_info_free:
 * @info The #HDTaskInfo to free
 *
 * Frees a #HDTaskInfo created with g_slice_new0 (HDTaskInfo).
 **/
static void
hd_task_info_free (HDTaskInfo *info)
{
  if (!info)
    return;

  g_free (info->label);
  g_free (info->icon_name);

  if (info->icon)
    g_object_unref (info->icon);

  if (info->row)
    gtk_tree_row_reference_free (info->row);

  g_slice_free (HDTaskInfo, info);
}

static GdkPixbuf *
load_icon_from_absolute_path (const gchar *path)
{
  GdkPixbuf *pixbuf;
  GError *error = NULL;

  pixbuf = gdk_pixbuf_new_from_file_at_size (path,
                                             HILDON_ICON_PIXEL_SIZE_THUMB,
                                             HILDON_ICON_PIXEL_SIZE_THUMB,
                                             &error);
  if (error)
    {
      g_debug ("%s. Could not load icon %s from file. %s",
               __FUNCTION__,
               path,
               error->message);
      g_error_free (error);
    }

  return pixbuf;
}

static GdkPixbuf *
load_icon_from_theme (const gchar *icon)
{
  GdkPixbuf *pixbuf;
  GError *error = NULL;

  pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                     icon,
                                     HILDON_ICON_PIXEL_SIZE_THUMB,
                                     GTK_ICON_LOOKUP_NO_SVG,
                                     &error);

  if (error)
    {
      g_debug ("%s. Could not load icon %s from theme. %s",
               __FUNCTION__,
               icon,
               error->message);
      g_error_free (error);
    }

  return pixbuf;
}

static GdkPixbuf *
load_default_icon (void)
{
  GdkPixbuf *pixbuf;
  GError *error = NULL;

  pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                     "tasklaunch_default_application",
                                     HILDON_ICON_PIXEL_SIZE_THUMB,
                                     GTK_ICON_LOOKUP_NO_SVG,
                                     &error);

  if (error)
    {
      g_warning ("%s. Could not load default application icon from theme. %s",
                 __FUNCTION__,
                 error->message);
      g_error_free (error);
    }

  return pixbuf;
}

static GdkPixbuf *
load_icon_from_icon_name (const gchar *icon_name)
{
  GdkPixbuf *pixbuf = NULL;

  if (icon_name)
    {
      if (g_path_is_absolute (icon_name))
        pixbuf = load_icon_from_absolute_path (icon_name);
      else
        pixbuf = load_icon_from_theme (icon_name);
    }

  if (!pixbuf)
    pixbuf = load_default_icon ();

  return pixbuf;
}

static gboolean
hd_shortcut_widgets_load_desktop_file (const gchar *filename)
{
  HDWidgets *widgets = hd_shortcut_widgets_get ();
  HDShortcutWidgetsPrivate *priv = HD_SHORTCUT_WIDGETS (widgets)->priv;
  GKeyFile *desktop_file;
  GError *error = NULL;
  HDTaskInfo *info = NULL;
  gchar *desktop_id = NULL;
  gchar *type = NULL, *translation_domain = NULL, *name = NULL;

  g_debug ("hd_shortcut_widgets_load_desktop_file (%s)", filename);

  desktop_file = g_key_file_new ();
  if (!g_key_file_load_from_file (desktop_file,
                                  filename,
                                  G_KEY_FILE_NONE,
                                  &error))
    {
      g_debug ("Could not read .desktop file `%s'. %s",
               filename,
               error->message);
      g_error_free (error);
      goto cleanup;
    }

  type = g_key_file_get_string (desktop_file,
                                G_KEY_FILE_DESKTOP_GROUP,
                                G_KEY_FILE_DESKTOP_KEY_TYPE,
                                NULL);

  /* Test if type is Application */
  if (!type || strcmp (type, G_KEY_FILE_DESKTOP_TYPE_APPLICATION))
    {
      goto cleanup;
    }

  /* Test if it should not displayed */
  if (g_key_file_get_boolean (desktop_file,
                              G_KEY_FILE_DESKTOP_GROUP,
                              G_KEY_FILE_DESKTOP_KEY_NO_DISPLAY,
                              NULL))
    {
      goto cleanup;
    }

  /* Get translation domain if set, so Name can be translated */
  translation_domain = g_key_file_get_string (desktop_file,
                                              G_KEY_FILE_DESKTOP_GROUP,
                                              HD_KEY_FILE_DESKTOP_KEY_TRANSLATION_DOMAIN,
                                              NULL);

  name = g_key_file_get_string (desktop_file,
                                G_KEY_FILE_DESKTOP_GROUP,
                                G_KEY_FILE_DESKTOP_KEY_NAME,
                                &error);
  if (error)
    {
      g_debug ("Could not read Name entry in .desktop file `%s'. %s",
               filename,
               error->message);
      g_error_free (error);
      goto cleanup;
    }

  /* Get the desktop_id */
  desktop_id = g_path_get_basename (filename);

  info = g_hash_table_lookup (priv->available_tasks,
                              desktop_id);
  if (!info)
    {
      info = g_slice_new0 (HDTaskInfo);

      g_hash_table_insert (priv->available_tasks,
                           g_strdup (desktop_id),
                           info);
    }
  else
    {
      g_free (info->label);
      g_free (info->icon_name);
      if (info->icon)
        info->icon = (g_object_unref (info->icon), NULL);
    }

  /* Translate name */
  if (!translation_domain)
    {
      /* Use GETTEXT_PACKAGE as default translation domain */
      info->label = g_strdup (dgettext (GETTEXT_PACKAGE, name));
    }
  else
    {
      info->label = g_strdup (dgettext (translation_domain, name));
    }

  /* Get the icon */
  info->icon_name = g_key_file_get_string (desktop_file,
                                           G_KEY_FILE_DESKTOP_GROUP,
                                           G_KEY_FILE_DESKTOP_KEY_ICON,
                                           &error);
  if (!info->icon_name)
    {
      g_debug ("Could not read Icon entry in .desktop file `%s'. %s",
               filename,
               error->message);
      g_error_free (error);
    }

  info->icon = load_icon_from_icon_name (info->icon_name);

  if (gtk_tree_row_reference_valid (info->row))
    {
      GtkTreeIter iter;
      GtkTreePath *path;

      path = gtk_tree_row_reference_get_path (info->row);
      if (gtk_tree_model_get_iter (priv->model, &iter, path))
        {
          gtk_list_store_set (GTK_LIST_STORE (priv->model),
                              &iter,
                              COL_TITLE, info->label,
                              COL_ICON, info->icon,
                              COL_DESKTOP, desktop_id,
                              -1);
        }
      gtk_tree_path_free (path);
    }
  else
    {
      GtkTreeIter iter;
      GtkTreePath *path;

      gtk_list_store_insert_with_values (GTK_LIST_STORE (priv->model),
                                         &iter, -1,
                                         COL_TITLE, info->label,
                                         COL_ICON, info->icon,
                                         COL_DESKTOP, desktop_id,
                                         -1);

      /* Update row reference */
      path = gtk_tree_model_get_path (priv->model, &iter);
      gtk_tree_row_reference_free (info->row);
      info->row = gtk_tree_row_reference_new (priv->model, path);
      gtk_tree_path_free (path);
    }

  g_signal_emit (widgets,
                 shortcut_widgets_signals[DESKTOP_FILE_CHANGED],
                 g_quark_from_string (desktop_id));

cleanup:
  g_key_file_free (desktop_file);
  g_free (type);
  g_free (translation_domain);
  g_free (name);
  g_free (desktop_id);

  return FALSE;
}

static void
update_icon (HDShortcutWidgets *widgets,
             const gchar       *desktop_id,
             HDTaskInfo        *info)
{
  HDShortcutWidgetsPrivate *priv = widgets->priv;

  if (info->icon)
    g_object_unref (info->icon);

  info->icon = load_icon_from_icon_name (info->icon_name);

  if (gtk_tree_row_reference_valid (info->row))
    {
      GtkTreeIter iter;
      GtkTreePath *path;

      path = gtk_tree_row_reference_get_path (info->row);
      if (gtk_tree_model_get_iter (priv->model, &iter, path))
        {
          gtk_list_store_set (GTK_LIST_STORE (priv->model),
                              &iter,
                              COL_ICON, info->icon,
                              -1);
        }
      gtk_tree_path_free (path);
    }

  g_signal_emit (widgets,
                 shortcut_widgets_signals[DESKTOP_FILE_CHANGED],
                 g_quark_from_string (desktop_id));
}

static void
update_all_icons (HDShortcutWidgets *widgets)
{
  HDShortcutWidgetsPrivate *priv = widgets->priv;
  GHashTableIter iter;
  gpointer desktop_id, task_info;

  g_hash_table_iter_init (&iter,
                          priv->available_tasks);
  while (g_hash_table_iter_next (&iter, &desktop_id, &task_info)) 
    {
      update_icon (widgets,
                   desktop_id,
                   task_info);
    }
}

static gboolean
hd_shortcut_widgets_remove_desktop_file (const gchar *filename)
{
  HDWidgets *widgets = hd_shortcut_widgets_get ();
  HDShortcutWidgetsPrivate *priv = HD_SHORTCUT_WIDGETS (widgets)->priv;
  gchar *desktop_id = NULL;
  HDTaskInfo *info = NULL;


  /* Get the desktop_id */
  desktop_id = g_path_get_basename (filename);

  info = g_hash_table_lookup (priv->available_tasks,
                              desktop_id);

  if (info && gtk_tree_row_reference_valid (info->row))
    {
      GtkTreeIter iter;
      GtkTreePath *path;

      path = gtk_tree_row_reference_get_path (info->row);
      if (gtk_tree_model_get_iter (priv->model, &iter, path))
        gtk_list_store_remove (GTK_LIST_STORE (priv->model),
                               &iter);
      gtk_tree_path_free (path);
    }

  g_signal_emit (widgets,
                 shortcut_widgets_signals[DESKTOP_FILE_DELETED],
                 g_quark_from_string (desktop_id));

  g_hash_table_remove (priv->available_tasks,
                       desktop_id);

  g_free (desktop_id);

  return FALSE;
}
 
static void
applications_dir_changed (GnomeVFSMonitorHandle    *handle,
                          const gchar              *monitor_uri,
                          const gchar              *info_uri,
                          GnomeVFSMonitorEventType  event_type,
                          gpointer                  user_data)
{
  if (g_file_test (info_uri, G_FILE_TEST_IS_DIR))
    {
      if (event_type == GNOME_VFS_MONITOR_EVENT_CREATED)
        g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                         (GSourceFunc) hd_shortcut_widgets_scan_for_desktop_files,
                         gnome_vfs_get_local_path_from_uri (info_uri),
                         (GDestroyNotify) g_free);
    }
  else
    {
      if (event_type == GNOME_VFS_MONITOR_EVENT_CREATED ||
          event_type == GNOME_VFS_MONITOR_EVENT_CHANGED)
        g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                         (GSourceFunc) hd_shortcut_widgets_load_desktop_file,
                         gnome_vfs_get_local_path_from_uri (info_uri),
                         (GDestroyNotify) g_free);
      else if (event_type == GNOME_VFS_MONITOR_EVENT_DELETED)
        {
          g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                           (GSourceFunc) hd_shortcut_widgets_remove_desktop_file,
                           gnome_vfs_get_local_path_from_uri (info_uri),
                           (GDestroyNotify) g_free);
        }
    }
}

static int
visit_func (const char        *f_path,
            const struct stat *sb,
            int                type_flag,
            struct FTW        *ftw_buf)
{
  g_debug ("visit_func %s, %d", f_path, type_flag);

  /* Directory */
  switch (type_flag)
    {
      case FTW_D:
          {
            GnomeVFSMonitorHandle *handle;
            HDShortcutWidgetsPrivate *priv = HD_SHORTCUT_WIDGETS (hd_shortcut_widgets_get ())->priv;

            gnome_vfs_monitor_add (&handle,
                                   f_path,
                                   GNOME_VFS_MONITOR_DIRECTORY,
                                   (GnomeVFSMonitorCallback) applications_dir_changed,
                                   NULL);

            g_hash_table_insert (priv->monitors,
                                 g_strdup (f_path),
                                 handle);
          }
        break;
      case FTW_F:
        g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                         (GSourceFunc) hd_shortcut_widgets_load_desktop_file,
                         g_strdup (f_path),
                         (GDestroyNotify) g_free);
        break;
      default:
        g_debug ("%s, %d", f_path, type_flag);
    }

  return 0;
}

static gboolean
hd_shortcut_widgets_scan_for_desktop_files (const gchar *directory)
{
  g_debug ("hd_shortcut_widgets_scan_for_desktop_files: %s", directory);

  nftw (directory, visit_func, 20, FTW_PHYS); 

  return FALSE;
}

extern gboolean task_shortcuts_hide_bg;

extern int task_shortcut_width;
extern int task_shortcut_height;

static void
update_installed_shortcuts (HDShortcutWidgets *widgets)
{
  HDShortcutWidgetsPrivate *priv = widgets->priv;
  GSList *list, *l;
  GError *error = NULL;

  task_shortcuts_hide_bg = gconf_client_get_bool (priv->gconf_client,
                                TASK_SHORTCUTS_HIDE_BG_GCONF_KEY, &error);
  if(error || !task_shortcuts_hide_bg) task_shortcuts_hide_bg = 0;
  
  task_shortcut_width = gconf_client_get_int (priv->gconf_client,
                                  TASK_SHORTCUTS_SIZE_GCONF_KEY, &error);
  if(error || !task_shortcut_width)
  {
    task_shortcut_width = HD_TASK_DEF_SHORTCUT_WIDTH;
    task_shortcut_height = HD_TASK_DEF_SHORTCUT_HEIGHT;
  }
  else
  {
    task_shortcut_height = HD_TASK_SHORTCUT_HEIGHT ;
  }

  /* Scale backgrounds if needed */
  GdkPixbuf *pixbuf;
  int need_to_scale = 0;
  pixbuf = gdk_pixbuf_new_from_file (HD_TASK_SCALED_BACKGROUND_IMAGE_FILE, NULL);
  if (pixbuf) {
    if (gdk_pixbuf_get_width(pixbuf) != task_shortcut_width) {
      need_to_scale = 1;
    }
  } else {
    need_to_scale = 1;
  }
  g_object_unref(pixbuf);

  if (need_to_scale) {
    pixbuf = gdk_pixbuf_new_from_file (HD_TASK_BACKGROUND_IMAGE_FILE, NULL);
    pixbuf = gdk_pixbuf_scale_simple(pixbuf, task_shortcut_width, task_shortcut_width, GDK_INTERP_BILINEAR);
    gdk_pixbuf_save (pixbuf, HD_TASK_SCALED_BACKGROUND_IMAGE_FILE, "png", NULL, "quality", "100", NULL);
    g_object_unref(pixbuf);

    pixbuf = gdk_pixbuf_new_from_file (HD_TASK_BACKGROUND_ACTIVE_IMAGE_FILE, NULL);
    pixbuf = gdk_pixbuf_scale_simple(pixbuf, task_shortcut_width, task_shortcut_width, GDK_INTERP_BILINEAR);
    gdk_pixbuf_save (pixbuf, HD_TASK_SCALED_BACKGROUND_ACTIVE_IMAGE_FILE, "png", NULL, "quality", "100", NULL);
    g_object_unref(pixbuf);
  }



  /* Get the list of strings of task shortcuts */
  list = gconf_client_get_list (priv->gconf_client,
                                TASK_SHORTCUTS_GCONF_KEY,
                                GCONF_VALUE_STRING,
                                &error);

  /* Check if there was an error */
  if (error)
    {
      g_warning ("Could not get list of task shortcuts from GConf: %s", error->message);
      g_error_free (error);
      return;
    }

  /* Replace content of hash table with list of installed shortcuts */
  g_hash_table_remove_all (priv->installed_shortcuts);
  for (l = list; l; l = l->next)
    {
      g_hash_table_insert (priv->installed_shortcuts,
                           l->data,
                           GUINT_TO_POINTER (1));
    }

  /* Update filtered model */
  gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (priv->filtered_model));

  /* Free the list, the list content is still referenced by the hash table */
  g_slist_free (list);
}

static void
shortcuts_gconf_notify (GConfClient   *client,
                        guint          cnxn_id,
                        GConfEntry    *entry,
                        HDShortcutWidgets *widgets)
{
  update_installed_shortcuts (widgets);
}

static gboolean
filtered_model_visible_func (GtkTreeModel *model,
                             GtkTreeIter  *iter,
                             gpointer      data)
{
  HDShortcutWidgetsPrivate *priv = HD_SHORTCUT_WIDGETS (data)->priv;
  gchar *desktop_id;
  gpointer value;

  gtk_tree_model_get (model, iter,
                      2, &desktop_id,
                      -1);

  /* Check if a shortcut for desktop-id is already installed */
  value = g_hash_table_lookup (priv->installed_shortcuts,
                               desktop_id);

  g_free (desktop_id);

  return value == NULL;
}

static void
hd_shortcut_widgets_init (HDShortcutWidgets *widgets)
{
  HDShortcutWidgetsPrivate *priv;

  /* Install private */
  widgets->priv = HD_SHORTCUT_WIDGETS_GET_PRIVATE (widgets);
  priv = widgets->priv;

  priv->available_tasks = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                 g_free, (GDestroyNotify) hd_task_info_free);
  priv->installed_shortcuts = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                     g_free, NULL);
  priv->monitors = g_hash_table_new_full (g_str_hash, g_str_equal,
                                          g_free, (GDestroyNotify) gnome_vfs_monitor_cancel);

  priv->model = GTK_TREE_MODEL (gtk_list_store_new (3,
                                                    G_TYPE_STRING,
                                                    GDK_TYPE_PIXBUF,
                                                    G_TYPE_STRING));
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (priv->model),
                                        0,
                                        GTK_SORT_ASCENDING);
  priv->filtered_model = gtk_tree_model_filter_new (priv->model,
                                                    NULL);
  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (priv->filtered_model),
                                          filtered_model_visible_func,
                                          widgets,
                                          NULL);

  /* GConf */
  priv->gconf_client = gconf_client_get_default ();

  /* Add notification of shortcuts key */
  gconf_client_notify_add (priv->gconf_client,
                           TASK_SHORTCUTS_GCONF_KEY,
                           (GConfClientNotifyFunc) shortcuts_gconf_notify,
                           widgets,
                           NULL, NULL);

  update_installed_shortcuts (widgets);

  g_signal_connect_swapped (gtk_icon_theme_get_default (), "changed",
                            G_CALLBACK (update_all_icons), widgets);
}

static void
hd_shortcut_widgets_dispose (GObject *obj)
{
  HDShortcutWidgetsPrivate *priv = HD_SHORTCUT_WIDGETS (obj)->priv;

  if (priv->gconf_client)
    priv->gconf_client = (g_object_unref (priv->gconf_client), NULL);

  if (priv->filtered_model)
    priv->filtered_model = (g_object_unref (priv->filtered_model), NULL);

  if (priv->model)
    priv->model = (g_object_unref (priv->model), NULL);

  G_OBJECT_CLASS (hd_shortcut_widgets_parent_class)->dispose (obj);
}

static void
hd_shortcut_widgets_finalize (GObject *obj)
{
  HDShortcutWidgetsPrivate *priv = HD_SHORTCUT_WIDGETS (obj)->priv;

  g_hash_table_destroy (priv->available_tasks);
  g_hash_table_destroy (priv->installed_shortcuts);
  g_hash_table_destroy (priv->monitors);

  G_OBJECT_CLASS (hd_shortcut_widgets_parent_class)->finalize (obj);
}

static GtkTreeModel *
hd_shortcut_widgets_get_model (HDWidgets *widgets)
{
  HDShortcutWidgetsPrivate *priv = HD_SHORTCUT_WIDGETS (widgets)->priv;

  return g_object_ref (priv->filtered_model);
}


static const gchar *
hd_shortcut_widgets_get_dialog_title (HDWidgets *widgets)
{
  return dgettext (GETTEXT_PACKAGE, "home_ti_select_shortcut");
}

static void
hd_shortcut_widgets_setup_column_renderes (HDWidgets     *widgets,
                                           GtkCellLayout *column)
{
  GtkCellRenderer *renderer;

  /* Add the icon renderer */
  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_layout_pack_start (column,
                              renderer,
                              FALSE);
  gtk_cell_layout_add_attribute (column,
                                 renderer,
                                 "pixbuf", COL_ICON);

  /* Add the label renderer */
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer, "xpad", HILDON_MARGIN_DOUBLE, NULL);
  gtk_cell_layout_pack_start (column,
                              renderer,
                              FALSE);
  gtk_cell_layout_add_attribute (column,
                                 renderer,
                                 "text", COL_TITLE);
}

static void
hd_shortcut_widgets_install_widget (HDWidgets   *widgets,
                                GtkTreePath *path)
{
  HDShortcutWidgetsPrivate *priv = HD_SHORTCUT_WIDGETS (widgets)->priv;
  GtkTreeIter iter;
  gchar *desktop_id;
  GHashTableIter hash_iter;
  gpointer key;
  GSList *list = NULL;
  GError *error = NULL;
  gchar *gconf_key;

  gtk_tree_model_get_iter (priv->filtered_model, &iter, path);

  gtk_tree_model_get (priv->filtered_model, &iter,
                      2, &desktop_id,
                      -1);

  /* Reset view and position key because they are explicitly added */
  gconf_key = g_strdup_printf (TASK_SHORTCUT_VIEW_GCONF_KEY, desktop_id);
  gconf_client_set_int (priv->gconf_client,
                        gconf_key,
                        -1,
                        &error);
  if (error)
    {
      g_warning ("%s. Could not write view key %s to GConf. %s",
                 __FUNCTION__,
                 gconf_key,
                 error->message);
      g_clear_error (&error);
    }
  g_free (gconf_key);

  gconf_key = g_strdup_printf (TASK_SHORTCUT_POSITION_GCONF_KEY, desktop_id);
  gconf_client_set_list (priv->gconf_client,
                         gconf_key,
                         GCONF_VALUE_INT,
                         NULL,
                         &error);
  if (error)
    {
      g_warning ("%s. Could not write position key %s to GConf. %s",
                 __FUNCTION__,
                 gconf_key,
                 error->message);
      g_clear_error (&error);
    }
  g_free (gconf_key);

  /* Add it to installed shortcuts set */
  g_hash_table_insert (priv->installed_shortcuts,
                       desktop_id,
                       GUINT_TO_POINTER (1));

  /* Iterate over all installed shortcuts and add them to the list */
  g_hash_table_iter_init (&hash_iter, priv->installed_shortcuts);
  while (g_hash_table_iter_next (&hash_iter, &key, NULL))
    list = g_slist_append (list, key);

  /* Set the new list to GConf */
  gconf_client_set_list (priv->gconf_client,
                         TASK_SHORTCUTS_GCONF_KEY,
                         GCONF_VALUE_STRING,
                         list,
                         &error);

  if (error)
    {
      g_warning ("Could not write string list to GConf (%s): %s.",
                 TASK_SHORTCUTS_GCONF_KEY,
                 error->message);
      g_clear_error (&error);
    }

  /* Free the list, the content is still referenced by the hash table */
  g_slist_free (list);
}

static gint
hd_shortcut_widgets_get_text_column (HDWidgets *widgets)
{
  return COL_TITLE;
}

static void
hd_shortcut_widgets_class_init (HDShortcutWidgetsClass *klass)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (klass);
  HDWidgetsClass *widgets_class = HD_WIDGETS_CLASS (klass);

  obj_class->dispose = hd_shortcut_widgets_dispose;
  obj_class->finalize = hd_shortcut_widgets_finalize;

  widgets_class->get_dialog_title = hd_shortcut_widgets_get_dialog_title;
  widgets_class->get_model = hd_shortcut_widgets_get_model;
  widgets_class->setup_column_renderes = hd_shortcut_widgets_setup_column_renderes;
  widgets_class->install_widget = hd_shortcut_widgets_install_widget;
  widgets_class->get_text_column  = hd_shortcut_widgets_get_text_column;

  shortcut_widgets_signals [DESKTOP_FILE_CHANGED] = g_signal_new ("desktop-file-changed",
                                                              G_TYPE_FROM_CLASS (klass),
                                                              G_SIGNAL_RUN_FIRST | G_SIGNAL_DETAILED,
                                                              G_STRUCT_OFFSET (HDShortcutWidgetsClass,
                                                                               desktop_file_changed),
                                                              NULL, NULL,
                                                              g_cclosure_marshal_VOID__VOID,
                                                              G_TYPE_NONE, 0);
  shortcut_widgets_signals [DESKTOP_FILE_DELETED] = g_signal_new ("desktop-file-deleted",
                                                              G_TYPE_FROM_CLASS (klass),
                                                              G_SIGNAL_RUN_FIRST | G_SIGNAL_DETAILED,
                                                              0,
                                                              NULL, NULL,
                                                              g_cclosure_marshal_VOID__VOID,
                                                              G_TYPE_NONE, 0);

  g_type_class_add_private (klass, sizeof (HDShortcutWidgetsPrivate));
}

/* Returns the singleton HDShortcutWidgets instance. Should not be refed or unrefed */
HDWidgets *
hd_shortcut_widgets_get (void)
{
  static HDWidgets *widgets = NULL;

  if (G_UNLIKELY (!widgets))
    {
      widgets = g_object_new (HD_TYPE_SHORTCUT_WIDGETS, NULL);

      gdk_threads_add_idle ((GSourceFunc) hd_shortcut_widgets_scan_for_desktop_files,
                            HD_APPLICATIONS_DIR);
      gdk_threads_add_idle ((GSourceFunc) hd_shortcut_widgets_scan_for_desktop_files,
                            HD_USER_APPLICATIONS_DIR);
    }

  return widgets;
}

gboolean
hd_shortcut_widgets_is_available (HDShortcutWidgets *widgets,
                                  const gchar       *desktop_id)
{
  HDShortcutWidgetsPrivate *priv = widgets->priv;

  g_return_val_if_fail (HD_IS_SHORTCUT_WIDGETS (widgets), FALSE);
  g_return_val_if_fail (desktop_id, FALSE);

  return g_hash_table_lookup (priv->available_tasks,
                              desktop_id) != NULL;
}

GdkPixbuf *
hd_shortcut_widgets_get_icon (HDShortcutWidgets *widgets,
                              const gchar       *desktop_id)
{
  HDShortcutWidgetsPrivate *priv = widgets->priv;
  HDTaskInfo *info;

  g_return_val_if_fail (HD_IS_SHORTCUT_WIDGETS (widgets), NULL);
  g_return_val_if_fail (desktop_id, NULL);

  /* Lookup task */
  info = g_hash_table_lookup (priv->available_tasks,
                              desktop_id);

  /* Return NULL if task is not available */
  if (!info)
    return NULL;

  return info->icon;
}
