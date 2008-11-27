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
#include <gconf/gconf-client.h>

#include <libintl.h>
#include <locale.h>
#include <signal.h>
#include <stdlib.h>

#include "hd-notification-manager.h"
#include "hd-system-notifications.h"
#include "hd-incoming-events.h"
#include "hd-bookmark-manager.h"
#include "hd-bookmark-shortcut.h"
#include "hd-task-manager.h"
#include "hd-task-shortcut.h"
#include "hd-hildon-home-dbus.h"
#include "hd-applet-manager.h"

#define HD_STAMP_DIR   "/tmp/hildon-desktop/"
#define HD_HOME_STAMP_FILE HD_STAMP_DIR "hildon-home.stamp"

#define HD_GCONF_DIR_HILDON_HOME "/apps/osso/hildon-home"
#define HD_GCONF_KEY_HILDON_HOME_TASK_SHORTCUTS HD_GCONF_DIR_HILDON_HOME "/task-shortcuts"
#define HD_GCONF_KEY_HILDON_HOME_BOOKMARK_SHORTCUTS HD_GCONF_DIR_HILDON_HOME "/bookmark-shortcuts"

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

int
main (int argc, char **argv)
{
  HDPluginManager *notification_pm;
  HDNotificationManager *nm;
  HDSystemNotifications *sn;
  GConfClient *client;
  GError *error = NULL;

  setlocale (LC_ALL, "");
  bindtextdomain (GETTEXT_PACKAGE, "/usr/share/locale");
  textdomain (GETTEXT_PACKAGE);

  g_thread_init (NULL);
  gtk_init (&argc, &argv);

  gnome_vfs_init ();

  signal (SIGTERM, signal_handler);

  hd_stamp_file_init (HD_HOME_STAMP_FILE);

  /* Initialize applet manager*/
  hd_applet_manager_get ();

  /* Intialize notifications */
  notification_pm = hd_plugin_manager_new (hd_config_file_new_with_defaults ("notification.conf"));

  nm = hd_notification_manager_get ();
  sn = hd_system_notifications_get ();
  hd_incoming_events_new (notification_pm);

  /* Load Plugins when idle */
  gdk_threads_add_idle (load_plugins_idle, notification_pm);

  /* Add shortcuts gconf dirs so hildon-home gets notifications about changes */
  client = gconf_client_get_default ();
  gconf_client_add_dir (client,
                        HD_GCONF_DIR_HILDON_HOME,
                        GCONF_CLIENT_PRELOAD_ONELEVEL,
                        &error);
  if (error)
    {
      g_warning ("Could not add gconf watch for dir " HD_GCONF_DIR_HILDON_HOME ". %s", error->message);
      g_error_free (error);
    }
  g_object_unref (client);

  /* Task Shortcuts */
  hd_task_manager_get ();
  hd_shortcuts_new (HD_GCONF_KEY_HILDON_HOME_TASK_SHORTCUTS,
                    HD_TYPE_TASK_SHORTCUT);

  /* Bookmark Shortcuts */
  hd_bookmark_manager_get ();
  hd_shortcuts_new (HD_GCONF_KEY_HILDON_HOME_BOOKMARK_SHORTCUTS,
                    HD_TYPE_BOOKMARK_SHORTCUT);

  /* D-Bus */
  hd_hildon_home_dbus_get ();

  /* Start the main loop */
  gtk_main ();

  return 0;
}
