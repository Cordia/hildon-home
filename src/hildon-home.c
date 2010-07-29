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

#include "hd-backgrounds.h"
#include "hd-notification-manager.h"
#include "hd-system-notifications.h"
#include "hd-incoming-events.h"
#include "hd-bookmark-widgets.h"
#include "hd-bookmark-shortcut.h"
#include "hd-shortcut-widgets.h"
#include "hd-task-shortcut.h"
#include "hd-hildon-home-dbus.h"
#include "hd-applet-manager.h"

#define HD_STAMP_DIR   "/tmp/hildon-desktop/"
#define HD_HOME_STAMP_FILE HD_STAMP_DIR "hildon-home.stamp"

#define OPERATOR_APPLET_MODULE_PATH "/usr/lib/hildon-desktop/connui-cellular-operator-home-item.so"
#define OPERATOR_APPLET_PLUGIN_ID "_HILDON_OPERATOR_APPLET"

#define HD_GCONF_DIR_HILDON_HOME "/apps/osso/hildon-home"
#define HD_GCONF_KEY_HILDON_HOME_TASK_SHORTCUTS HD_GCONF_DIR_HILDON_HOME "/task-shortcuts"
#define HD_GCONF_KEY_HILDON_HOME_BOOKMARK_SHORTCUTS HD_GCONF_DIR_HILDON_HOME "/bookmark-shortcuts"

HDShortcuts *hd_shortcuts_task_shortcuts;
static HDShortcuts *hd_shortcuts_bookmarks;

static gboolean enable_debug = FALSE;
static GOptionEntry entries[] =
{
  { "enable-debug", 'd', 0, G_OPTION_ARG_NONE, &enable_debug, "Enable debug output", NULL },
  { NULL }
};

/* signal handler, hildon-desktop sends SIGTERM to all tracked applications
 * when it receives SIGTEM itself */
static void
signal_handler (int signal)
{ /* Finish what we're doing but exit as soon as possible. */
  hd_stamp_file_finalize (HD_HOME_STAMP_FILE);
  g_idle_add_full (G_PRIORITY_DEFAULT,
                   (GSourceFunc)gtk_main_quit, NULL, NULL);
}

static void
load_operator_applet (void)
{
  GTypeModule *module;
  GObject *operator_applet;

  /* Load operator applet module */
  module = (GTypeModule *) hd_plugin_module_new (OPERATOR_APPLET_MODULE_PATH);

  if (!g_type_module_use (module))
    {
      g_warning ("Could not load operator module %s.", OPERATOR_APPLET_MODULE_PATH);

      return;
    }

  /* Create operator applet */
  operator_applet = hd_plugin_module_new_object (HD_PLUGIN_MODULE (module),
                                                 OPERATOR_APPLET_PLUGIN_ID);

  /* Show operator applet */
  if (GTK_IS_WIDGET (operator_applet))
    gtk_widget_show (GTK_WIDGET (operator_applet));

  g_type_module_unuse (module);
}

/* Log handler which ignores debug output */
static void
log_ignore_debug_handler (const gchar *log_domain,
                          GLogLevelFlags log_level,
                          const gchar *message,
                          gpointer unused_data)
{
  if (enable_debug || 
      log_level != G_LOG_LEVEL_DEBUG)
    g_log_default_handler (log_domain,
                           log_level,
                           message,
                           unused_data);
}

static gboolean
waitidle_go (gpointer unused)
{
  hildon_banner_show_information_with_markup (NULL, NULL,
      "<span font=\"120\">&#x263A;&#x263A;&#x263A;</span>");
  return FALSE;
}

static gboolean
waitidle_wait (gpointer unused)
{ /* Our banner is going away, wait until it completely disappears.
   * With the default transitions.init settings it takes 500ms. */  
  g_timeout_add (500, waitidle_go, NULL);
  return FALSE;
}

static gboolean 
check_error (GError **errp)
{
  if (*errp)
    {
      g_error_free (*errp);
      *errp = NULL;
      return TRUE;
    }
  else
    return FALSE;
}

/* g_timeout_add() callback to check the idle time of the CPU periodically
 * until it reaches a certain percentage, then show the desktop widgets. */
