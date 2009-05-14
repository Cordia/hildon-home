/*
 * This file is part of hildon-home
 *
 * Copyright (C) 2008 Nokia Corporation.
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

#include <string.h>

#define _XOPEN_SOURCE 500
#include <ftw.h>

#include "hd-task-manager.h"

#define HD_TASK_MANAGER_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_TASK_MANAGER, HDTaskManagerPrivate))

#define TASK_SHORTCUTS_GCONF_KEY "/apps/osso/hildon-home/task-shortcuts"

/* Task .desktop file keys */
#define HD_KEY_FILE_DESKTOP_KEY_TRANSLATION_DOMAIN "X-Text-Domain"

/* App mgr D-Bus interface to launch tasks */
#define APP_MGR_DBUS_NAME "com.nokia.HildonDesktop.AppMgr"
#define APP_MGR_DBUS_PATH "/com/nokia/HildonDesktop/AppMgr"

#define APP_MGR_LAUNCH_APPLICATION "LaunchApplication"

struct _HDTaskManagerPrivate
{
  HDPluginConfiguration *plugin_configuration;

  GtkTreeModel *model;
  GtkTreeModel *filtered_model;

  GHashTable *available_tasks;
  GHashTable *installed_shortcuts;

  GHashTable *monitors;

  GConfClient *gconf_client;

  DBusGProxy *app_mgr_proxy;
};

typedef struct
{
  gchar *label;
  gchar *icon;

  GtkTreeRowReference *row;
} HDTaskInfo;

enum
{
  DESKTOP_FILE_CHANGED,
  DESKTOP_FILE_DELETED,
  LAST_SIGNAL
};

static guint task_manager_signals [LAST_SIGNAL] = { 0 };

static gboolean hd_task_manager_scan_for_desktop_files (const gchar *directory);

G_DEFINE_TYPE (HDTaskManager, hd_task_manager, G_TYPE_OBJECT);

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
  g_free (info->icon);

  if (info->row)
    gtk_tree_row_reference_free (info->row);

  g_slice_free (HDTaskInfo, info);
}

static gboolean
hd_task_manager_load_desktop_file (const gchar *filename)
{
  HDTaskManager *manager = hd_task_manager_get ();
  HDTaskManagerPrivate *priv = manager->priv;
  GKeyFile *desktop_file;
  GError *error = NULL;
  HDTaskInfo *info = NULL;
  gchar *desktop_id = NULL;
  gchar *type = NULL, *translation_domain = NULL, *name = NULL;
  GdkPixbuf *pixbuf = NULL;
  GtkIconTheme *icon_theme = gtk_icon_theme_get_default ();
  GtkIconInfo *icon_info = NULL;

  g_debug ("hd_task_manager_load_desktop_file (%s)", filename);

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
      g_free (info->icon);
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
  info->icon = g_key_file_get_string (desktop_file,
                                      G_KEY_FILE_DESKTOP_GROUP,
                                      G_KEY_FILE_DESKTOP_KEY_ICON,
                                      &error);
  if (!info->icon)
    {
      g_debug ("Could not read Icon entry in .desktop file `%s'. %s",
               filename,
               error->message);
      g_error_free (error);
    }

  /* Load icon for list */
  if (info->icon)
    icon_info = gtk_icon_theme_lookup_icon (icon_theme,
                                            info->icon,
                                            64,
                                            GTK_ICON_LOOKUP_NO_SVG);

  if (!icon_info)
    icon_info = gtk_icon_theme_lookup_icon (icon_theme,
                                            "tasklaunch_default_application",
                                            64,
                                            GTK_ICON_LOOKUP_NO_SVG);

  if (icon_info)
    {
      pixbuf = gtk_icon_info_load_icon (icon_info, NULL);
      gtk_icon_info_free (icon_info);
    }

  if (gtk_tree_row_reference_valid (info->row))
    {
      GtkTreeIter iter;
      GtkTreePath *path;

      path = gtk_tree_row_reference_get_path (info->row);
      if (gtk_tree_model_get_iter (priv->model, &iter, path))
        {
          gtk_list_store_set (GTK_LIST_STORE (priv->model),
                              &iter,
                              0, info->label,
                              1, info->icon,
                              2, desktop_id,
                              3, pixbuf,
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
                                         0, info->label,
                                         1, info->icon,
                                         2, desktop_id,
                                         3, pixbuf,
                                         -1);

      /* Update row reference */
      path = gtk_tree_model_get_path (priv->model, &iter);
      gtk_tree_row_reference_free (info->row);
      info->row = gtk_tree_row_reference_new (priv->model, path);
      gtk_tree_path_free (path);
    }

  g_signal_emit (manager,
                 task_manager_signals[DESKTOP_FILE_CHANGED],
                 g_quark_from_string (desktop_id));

cleanup:
  g_key_file_free (desktop_file);
  g_free (type);
  g_free (translation_domain);
  g_free (name);
  g_free (desktop_id);
  if (pixbuf)
    g_object_unref (pixbuf);

  return FALSE;
}

static gboolean
hd_task_manager_remove_desktop_file (const gchar *filename)
{
  HDTaskManager *manager = hd_task_manager_get ();
  HDTaskManagerPrivate *priv = manager->priv;
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

  g_signal_emit (manager,
                 task_manager_signals[DESKTOP_FILE_DELETED],
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
                         (GSourceFunc) hd_task_manager_scan_for_desktop_files,
                         gnome_vfs_get_local_path_from_uri (info_uri),
                         (GDestroyNotify) g_free);
    }
  else
    {
      if (event_type == GNOME_VFS_MONITOR_EVENT_CREATED ||
          event_type == GNOME_VFS_MONITOR_EVENT_CHANGED)
        g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                         (GSourceFunc) hd_task_manager_load_desktop_file,
                         gnome_vfs_get_local_path_from_uri (info_uri),
                         (GDestroyNotify) g_free);
      else if (event_type == GNOME_VFS_MONITOR_EVENT_DELETED)
        {
          g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                           (GSourceFunc) hd_task_manager_remove_desktop_file,
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
            HDTaskManagerPrivate *priv = hd_task_manager_get ()->priv;

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
                         (GSourceFunc) hd_task_manager_load_desktop_file,
                         g_strdup (f_path),
                         (GDestroyNotify) g_free);
        break;
      default:
        g_debug ("%s, %d", f_path, type_flag);
    }

  return 0;
}

