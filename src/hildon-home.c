/*
 * This file is part of hildon-home
 * 
 * Copyright (C) 2006, 2007, 2008 Nokia Corporation.
 *
 * Based on main.c from hildon-desktop.
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

#include <glib/gstdio.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libhildondesktop/libhildondesktop.h>
#include <hildon/hildon.h>

#include <libintl.h>
#include <locale.h>
#include <signal.h>
#include <stdlib.h>

#include "hd-notification-manager.h"

#define HD_STAMP_DIR   "/tmp/osso-appl-states/hildon-desktop/"
#define HD_NOTIFICATION_STAMP_FILE HD_STAMP_DIR "hildon-home-notification.stamp"

/* signal handler, hildon-desktop sends SIGTERM to all tracked applications
 * when it receives SIGTEM itselgf */
static void
signal_handler (int signal)
{
  if (signal == SIGTERM)
  {
    /* 
     * Clean up stamp file created by hd_plugin_manager_run
     * On next startup the stamp file is created again and hildon-desktop remains
     * in normal operation mode without entering into safe mode where some plugins
     * are disabled.
     */
    if (g_file_test (HD_NOTIFICATION_STAMP_FILE, G_FILE_TEST_EXISTS))
      {
        g_unlink (HD_NOTIFICATION_STAMP_FILE);
      }

    exit (0);
  }
}

static guint
load_priority_func (const gchar *plugin_id,
                    GKeyFile    *keyfile,
                    gpointer     data)
{
  return G_MAXUINT;
}

static gboolean
load_plugins_idle (gpointer data)
{

  /* Load the configuration of the plugin manager and load plugins */
  hd_plugin_manager_run (HD_PLUGIN_MANAGER (data));

  return FALSE;
}

#if 0
static gboolean
hd_desktop_system_notifications_filter (GtkTreeModel *model,
				        GtkTreeIter *iter,
				        gpointer user_data)
{
  GHashTable *hints;
  GValue *category;

  gtk_tree_model_get (model,
		      iter,
		      HD_NM_COL_HINTS, &hints,
		      -1);

  if (hints == NULL) 
    return FALSE;
  
  category = g_hash_table_lookup (hints, "category");

  if (category == NULL)
  {
    return FALSE;
  }
  else if (g_str_has_prefix (g_value_get_string (category), "system"))
  {
    return TRUE;
  } 
  else
  {
    return FALSE;
  }
}

static void
hd_desktop_system_notification_closed (HDNotificationManager *nm,
				       gint id,
				       HDDesktop *desktop)
{
  gpointer widget;

  widget = g_hash_table_lookup (desktop->priv->notifications, GINT_TO_POINTER (id));

  g_hash_table_remove (desktop->priv->notifications, GINT_TO_POINTER (id));

  if (GTK_IS_WIDGET (widget))
  {
    gtk_widget_destroy (GTK_WIDGET (widget));
  }
}

static void
hd_desktop_system_notification_received (GtkTreeModel *model,
                                         GtkTreePath *path,
                                         GtkTreeIter *iter,
                                         gpointer user_data)  

{
  HDDesktop *desktop;
  GtkWidget *notification = NULL;
  GHashTable *hints;
  GValue *hint;
  gchar **actions;
  const gchar *hint_s;
  gchar *summary;
  gchar *body;
  gchar *icon_name;
  gint id;

  g_return_if_fail (HD_IS_DESKTOP (user_data));
  
  desktop = HD_DESKTOP (user_data);

  gtk_tree_model_get (model,
		      iter,
		      HD_NM_COL_ID, &id,
		      HD_NM_COL_SUMMARY, &summary,
		      HD_NM_COL_BODY, &body,
		      HD_NM_COL_ICON_NAME, &icon_name,
		      HD_NM_COL_ACTIONS, &actions,
		      HD_NM_COL_HINTS, &hints,
		      -1);

  hint = g_hash_table_lookup (hints, "category");
  hint_s = g_value_get_string (hint);

  if (g_str_equal (hint_s, "system.note.infoprint")) 
  {
    notification = hd_desktop_create_note_infoprint (summary, 
		    				     body, 
						     icon_name);

    gtk_widget_show_all (notification);
  }
  else if (g_str_equal (hint_s, "system.note.dialog")) 
  {
    HDDesktopNotificationInfo *ninfo;
    gint dialog_type = 0;
    
    hint = g_hash_table_lookup (hints, "dialog-type");
    dialog_type = g_value_get_int (hint);
    
    notification = hd_desktop_create_note_dialog (summary, 
		    				  body, 
						  icon_name,
						  dialog_type,
						  actions);

    ninfo = g_new0 (HDDesktopNotificationInfo, 1); 

    ninfo->id = id;
    ninfo->desktop = desktop;
  
    g_signal_connect (G_OBJECT (notification),
  		      "response",
  		      G_CALLBACK (hd_desktop_system_notification_dialog_response),
  		      ninfo);

    g_signal_connect (G_OBJECT (notification),
  		      "destroy",
  		      G_CALLBACK (hd_desktop_system_notification_dialog_destroy),
  		      desktop);

    if (g_queue_is_empty (desktop->priv->dialog_queue))
    {
      gtk_widget_show_all (notification);
    }

    g_queue_push_tail (desktop->priv->dialog_queue, notification);
  } 
  else
  {
    goto clean;
  }

  g_hash_table_insert (desktop->priv->notifications, 
		       GINT_TO_POINTER (id), 
		       notification);

clean:
  g_free (summary);
  g_free (body);
  g_free (icon_name);
}
#endif

int
main (int argc, char **argv)
{
  HDConfigFile *config_file;
  HDPluginManager *plugin_manager;
  gchar *user_config_dir;

  g_thread_init (NULL);
  setlocale (LC_ALL, "");

  gtk_init (&argc, &argv);

  gnome_vfs_init ();

  signal (SIGTERM, signal_handler);

  /* User configuration directory (~/) */
  user_config_dir = g_build_filename (g_get_user_config_dir (),
                                      "hildon-desktop",
                                      NULL);
  g_debug ("User config dir: %s", user_config_dir);

  /* Create a config file object for the plugin manager */
  config_file = hd_config_file_new (HD_DESKTOP_CONFIG_PATH,
                                    user_config_dir,
                                    "notification.conf");
  plugin_manager = hd_plugin_manager_new (config_file, HD_NOTIFICATION_STAMP_FILE);
  g_object_unref (config_file);
  g_free (user_config_dir);

  /* Set the load priority function */
  hd_plugin_manager_set_load_priority_func (plugin_manager,
                                            load_priority_func,
                                            NULL,
                                            NULL);

  /* Load Plugins when idle */
  gdk_threads_add_idle (load_plugins_idle, plugin_manager);

#if 0
  nm = gtk_tree_model_filter_new (GTK_TREE_MODEL (hd_notification_manager_get_singleton ()), NULL);

  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (nm),
		                          hd_desktop_system_notifications_filter,
					  NULL,
					  NULL);


  g_signal_connect (hd_notification_manager_get_singleton (),
		  "notification-closed",
		  G_CALLBACK (hd_desktop_system_notification_closed),
		  desktop);

  g_signal_connect (nm,
		  "row-inserted",
		  G_CALLBACK (hd_desktop_system_notification_received),
		  desktop);
#endif

  /* Start the main loop */
  gtk_main ();

  return 0;
}