static gboolean
waitidle (GKeyFile *conf)
{
  static FILE *st;
  static GtkWidget *bar;
  static gboolean tuning, smiley;
  static gdouble *idles, threshold;
  static long long prev_total, prev_idle;
  static guint ttl, window, nidles, idlep;
  long long total, usr, nic, sys, idle, iowait, irq, softirq, steal;

  if (!st)
    { /* Initialize */
      GError *err;

      /* Where we can get the time spent in idle. */
      if (!(st = fopen ("/proc/stat", "r")))
        {
          g_critical ("/proc/stat: %m");
          g_key_file_free (conf);
          return FALSE;
        }

      /* Don't buffer, we'll reread the file periodically. */
      setvbuf (st, NULL, _IONBF, 0);

      /* Read @ttl, @window, @threshold from the configuration. */
      err = NULL;
      ttl       = g_key_file_get_integer (conf, "Waitidle", "timeout", &err);
      if (check_error(&err))
        ttl = 60;
      window  = g_key_file_get_integer (conf, "Waitidle", "window", &err);
      if (check_error(&err))
        window = 3;
      threshold = g_key_file_get_double (conf,  "Waitidle", "threshold", &err);
      if (check_error(&err))
        threshold = 0.1;
      tuning = g_key_file_get_boolean (conf,  "Waitidle", "tuning", NULL);
      smiley = g_key_file_get_boolean (conf,  "Waitidle", "smiley", NULL);

      idles = g_slice_alloc (sizeof (*idles) * window);
      bar = gtk_progress_bar_new ();
      if (tuning)
        /* We're running forever if @tuning, use @ttl as a counter. */
        ttl = 0;
    } /* Initialize */

  /* Read the jiffies. */
  if (fscanf (st, "cpu  "
              "%lld %lld %lld %lld %lld %lld %lld %lld",
              &usr, &nic, &sys, &idle, &iowait, &irq,
              &softirq, &steal) < 8)
    {
      g_critical ("/proc/stat: nonsense");
      goto done;
    }
  total = usr + nic + sys + idle + iowait + irq + softirq + steal;

  /* We need two consecutive samples to calculate idle%. */
  if (prev_total)
    {
      static GtkWidget *banner;
      static double prev_idlef;
      gdouble idlef;

      /* Calculate the ratio spent in idle. */
      if (!(idlef = total-prev_total))
        idlef = idle - prev_idle;
      else
        idlef = (gdouble)(idle-prev_idle) / idlef;

      /* Add it to the window. */
      idles[idlep++] = idlef;
      if (nidles < window)
        nidles++;

      /* If the window is full see if the average has reached @threshold. */
      if (bar && nidles >= window)
        {
          guint i;

          idlep %= nidles;
          for (idlef = i = 0; i < nidles; i++)
            idlef += idles[i];
          idlef /= nidles;
          if (idlef >= threshold)
            { /* Finish ourselves and show the widgets. */
              g_object_set (hd_shortcuts_task_shortcuts,
                            "throttled", FALSE, NULL);
              g_object_set (hd_shortcuts_bookmarks,
                            "throttled", FALSE, NULL);
              hd_applet_manager_throttled (
                           HD_APPLET_MANAGER (hd_applet_manager_get ()),
                           FALSE);
              gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (bar), 1.0);
              if (smiley) g_signal_connect (banner, "hide",
                                G_CALLBACK (waitidle_wait), NULL);
              if (tuning)
                { /* Just keep going on. */
                  g_warning ("waitidle done");
                  bar = NULL;
                }
              else
                goto done;
            }
        }

      if (bar != NULL)
        { /* Update the progress with the current idle%. */
          if (!banner || ABS(idlef-prev_idlef) >= 0.05)
            gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (bar), idlef);
          else
            gtk_progress_bar_pulse (GTK_PROGRESS_BAR (bar));
          banner = hildon_banner_show_custom_widget (NULL, bar);
        }

      prev_idlef = idlef;
      if (tuning)
        g_warning ("waitidle: %u. %f", ttl, idlef);
    }
  else if (tuning)
    g_warning ("waitidle started");

  /* Do we still have time to live? */
  if (tuning)
    ttl++;
  else if (ttl > 0)
    ttl--;
  else
    {
      g_warning ("waitidle: timeout reached");
      goto done;
    }

  /* Prepare for the next turn. */
  fseek(st, 0, SEEK_SET);
  prev_total = total;
  prev_idle  = idle;
  return TRUE;