static gboolean
hd_task_manager_scan_for_desktop_files (const gchar *directory)
{
  g_debug ("hd_task_manager_scan_for_desktop_files: %s", directory);

  nftw (directory, visit_func, 20, FTW_PHYS); 

  return FALSE;
}

static void
update_installed_shortcuts (HDTaskManager *manager)
{
  HDTaskManagerPrivate *priv = manager->priv;
  GSList *list, *l;
  GError *error = NULL;

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
                        HDTaskManager *manager)
{
  update_installed_shortcuts (manager);
}

static gboolean
filtered_model_visible_func (GtkTreeModel *model,
                             GtkTreeIter  *iter,
                             gpointer      data)
{
  HDTaskManagerPrivate *priv = HD_TASK_MANAGER (data)->priv;
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
hd_task_manager_init (HDTaskManager *manager)
{
  HDTaskManagerPrivate *priv;

  /* Install private */
  manager->priv = HD_TASK_MANAGER_GET_PRIVATE (manager);
  priv = manager->priv;

  priv->available_tasks = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                 g_free, (GDestroyNotify) hd_task_info_free);
  priv->installed_shortcuts = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                     g_free, NULL);
  priv->monitors = g_hash_table_new_full (g_str_hash, g_str_equal,
                                          g_free, (GDestroyNotify) gnome_vfs_monitor_cancel);

  priv->model = GTK_TREE_MODEL (gtk_list_store_new (4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, GDK_TYPE_PIXBUF));
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (priv->model),
                                        0,
                                        GTK_SORT_ASCENDING);
  priv->filtered_model = gtk_tree_model_filter_new (priv->model,
                                                    NULL);
  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (priv->filtered_model),
                                          filtered_model_visible_func,
                                          manager,
                                          NULL);

  /* GConf */
  priv->gconf_client = gconf_client_get_default ();

  /* Add notification of shortcuts key */
  gconf_client_notify_add (priv->gconf_client,
                           TASK_SHORTCUTS_GCONF_KEY,
                           (GConfClientNotifyFunc) shortcuts_gconf_notify,
                           manager,
                           NULL, NULL);

  update_installed_shortcuts (manager);
}

static void
hd_task_manager_dispose (GObject *obj)
{
  HDTaskManagerPrivate *priv = HD_TASK_MANAGER (obj)->priv;

  if (priv->gconf_client)
    priv->gconf_client = (g_object_unref (priv->gconf_client), NULL);

  if (priv->filtered_model)
    priv->filtered_model = (g_object_unref (priv->filtered_model), NULL);

  if (priv->model)
    priv->model = (g_object_unref (priv->model), NULL);

  if (priv->app_mgr_proxy)
    priv->app_mgr_proxy = (g_object_unref (priv->app_mgr_proxy), NULL);

  G_OBJECT_CLASS (hd_task_manager_parent_class)->dispose (obj);
}

static void
hd_task_manager_finalize (GObject *obj)
{
  HDTaskManagerPrivate *priv = HD_TASK_MANAGER (obj)->priv;

  g_hash_table_destroy (priv->available_tasks);
  g_hash_table_destroy (priv->installed_shortcuts);
  g_hash_table_destroy (priv->monitors);

  G_OBJECT_CLASS (hd_task_manager_parent_class)->finalize (obj);
}

