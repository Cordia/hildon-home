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

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>

#include <gconf/gconf-client.h>
#include <libgnomevfs/gnome-vfs.h>

#include <libhildondesktop/libhildondesktop.h>

#include <string.h>

#include <osso_bookmark_parser.h>

#include "hd-bookmark-widgets.h"

#define HD_BOOKMARK_WIDGETS_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_BOOKMARK_WIDGETS, HDBookmarkWidgetsPrivate))

#define BOOKMARK_SHORTCUTS_GCONF_KEY "/apps/osso/hildon-home/bookmark-shortcuts"

#define BOOKMARK_EXTENSION_LEN 3

/* GConf path for boomarks */
#define BOOKMARKS_GCONF_PATH      "/apps/osso/hildon-home/bookmarks"
#define BOOKMARKS_GCONF_KEY_LABEL BOOKMARKS_GCONF_PATH "/%s/label"
#define BOOKMARKS_GCONF_KEY_URL   BOOKMARKS_GCONF_PATH "/%s/url"
#define BOOKMARKS_GCONF_KEY_ICON  BOOKMARKS_GCONF_PATH "/%s/icon"

/* Definitions for the ID generation */ 
#define ID_VALID_CHARS "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_+?"
#define ID_SUBSTITUTOR '_'

struct _HDBookmarkWidgetsPrivate
{
  GtkTreeModel *model;

  GnomeVFSMonitorHandle *user_bookmarks_handle;

  guint parse_idle_id;
};

G_DEFINE_TYPE (HDBookmarkWidgets, hd_bookmark_widgets, HD_TYPE_WIDGETS);

static void
hd_bookmark_widgets_add_bookmark_item (HDBookmarkWidgets *widgets,
                                       BookmarkItem      *item)
{
  HDBookmarkWidgetsPrivate *priv = widgets->priv;
  GdkPixbuf *pixbuf = NULL;
  gchar *name;
  gchar *icon_path = NULL;

  /* If it is a folder recurse over all children */
  if (item->isFolder)
    {
      GSList *c;

      g_debug ("%s. Folder.", __FUNCTION__);

      for (c = item->list; c; c = c->next)
        {
          hd_bookmark_widgets_add_bookmark_item (widgets,
                                                 c->data);
        }

      return;
    }

  g_debug ("%s. Name: %s", __FUNCTION__, item->name);

  /* A deleted operator bookmark */
  if (item->isDeleted)
    return;

  name = g_strndup (item->name, strlen (item->name) - BOOKMARK_EXTENSION_LEN);

  if (item->thumbnail_file)
    {
      icon_path = g_build_filename (g_get_home_dir (),
                                    THUMBNAIL_PATH,
                                    item->thumbnail_file,
                                    NULL);

      pixbuf = gdk_pixbuf_new_from_file_at_size (icon_path, 106, 64, NULL);
    }

  gtk_list_store_insert_with_values (GTK_LIST_STORE (priv->model),
                                     NULL, -1,
                                     0, name,
                                     1, icon_path,
                                     2, item->url,
                                     3, pixbuf,
                                     -1);

  g_free (name);
  g_free (icon_path);
  if (pixbuf)
    g_object_unref (pixbuf);
}

static void
free_bookmark_item (BookmarkItem *bookmark)
{
    if (!bookmark)
        return;

    bookmark->name = (g_free (bookmark->name), NULL);
    bookmark->url = (g_free(bookmark->url), NULL);
    bookmark->favicon_file = (g_free(bookmark->favicon_file), NULL);
    bookmark->thumbnail_file = (g_free(bookmark->thumbnail_file), NULL);

    if (bookmark->list) {
        g_slist_foreach (bookmark->list, (GFunc) free_bookmark_item, NULL);
        g_slist_free (bookmark->list);
        bookmark->list = NULL;
    }

    g_free (bookmark);
}

static gboolean
hd_bookmark_widgets_parse_bookmark_files (HDBookmarkWidgets *widgets)
{
  HDBookmarkWidgetsPrivate *priv = widgets->priv;
  BookmarkItem *root = NULL;

  /* Unset the thread id so the files are parsed again if there is a change */
  priv->parse_idle_id = 0;

  /* Clear all entries from the model */
  gtk_list_store_clear (GTK_LIST_STORE (priv->model));

  /* Try to load user bookmarks from file */
  if (!get_root_bookmark (&root, MYBOOKMARKS))
    get_bookmark_from_backup(&root, MYBOOKMARKSFILEBACKUP);

  if (root != NULL)
  {
    hd_bookmark_widgets_add_bookmark_item (widgets, root);
    free_bookmark_item (root);
  }
  else
    g_warning ("Could not read users bookmarks from file");

  return FALSE;
}

static void
hd_bookmark_widgets_bookmark_files_changed (GnomeVFSMonitorHandle *handle,
                                            const gchar *monitor_uri,
                                            const gchar *info_uri,
                                            GnomeVFSMonitorEventType event_type,
                                            gpointer user_data)
{
  HDBookmarkWidgets *widgets = HD_BOOKMARK_WIDGETS (user_data);
  HDBookmarkWidgetsPrivate *priv = widgets->priv;

  g_debug ("%s. Type: %u", __FUNCTION__, event_type);

  if (!priv->parse_idle_id)
    priv->parse_idle_id = gdk_threads_add_idle ((GSourceFunc) hd_bookmark_widgets_parse_bookmark_files,
                                                user_data);
}