done: /* Final clean up. */
  g_slice_free1 (sizeof (*idles) * window, idles);
  fclose (st);
  g_key_file_free (conf);
  return FALSE;
}

int
main (int argc, char **argv)
{
  GConfClient *client;
  GError *error = NULL;
  GKeyFile *conf;

  setlocale (LC_ALL, "");
  bindtextdomain (GETTEXT_PACKAGE, "/usr/share/locale");
  textdomain (GETTEXT_PACKAGE);

  /* Initialize threads */
  if (!g_thread_supported ())
    g_thread_init (NULL);

  /* Ignore debug output */
if (0)  g_log_set_default_handler (log_ignore_debug_handler, NULL);

  /* Initialize Gtk+ */
  gtk_init_with_args (&argc, &argv,
                      "Manage widgets, notifications and layout mode dialogs",
                      entries,
                      NULL,
                      NULL);

  /* Initialize Hildon */
  hildon_init ();

  /* Initialize GnomeVFS */
  gnome_vfs_init ();

  /* Add handler for signals */
  signal (SIGINT,  signal_handler);
  signal (SIGTERM, signal_handler);

  conf = NULL;
  if (1 /*!g_file_test (HD_HOME_STAMP_FILE, G_FILE_TEST_EXISTS)
      && !isatty (STDIN_FILENO)*/)
    {
      conf = g_key_file_new ();
      g_key_file_load_from_file (conf, "/etc/hildon-desktop/home.conf",
                                 G_KEY_FILE_NONE, NULL);
      if (!g_key_file_get_boolean (conf,  "Waitidle", "enabled", NULL))
        {
          g_key_file_free (conf);
          conf = NULL;
        }
    }
  hd_stamp_file_init (HD_HOME_STAMP_FILE);

  /* Backgrounds */
  hd_backgrounds_startup (hd_backgrounds_get ());

  /* Load operator applet */
  load_operator_applet ();

  /* Initialize applet manager */
  hd_applet_manager_throttled (HD_APPLET_MANAGER (hd_applet_manager_get ()),
                               !!conf);

  /* Intialize notifications */
  hd_notification_manager_get ();
  hd_system_notifications_get ();
  hd_incoming_events_get ();
  hd_notification_manager_db_load (hd_notification_manager_get ());

  /* Add shortcuts gconf dirs so hildon-home gets notifications about changes */
  client = gconf_client_get_default ();
  gconf_client_add_dir (client,
                        HD_GCONF_DIR_HILDON_HOME,
                        GCONF_CLIENT_PRELOAD_ONELEVEL,
                        &error);
  if (error)
    {
      g_warning ("Could not add gconf watch for dir %s. %s",
                 HD_GCONF_DIR_HILDON_HOME,
                 error->message);
      g_error_free (error);
    }
  g_object_unref (client);

  /* Task Shortcuts */
  hd_shortcut_widgets_get ();
  hd_shortcuts_task_shortcuts =
    g_object_new (HD_TYPE_SHORTCUTS,
                  "gconf-key",      HD_GCONF_KEY_HILDON_HOME_TASK_SHORTCUTS,
                  "shortcut-type",  HD_TYPE_TASK_SHORTCUT,
                  "throttled",      !!conf, NULL);

  /* Bookmark Shortcuts */
  hd_bookmark_widgets_get ();
  hd_shortcuts_bookmarks =
    g_object_new (HD_TYPE_SHORTCUTS,
                  "gconf-key",      HD_GCONF_KEY_HILDON_HOME_BOOKMARK_SHORTCUTS,
                  "shortcut-type",  HD_TYPE_BOOKMARK_SHORTCUT,
                  "throttled",      !!conf, NULL);

  /* D-Bus */
  hd_hildon_home_dbus_get ();

  /* Start the main loop */
  if (conf)
    g_timeout_add_seconds (1, (GSourceFunc)waitidle, conf);
  gtk_main ();
  
  hd_stamp_file_finalize (HD_HOME_STAMP_FILE);

  /* We got a signal, flush the database.  How we do it breaks
   * if somebody has taken reference of the nm, but we don't. */
  g_object_unref (hd_notification_manager_get ());

  return 0;
}
