/*
 * This file is part of hildon-desktop
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

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>

#include <gconf/gconf-client.h>

#include <string.h>

#include <osso_bookmark_parser.h>

#include "hd-bookmark-manager.h"

#define HD_BOOKMARK_MANAGER_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_BOOKMARK_MANAGER, HDBookmarkManagerPrivate))

#define BOOKMARK_SHORTCUTS_GCONF_KEY "/apps/osso/hildon-home/bookmark-shortcuts"

#define BOOKMARK_EXTENSION_LEN 3

struct _HDBookmarkManagerPrivate
{
  HDPluginConfiguration *plugin_configuration;

  GtkTreeModel *model;

  GHashTable *available_bookmarks;
};

typedef struct
{
  gchar *name;
  gchar *icon;
  gchar *url;
} HDBookmarkInfo;

G_DEFINE_TYPE (HDBookmarkManager, hd_bookmark_manager, G_TYPE_OBJECT);

static void
hd_bookmark_manager_add_bookmark_item (HDBookmarkManager *manager,
                                       BookmarkItem      *item)
{
  HDBookmarkManagerPrivate *priv = manager->priv;
  HDBookmarkInfo *info;

  g_debug ("hd_bookmark_manager_add_bookmark_item");

  /* If it is a folder recurse over all children */
  if (item->isFolder)
    {
      GSList *c;

      for (c = item->list; c; c = c->next)
        {
          hd_bookmark_manager_add_bookmark_item (manager,
                                                 c->data);
        }

      return;
    }

  /* Else add the bookmark info to our table */
  info = g_slice_new0 (HDBookmarkInfo);

  info->name = g_strndup (item->name, strlen (item->name) - BOOKMARK_EXTENSION_LEN);
  info->icon = g_strdup (item->favicon_file);
  info->url = g_strdup (item->url);

  g_hash_table_insert (priv->available_bookmarks,
                       info->url,
                       info);

  gtk_list_store_insert_with_values (GTK_LIST_STORE (priv->model),
                                     NULL, -1,
                                     0, info->name,
                                     1, info->icon,
                                     2, info->url,
                                     -1);

  return;
}

static gboolean
hd_bookmark_manager_parse_bookmark_files (HDBookmarkManager *manager)
{
  BookmarkItem *root = create_bookmark_new ();

  if (!get_root_bookmark (&root,
                          MYBOOKMARKS))
    {
      g_debug ("COuld not read bookmark file");
    }

  hd_bookmark_manager_add_bookmark_item (manager,
                                         root);

  return FALSE;
}

static void
hd_bookmark_manager_init (HDBookmarkManager *manager)
{
  HDBookmarkManagerPrivate *priv;
  manager->priv = HD_BOOKMARK_MANAGER_GET_PRIVATE (manager);
  priv = manager->priv;

  priv->available_bookmarks = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                     g_free, NULL);

  priv->model = GTK_TREE_MODEL (gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING));
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (priv->model),
                                        0,
                                        GTK_SORT_ASCENDING);

}

static void
hd_bookmark_manager_class_init (HDBookmarkManagerClass *klass)
{
  g_type_class_add_private (klass, sizeof (HDBookmarkManagerPrivate));
}

HDBookmarkManager *
hd_bookmark_manager_get (void)
{
  static HDBookmarkManager *manager = NULL;

  if (G_UNLIKELY (!manager))
    {
      manager = g_object_new (HD_TYPE_BOOKMARK_MANAGER, NULL);

      g_idle_add ((GSourceFunc) hd_bookmark_manager_parse_bookmark_files,
                  manager);
    }

  return manager;
}

GtkTreeModel *
hd_bookmark_manager_get_model (HDBookmarkManager *manager)
{
  HDBookmarkManagerPrivate *priv = manager->priv;

  return g_object_ref (priv->model);
}

void
hd_bookmark_manager_install_bookmark (HDBookmarkManager *manager,
                              GtkTreeIter     *iter)
{
  HDBookmarkManagerPrivate *priv = manager->priv;
  GConfClient *client;
  gchar *url;
  GSList *list;
  GError *error = NULL;

  client = gconf_client_get_default ();

  gtk_tree_model_get (priv->model, iter,
                      2, &url,
                      -1);
  
  /* Get the current list of bookmark shortcuts from GConf */
  list = gconf_client_get_list (client,
                                BOOKMARK_SHORTCUTS_GCONF_KEY,
                                GCONF_VALUE_STRING,
                                &error);

  if (error)
    {
      g_warning ("Could not get string list from GConf (%s): %s.",
                 BOOKMARK_SHORTCUTS_GCONF_KEY,
                 error->message);
      g_error_free (error);
      error = NULL;
    }

  /* Append the new bookmark */
  list = g_slist_append (list, url);

  /* Set the new list to GConf */
  gconf_client_set_list (client,
                         BOOKMARK_SHORTCUTS_GCONF_KEY,
                         GCONF_VALUE_STRING,
                         list,
                         &error);

  if (error)
    {
      g_warning ("Could not write string list from GConf (%s): %s.",
                 BOOKMARK_SHORTCUTS_GCONF_KEY,
                 error->message);
      g_error_free (error);
      error = NULL;
    }

  /* Free */
  g_slist_foreach (list, (GFunc) g_free, NULL);
  g_slist_free (list);

  g_object_unref (client);
}