static void
hd_task_manager_class_init (HDTaskManagerClass *klass)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (klass);

  obj_class->dispose = hd_task_manager_dispose;
  obj_class->finalize = hd_task_manager_finalize;

  task_manager_signals [DESKTOP_FILE_CHANGED] = g_signal_new ("desktop-file-changed",
                                                              G_TYPE_FROM_CLASS (klass),
                                                              G_SIGNAL_RUN_FIRST | G_SIGNAL_DETAILED,
                                                              G_STRUCT_OFFSET (HDTaskManagerClass,
                                                                               desktop_file_changed),
                                                              NULL, NULL,
                                                              g_cclosure_marshal_VOID__VOID,
                                                              G_TYPE_NONE, 0);
  task_manager_signals [DESKTOP_FILE_DELETED] = g_signal_new ("desktop-file-deleted",
                                                              G_TYPE_FROM_CLASS (klass),
                                                              G_SIGNAL_RUN_FIRST | G_SIGNAL_DETAILED,
                                                              0,
                                                              NULL, NULL,
                                                              g_cclosure_marshal_VOID__VOID,
                                                              G_TYPE_NONE, 0);

  g_type_class_add_private (klass, sizeof (HDTaskManagerPrivate));
}

/* Returns the singleton HDTaskManager instance. Should not be refed or unrefed */
HDTaskManager *
hd_task_manager_get (void)
{
  static HDTaskManager *manager = NULL;

  if (G_UNLIKELY (!manager))
    {
      manager = g_object_new (HD_TYPE_TASK_MANAGER, NULL);

      gdk_threads_add_idle ((GSourceFunc) hd_task_manager_scan_for_desktop_files,
                            HD_APPLICATIONS_DIR);
    }

  return manager;
}

GtkTreeModel *
hd_task_manager_get_model (HDTaskManager *manager)
{
  HDTaskManagerPrivate *priv = manager->priv;

  return g_object_ref (priv->filtered_model);
}

void
hd_task_manager_install_task (HDTaskManager *manager,
                              GtkTreeIter   *tree_iter)
{
  HDTaskManagerPrivate *priv = manager->priv;
  gchar *desktop_id;
  GHashTableIter hash_iter;
  gpointer key;
  GSList *list = NULL;
  GError *error = NULL;

  gtk_tree_model_get (priv->filtered_model, tree_iter,
                      2, &desktop_id,
                      -1);

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
      g_error_free (error);
    }

  /* Free the list, the content is still referenced by the hash table */
  g_slist_free (list);
}

const gchar *
hd_task_manager_get_label (HDTaskManager *manager,
                           const gchar   *desktop_id)
{
  HDTaskManagerPrivate *priv = manager->priv;
  HDTaskInfo *info;

  g_return_val_if_fail (HD_IS_TASK_MANAGER (manager), NULL);
  g_return_val_if_fail (desktop_id, NULL);

  /* Lookup task */
  info = g_hash_table_lookup (priv->available_tasks,
                              desktop_id);

  /* Return NULL if task is not available */
  if (!info)
    {
/*      g_warning ("Could not get label for %s", desktop_id); */
      return NULL;
    }

  return info->label;
}

const gchar *
hd_task_manager_get_icon (HDTaskManager *manager,
                          const gchar   *desktop_id)
{
  HDTaskManagerPrivate *priv = manager->priv;
  HDTaskInfo *info;

  g_return_val_if_fail (HD_IS_TASK_MANAGER (manager), NULL);
  g_return_val_if_fail (desktop_id, NULL);

  /* Lookup task */
  info = g_hash_table_lookup (priv->available_tasks,
                              desktop_id);

  /* Return NULL if task is not available */
  if (!info)
    {
/*      g_warning ("Could not get label for %s", desktop_id); */
      return NULL;
    }

  return info->icon;
}

static DBusGProxy *
hd_task_manager_get_app_mgr_proxy (HDTaskManager *manager)
{
  HDTaskManagerPrivate *priv = manager->priv;

  /* Get app mgr proxy if not there yet */
  if (!priv->app_mgr_proxy)
    {
      DBusGConnection *connection;
      GError *error = NULL;

      /* Connect to D-Bus */
      connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

      if (error)
        {
          g_warning ("%s. Could not connect to session bus. %s",
                     __FUNCTION__,
                     error->message);
          g_error_free (error);
          return NULL;
        }

      priv->app_mgr_proxy = dbus_g_proxy_new_for_name (connection,
                                                       APP_MGR_DBUS_NAME,
                                                       APP_MGR_DBUS_PATH,
                                                       APP_MGR_DBUS_NAME);
    }

  return priv->app_mgr_proxy;
}

void
hd_task_manager_launch_task (HDTaskManager *manager,
                             const gchar   *desktop_id)
{
  DBusGProxy *app_mgr_proxy;

  g_return_if_fail (HD_IS_TASK_MANAGER (manager));
  g_return_if_fail (desktop_id);

  app_mgr_proxy = hd_task_manager_get_app_mgr_proxy (manager);

  if (app_mgr_proxy)
    {
      /* App mgr takes the id without the .desktop suffix */
      gchar *id = g_strndup (desktop_id, strlen (desktop_id) - strlen (".desktop"));

      dbus_g_proxy_call_no_reply (app_mgr_proxy,
                                  APP_MGR_LAUNCH_APPLICATION,
                                  G_TYPE_STRING,
                                  id,
                                  G_TYPE_INVALID,
                                  G_TYPE_INVALID);

      g_free (id);
    }
}