static void
hd_bookmark_widgets_constructed (GObject *object)
{
  HDBookmarkWidgetsPrivate *priv = HD_BOOKMARK_WIDGETS (object)->priv;
  gchar *user_bookmarks, *user_bookmarks_uri;
  GnomeVFSResult result;

  if (G_OBJECT_CLASS (hd_bookmark_widgets_parent_class)->constructed)
    G_OBJECT_CLASS (hd_bookmark_widgets_parent_class)->constructed (object);

  /* Create the bookmark paths if they do not exist yet */
  set_bookmark_files_path ();
  
  user_bookmarks = g_build_filename (g_get_home_dir (),
                                     MYBOOKMARKS,
                                     NULL);
  user_bookmarks_uri = gnome_vfs_get_uri_from_local_path (user_bookmarks);

  result = gnome_vfs_monitor_add (&priv->user_bookmarks_handle,
                                  user_bookmarks_uri,
                                  GNOME_VFS_MONITOR_FILE,
                                  hd_bookmark_widgets_bookmark_files_changed,
                                  object);
  if (result != GNOME_VFS_OK)
    g_debug ("Could not add monitor for user bookmark file. %s", gnome_vfs_result_to_string (result));

  priv->parse_idle_id = gdk_threads_add_idle ((GSourceFunc) hd_bookmark_widgets_parse_bookmark_files,
                                              object);

  g_free (user_bookmarks);
  g_free (user_bookmarks_uri);
}

static void
hd_bookmark_widgets_init (HDBookmarkWidgets *widgets)
{
  HDBookmarkWidgetsPrivate *priv;
  widgets->priv = HD_BOOKMARK_WIDGETS_GET_PRIVATE (widgets);
  priv = widgets->priv;

  priv->model = GTK_TREE_MODEL (gtk_list_store_new (4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, GDK_TYPE_PIXBUF));
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (priv->model),
                                        0,
                                        GTK_SORT_ASCENDING);

}

static void
hd_bookmark_widgets_dipose (GObject *object)
{
  HDBookmarkWidgetsPrivate *priv = HD_BOOKMARK_WIDGETS (object)->priv;

  if (priv->model)
    priv->model = (g_object_unref (priv->model), NULL);

  if (priv->user_bookmarks_handle)
    priv->user_bookmarks_handle = (gnome_vfs_monitor_cancel (priv->user_bookmarks_handle), NULL);

  if (priv->parse_idle_id)
    {
      g_source_remove (priv->parse_idle_id);
      priv->parse_idle_id = 0;
    }

  G_OBJECT_CLASS (hd_bookmark_widgets_parent_class)->dispose (object);
}


static GtkTreeModel *
hd_bookmark_widgets_get_model (HDWidgets *widgets)
{
  HDBookmarkWidgetsPrivate *priv = HD_BOOKMARK_WIDGETS (widgets)->priv;

  return g_object_ref (priv->model);
}


static const gchar *
hd_bookmark_widgets_get_dialog_title (HDWidgets *widgets)
{
  return dgettext (GETTEXT_PACKAGE, "home_ti_select_bookmark");
}

static void
hd_bookmark_widgets_setup_column_renderes (HDWidgets     *widgets,
                                           GtkCellLayout *column)
{
  GtkCellRenderer *renderer;
  
  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_layout_pack_start (column,
                              renderer,
                              FALSE);
  gtk_cell_layout_add_attribute (column,
                                 renderer,
                                 "pixbuf", 3);

  /* Add the label renderer */
  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (column,
                              renderer,
                              FALSE);
  gtk_cell_layout_add_attribute (column,
                                 renderer,
                                 "text", 0);
}

static void
hd_bookmark_widgets_install_widget (HDWidgets   *widgets,
                                    GtkTreePath *path)
{
  HDBookmarkWidgetsPrivate *priv = HD_BOOKMARK_WIDGETS (widgets)->priv;
  GtkTreeIter iter;
  gchar *label, *icon, *url;

  gtk_tree_model_get_iter (priv->model, &iter, path);

  gtk_tree_model_get (priv->model, &iter,
                      0, &label,
                      1, &icon,
                      2, &url,
                      -1);

  hd_shortcuts_add_bookmark_shortcut (url,
                                      label,
                                      icon);

  /* Free memory */
  g_free (label);
  g_free (icon);
  g_free (url);
}

static void
hd_bookmark_widgets_class_init (HDBookmarkWidgetsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  HDWidgetsClass *widgets_class = HD_WIDGETS_CLASS (klass);

  object_class->constructed = hd_bookmark_widgets_constructed;
  object_class->dispose = hd_bookmark_widgets_dipose;

  widgets_class->get_dialog_title = hd_bookmark_widgets_get_dialog_title;
  widgets_class->get_model = hd_bookmark_widgets_get_model;
  widgets_class->setup_column_renderes = hd_bookmark_widgets_setup_column_renderes;
  widgets_class->install_widget = hd_bookmark_widgets_install_widget;

  g_type_class_add_private (klass, sizeof (HDBookmarkWidgetsPrivate));
}

/* Retuns the singleton HDBookmarkWidgets instance. Should not be refed or unrefed */
HDWidgets *
hd_bookmark_widgets_get (void)
{
  static HDWidgets *bookmark_widgets = NULL;

  if (G_UNLIKELY (!bookmark_widgets))
    bookmark_widgets = g_object_new (HD_TYPE_BOOKMARK_WIDGETS, NULL);

  return bookmark_widgets;
}
