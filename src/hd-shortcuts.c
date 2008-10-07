/*
 * This file is part of hildon-home
 *
 * Copyright (C) 2008 Nokia Corporation.
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

#include <glib.h>
#include <gconf/gconf-client.h>

#include <string.h>

#include "hd-task-shortcut.h"

#include "hd-shortcuts.h"

#define HD_GCONF_DIR_HILDON_HOME "/apps/osso/hildon-home"
#define HD_GCONF_KEY_HILDON_HOME_TASK_SHORTCUTS HD_GCONF_DIR_HILDON_HOME "/task-shortcuts"
#define HD_GCONF_KEY_HILDON_HOME_BOOKMARK_SHORTCUTS HD_GCONF_DIR_HILDON_HOME "/bookmark-shortcuts"

#define HD_SHORTCUTS_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_SHORTCUTS, HDShortcutsPrivate))

struct _HDShortcutsPrivate
{
  GHashTable *applets;

  GConfClient *gconf_client;
};

G_DEFINE_TYPE (HDShortcuts, hd_shortcuts, G_TYPE_OBJECT);

/* Compare lists new and old and move elements unique in old
 * to to_remove and elements unique in new to to_add, elements
 * common in new and old are removed.
 *
 * old is destroyed by this function
 * new is destroyed by this function
 */
static void
create_sync_lists (GSList         *old,
                   GSList         *new,
                   GSList        **to_add,
                   GSList        **to_remove,
                   GCompareFunc    cmp_func,
                   GDestroyNotify  destroy_func)
{
  GSList *add = NULL;
  GSList *remove = NULL;

  g_return_if_fail (to_add != NULL);
  g_return_if_fail (to_remove != NULL);

  /* sort lists */
  old = g_slist_sort (old, cmp_func);
  new = g_slist_sort (new, cmp_func);

  while (old && new)
    {
      gint c = cmp_func (old->data, new->data);

      /* there is an element only in new 
       * move it to list to_add */
      if (c > 0)
        {
          GSList *n = new;
          new = g_slist_remove_link (new, new);
          add = g_slist_concat (n, add);
        }
      /* there is an element only in old 
       * move it to list to_remove */
      else if (c < 0)
        {
          GSList *o = old;
          old = g_slist_remove_link (old, old);
          remove = g_slist_concat (o, remove);
        }
      /* the next element is in old and new
       * remove it */
      else
        {
          destroy_func (old->data);
          destroy_func (new->data);

          old = g_slist_delete_link (old, old);
          new = g_slist_delete_link (new, new);
        }
    }

  /* add remaining elements to the approbiate lists */
  *to_add = g_slist_concat (new, add);
  *to_remove = g_slist_concat (old, remove);
}

static void
task_shortcuts_sync (HDShortcuts *shortcuts,
                     GSList      *new)
{
  HDShortcutsPrivate *priv = shortcuts->priv;
  GHashTableIter iter;
  gpointer key;
  GSList *old = NULL;
  GSList *to_add, *to_remove;
  GSList *s;

  g_hash_table_iter_init (&iter, priv->applets);
  while (g_hash_table_iter_next (&iter, &key, NULL)) 
    {
      old = g_slist_append (old, g_strdup (key));
    }

  create_sync_lists (old, new,
                     &to_add, &to_remove,
                     (GCompareFunc) strcmp,
                     (GDestroyNotify) g_free);

  for (s = to_remove; s; s = s->next)
    {
      g_hash_table_remove (priv->applets,
                           s->data);
      g_free (s->data);
    }

  for (s = to_add; s; s = s->next)
    {
      HDTaskShortcut *shortcut;

      shortcut = hd_task_shortcut_new (s->data);

      g_hash_table_insert (priv->applets,
                           s->data,
                           shortcut);

      gtk_widget_show (GTK_WIDGET (shortcut));
    }

  g_slist_free (to_remove);
  g_slist_free (to_add);
}

static void
task_shortcuts_notify (GConfClient *client,
                       guint        cnxn_id,
                       GConfEntry  *entry,
                       HDShortcuts *shortcuts)
{
  HDShortcutsPrivate *priv = shortcuts->priv;
  GSList *list;
  GError *error = NULL;

  /* Get the list of strings of task shortcuts */
  list = gconf_client_get_list (priv->gconf_client,
                                HD_GCONF_KEY_HILDON_HOME_TASK_SHORTCUTS,
                                GCONF_VALUE_STRING,
                                &error);

  /* Check if there was an error */
  if (!error)
    task_shortcuts_sync (shortcuts, list);
  else
    {
      g_warning ("Could not get list of task shortcuts from GConf: %s", error->message);
      g_error_free (error);
    }
}

static void
hd_shortcuts_dispose (GObject *object)
{
  HDShortcutsPrivate *priv = HD_SHORTCUTS (object)->priv;

  if (priv->gconf_client)
    {
      g_object_unref (priv->gconf_client);
      priv->gconf_client = NULL;
    }

  G_OBJECT_CLASS (hd_shortcuts_parent_class)->dispose (object);
}

static void
hd_shortcuts_finalize (GObject *object)
{
  HDShortcutsPrivate *priv = HD_SHORTCUTS (object)->priv;

  if (priv->applets)
    {
      g_hash_table_destroy (priv->applets);
      priv->applets = NULL;
    }

  G_OBJECT_CLASS (hd_shortcuts_parent_class)->finalize (object);
}

static void
hd_shortcuts_class_init (HDShortcutsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = hd_shortcuts_dispose;
  object_class->finalize = hd_shortcuts_finalize;

  g_type_class_add_private (klass, sizeof (HDShortcutsPrivate));
}

static void
hd_shortcuts_init (HDShortcuts *shortcuts)
{
  HDShortcutsPrivate *priv;
  GSList *list;
  GError *error = NULL;

  shortcuts->priv = HD_SHORTCUTS_GET_PRIVATE (shortcuts);
  priv = shortcuts->priv;

  /* Create Hashtable for task shortcut applets */
  priv->applets = g_hash_table_new_full (g_str_hash,
                                         g_str_equal,
                                         (GDestroyNotify) g_free,
                                         (GDestroyNotify) gtk_widget_destroy);

  /* GConf configuration in /apps/osso/hildon-home */
  priv->gconf_client = gconf_client_get_default ();
  gconf_client_add_dir (priv->gconf_client,
                        HD_GCONF_DIR_HILDON_HOME,
                        GCONF_CLIENT_PRELOAD_NONE,
                        &error);
  /* Check if there was an error */
  if (error)
    {
      g_warning ("Could not add dir for GConfClient to watch: %s", error->message);
      g_error_free (error);
      error = NULL;
    }

  /* Add notification of the task-shortcuts key */
  gconf_client_notify_add (priv->gconf_client,
                           HD_GCONF_KEY_HILDON_HOME_TASK_SHORTCUTS,
                           (GConfClientNotifyFunc) task_shortcuts_notify,
                           shortcuts,
                           NULL,
                           NULL);

  /* Get the list of strings of task shortcuts */
  list = gconf_client_get_list (priv->gconf_client,
                                HD_GCONF_KEY_HILDON_HOME_TASK_SHORTCUTS,
                                GCONF_VALUE_STRING,
                                &error);

  /* Check if there was an error */
  if (!error)
    task_shortcuts_sync (shortcuts, list);
  else
    {
      g_warning ("Could not get list of task shortcuts from GConf: %s", error->message);
      g_error_free (error);
    }
}

HDShortcuts *
hd_shortcuts_get ()
{
  HDShortcuts *shortcuts = g_object_new (HD_TYPE_SHORTCUTS, NULL);

  return shortcuts;
}
