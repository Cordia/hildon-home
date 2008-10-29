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
#include "hd-system-notifications.h"
#include "hd-incoming-events.h"
#include "hd-shortcuts.h"

#define HD_STAMP_DIR   "/tmp/hildon-desktop/"
#define HD_HOME_STAMP_FILE HD_STAMP_DIR "hildon-home.stamp"

/* signal handler, hildon-desktop sends SIGTERM to all tracked applications
 * when it receives SIGTEM itselgf */
static void
signal_handler (int signal)
{
  if (signal == SIGTERM)
  {
    hd_stamp_file_finalize (HD_HOME_STAMP_FILE);

    exit (0);
  }
}

/*
static guint
load_priority_func (const gchar *plugin_id,
                    GKeyFile    *keyfile,
                    gpointer     data)
{
  return G_MAXUINT;
}
*/

static gboolean
load_plugins_idle (gpointer data)
{

  /* Load the configuration of the plugin manager and load plugins */
  hd_plugin_manager_run (HD_PLUGIN_MANAGER (data));

  return FALSE;
}

static void
home_plugin_added (HDPluginManager *pm,
                   GObject         *plugin,
                   gpointer         data)
{
  if (HD_IS_HOME_PLUGIN_ITEM (plugin))
    gtk_widget_show (GTK_WIDGET (plugin));
}

static void
home_plugin_removed (HDPluginManager *pm,
                     GObject         *plugin,
                     gpointer         data)
{
  if (HD_IS_HOME_PLUGIN_ITEM (plugin))
    gtk_widget_destroy (GTK_WIDGET (plugin));
}


int
main (int argc, char **argv)
{
  gchar *user_config_dir;
  HDConfigFile *config_file;
  HDPluginManager *notification_pm, *home_pm;
  HDNotificationManager *nm;
  HDSystemNotifications *sn;
  HDShortcuts *shortcuts;

  setlocale (LC_ALL, "");
  bindtextdomain (GETTEXT_PACKAGE, "/usr/share/locale");
  textdomain (GETTEXT_PACKAGE);

  g_thread_init (NULL);
  gtk_init (&argc, &argv);

  gnome_vfs_init ();

  signal (SIGTERM, signal_handler);

  hd_stamp_file_init (HD_HOME_STAMP_FILE);

  /* User configuration directory (~/) */
  user_config_dir = g_build_filename (g_get_user_config_dir (),
                                      "hildon-desktop",
                                      NULL);
  g_debug ("User config dir: %s", user_config_dir);

  config_file = hd_config_file_new (HD_DESKTOP_CONFIG_PATH,
                                    user_config_dir,
                                    "notification.conf");
  notification_pm = hd_plugin_manager_new (config_file);

  config_file = hd_config_file_new (HD_DESKTOP_CONFIG_PATH,
                                    user_config_dir,
                                    "home.conf");
  home_pm = hd_plugin_manager_new (config_file);

  g_free (user_config_dir);

  g_signal_connect (home_pm, "plugin-added",
                    G_CALLBACK (home_plugin_added), NULL);
  g_signal_connect (home_pm, "plugin-removed",
                    G_CALLBACK (home_plugin_removed), NULL);

  nm = hd_notification_manager_get ();
  sn = hd_system_notifications_get ();
  hd_incoming_events_new (notification_pm);

  /* Load Plugins when idle */
  gdk_threads_add_idle (load_plugins_idle, home_pm);
  gdk_threads_add_idle (load_plugins_idle, notification_pm);

  /* Task Shortcuts */
  shortcuts = hd_shortcuts_get ();

  /* Start the main loop */
  gtk_main ();

  return 0;
}
