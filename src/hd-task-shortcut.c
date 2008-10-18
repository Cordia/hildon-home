/*
 * This file is part of hildon-home.
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

#include <string.h>

#include <gtk/gtk.h>
#include <hildon/hildon.h>
#include <dbus/dbus.h>

#include <glib/gi18n.h>

#include "hd-task-shortcut.h"

/* Applications directory */
#define APPLICATIONS_SUBDIR "applications", "hildon"

/* Plugin ID task-shortcut prefix */
#define PLUGIN_ID_FORMAT "-x-task-shortcut-%s"

/* Task .desktop file keys */
#define HD_KEY_FILE_DESKTOP_KEY_SERVICE "X-Osso-Service"
#define HD_KEY_FILE_DESKTOP_KEY_TRANSLATION_DOMAIN "X-Osso-Translation-Domain"

#define HD_TASK_SHORTCUT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE (obj,\
                                                                        HD_TYPE_TASK_SHORTCUT,\
                                                                        HDTaskShortcutPrivate))

struct _HDTaskShortcutPrivate
{
  gchar *desktop_id;

  gchar *name;

  gchar *exec;
  gchar *service;

  gchar *translation_domain;

  GtkWidget *label;
  GtkWidget *icon;

  gboolean button_pressed;
};

enum
{
  PROP_0,
  PROP_DESKTOP_ID,
};

G_DEFINE_TYPE (HDTaskShortcut, hd_task_shortcut, HD_TYPE_HOME_PLUGIN_ITEM);

static gpointer
get_applications_dirs_once (gpointer data)
{
  gchar **applications_dirs;
  const gchar *user_data_dir;
  const gchar * const *system_data_dirs;
  guint length, i;

  user_data_dir = g_get_user_data_dir ();
  system_data_dirs = g_get_system_data_dirs ();
  length = g_strv_length ((gchar **)system_data_dirs);

  /* Create an array with the user data dir, the system data dirs
   * and a NULL entry */
  applications_dirs = g_new0 (gchar *, length + 2);

  applications_dirs[0] = g_build_filename (user_data_dir,
                                           APPLICATIONS_SUBDIR,
                                           NULL);
  for (i = 0; i < length; i++)
    applications_dirs[i + 1] = g_build_filename (system_data_dirs[i],
                                                 APPLICATIONS_SUBDIR,
                                                 NULL);

  return applications_dirs;
}

static const gchar * const *
get_applications_dirs (void)
{
  static GOnce applications_dirs = G_ONCE_INIT;

  g_once (&applications_dirs, get_applications_dirs_once, NULL);

  return applications_dirs.retval;
}

static gboolean
hd_task_shortcut_load_from_file (HDTaskShortcut *shortcut,
                                 const gchar    *file_name)
{
  HDTaskShortcutPrivate *priv = shortcut->priv;
  GKeyFile *key_file;
  GError *error = NULL;

  key_file = g_key_file_new ();
  if (g_key_file_load_from_file (key_file,
                                 file_name,
                                 0,
                                 &error))
    {
      gchar *icon_name;
      gchar *domain = GETTEXT_PACKAGE;

      priv->name = g_key_file_get_locale_string (key_file,
                                                 G_KEY_FILE_DESKTOP_GROUP,
                                                 G_KEY_FILE_DESKTOP_KEY_NAME,
                                                 NULL,
                                                 NULL);
      if (priv->translation_domain)
        domain = priv->translation_domain;
      gtk_label_set_text (GTK_LABEL (shortcut->priv->label),
                          dgettext (domain, priv->name));

      icon_name = g_key_file_get_string (key_file,
                                         G_KEY_FILE_DESKTOP_GROUP,
                                         G_KEY_FILE_DESKTOP_KEY_ICON,
                                         NULL);
      if (icon_name)
        {
          if (g_path_is_absolute (icon_name))
            gtk_image_set_from_file (GTK_IMAGE (shortcut->priv->icon),
                                     icon_name);
          else
            gtk_image_set_from_icon_name (GTK_IMAGE (shortcut->priv->icon),
                                          icon_name,
                                          GTK_ICON_SIZE_INVALID);
          g_free (icon_name);
        }

      priv->exec = g_key_file_get_string (key_file,
                                          G_KEY_FILE_DESKTOP_GROUP,
                                          G_KEY_FILE_DESKTOP_KEY_EXEC,
                                          NULL);
      priv->service = g_key_file_get_string (key_file,
                                             G_KEY_FILE_DESKTOP_GROUP,
                                             HD_KEY_FILE_DESKTOP_KEY_SERVICE,
                                             NULL);
      priv->translation_domain = g_key_file_get_string (key_file,
                                                        G_KEY_FILE_DESKTOP_GROUP,
                                                        HD_KEY_FILE_DESKTOP_KEY_TRANSLATION_DOMAIN,
                                                        NULL);
    }
  else
    {
      g_debug ("Could not load shortcut .desktop file '%s'. %s", file_name, error->message);
      g_error_free (error);
      g_key_file_free (key_file);
      return FALSE;
    }

  g_key_file_free (key_file);

  return TRUE;
}

static void
hd_task_shortcut_set_desktop_id (HDTaskShortcut *shortcut,
                                 const gchar    *desktop_id)
{
  HDTaskShortcutPrivate *priv = shortcut->priv;
  const gchar * const * applications_dirs;
  guint i;

  g_free (priv->desktop_id);
  priv->desktop_id = g_strdup (desktop_id);

  applications_dirs = get_applications_dirs ();
  for (i = 0; applications_dirs[i]; i++)
    {
      gchar *file_name = g_build_filename (applications_dirs[i],
                                           desktop_id,
                                           NULL);

      if (hd_task_shortcut_load_from_file (shortcut,
                                           file_name))
        {
          g_free (file_name);
          break;
        }

      g_free (file_name);
    }
}

static void
hd_task_shortcut_finalize (GObject *object)
{
  HDTaskShortcutPrivate *priv = HD_TASK_SHORTCUT (object)->priv;

  g_free (priv->desktop_id);
  priv->desktop_id = NULL;

  G_OBJECT_CLASS (hd_task_shortcut_parent_class)->finalize (object);
}

static void
hd_task_shortcut_get_property (GObject      *object,
                               guint         prop_id,
                               GValue       *value,
                               GParamSpec   *pspec)
{
  HDTaskShortcutPrivate *priv = HD_TASK_SHORTCUT (object)->priv;

  switch (prop_id)
    {
    case PROP_DESKTOP_ID:
      g_value_set_string (value, priv->desktop_id);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
hd_task_shortcut_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_DESKTOP_ID:
      hd_task_shortcut_set_desktop_id (HD_TASK_SHORTCUT (object),
                                       g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

#define SERVICE_NAME_LEN        255
#define PATH_NAME_LEN           255
#define INTERFACE_NAME_LEN      255
#define TMP_NAME_LEN            255

#define OSSO_BUS_ROOT          "com.nokia"
#define OSSO_BUS_ROOT_PATH     "/com/nokia"
#define OSSO_BUS_TOP           "top_application"

static void
hd_task_shortcut_activate_service (const gchar *app)
{
  gchar service[SERVICE_NAME_LEN], path[PATH_NAME_LEN],
        interface[INTERFACE_NAME_LEN], tmp[TMP_NAME_LEN];
  DBusMessage *msg = NULL;
  DBusError error;
  DBusConnection *conn;

  g_debug ("%s: app=%s\n", __FUNCTION__, app);

  /* If we have full service name we will use it */
  if (g_strrstr(app, "."))
  {
    g_snprintf(service, SERVICE_NAME_LEN, "%s", app);
    g_snprintf(interface, INTERFACE_NAME_LEN, "%s", service);
    g_snprintf(tmp, TMP_NAME_LEN, "%s", app);
    g_snprintf(path, PATH_NAME_LEN, "/%s", g_strdelimit(tmp, ".", '/'));
  }
  else /* use com.nokia prefix */
  {
    g_snprintf(service, SERVICE_NAME_LEN, "%s.%s", OSSO_BUS_ROOT, app);
    g_snprintf(path, PATH_NAME_LEN, "%s/%s", OSSO_BUS_ROOT_PATH, app);
    g_snprintf(interface, INTERFACE_NAME_LEN, "%s", service);
  }

  dbus_error_init (&error);
  conn = dbus_bus_get (DBUS_BUS_SESSION, &error);
  if (dbus_error_is_set (&error))
  {
    g_warning ("could not start: %s: %s", service, error.message);
    dbus_error_free (&error);
    return;
  }

  msg = dbus_message_new_method_call (service, path, interface, OSSO_BUS_TOP);
  if (msg == NULL)
  {
    g_warning ("failed to create message");
    return;
  }

  if (!dbus_connection_send (conn, msg, NULL))
    g_warning ("dbus_connection_send failed");

  dbus_message_unref (msg);
}

static gboolean
hd_task_shortcut_activate (HDTaskShortcut  *shortcut,
                           GError         **error)
{
  HDTaskShortcutPrivate *priv = shortcut->priv;
  gboolean res = FALSE;

  if (priv->service)
    {
      /* launch the application, or if it's already running
       * move it to the top
       */
      hd_task_shortcut_activate_service (priv->service);
      return TRUE;
    }

#if 0
  if (hd_wm_is_lowmem_situation ())
    {
      if (!tn_close_application_dialog (CAD_ACTION_OPENING))
        {
          g_set_error (...);
          return FALSE;
        }
    }
#endif

  if (priv->exec)
    {
      gchar *space = strchr (priv->exec, ' ');
      gchar *exec;
      gint argc;
      gchar **argv = NULL;
      GPid child_pid;
      GError *internal_error = NULL;

      g_debug ("Executing %s: `%s'", priv->name, priv->exec);

      if (space)
        {
          gchar *cmd = g_strndup (priv->exec, space - priv->exec);
          gchar *exc = g_find_program_in_path (cmd);

          exec = g_strconcat (exc, space, NULL);

          g_free (cmd);
          g_free (exc);
        }
      else
        exec = g_find_program_in_path (priv->exec);

      if (!g_shell_parse_argv (exec, &argc, &argv, &internal_error))
        {
          g_propagate_error (error, internal_error);

          g_free (exec);
          if (argv)
            g_strfreev (argv);

          return FALSE;
        }

      res = g_spawn_async (NULL,
                           argv, NULL,
                           0,
                           NULL, NULL,
                           &child_pid,
                           &internal_error);
      if (internal_error)
        g_propagate_error (error, internal_error);

      g_free (exec);

      if (argv)
        g_strfreev (argv);

      return res;
    }
  else
    {
#if 0
      g_set_error (...);
#endif
      return FALSE;
    }

  g_assert_not_reached ();

  return FALSE;
}


static void
hd_task_shortcut_class_init (HDTaskShortcutClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = hd_task_shortcut_finalize;
  object_class->get_property = hd_task_shortcut_get_property;
  object_class->set_property = hd_task_shortcut_set_property;

  g_object_class_install_property (object_class,
                                   PROP_DESKTOP_ID,
                                   g_param_spec_string ("desktop-id",
                                                        "Desktop ID",
                                                        "name of the desktop file",
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private (klass, sizeof (HDTaskShortcutPrivate));
}

static gboolean
button_press_event_cb (GtkWidget      *widget,
                       GdkEventButton *event,
                       HDTaskShortcut *shortcut)
{
  HDTaskShortcutPrivate *priv = shortcut->priv;

  if (event->button == 1)
    {
      priv->button_pressed = TRUE;

      return TRUE;
    }

  return FALSE;
}

static gboolean
button_release_event_cb (GtkWidget      *widget,
                         GdkEventButton *event,
                         HDTaskShortcut *shortcut)
{
  HDTaskShortcutPrivate *priv = shortcut->priv;

  if (event->button == 1 &&
      priv->button_pressed)
    {
      priv->button_pressed = FALSE;

      hd_task_shortcut_activate (shortcut,
                                 NULL);
      return TRUE;
    }

  return FALSE;
}

static gboolean
leave_notify_event_cb (GtkWidget        *widget,
                       GdkEventCrossing *event,
                       HDTaskShortcut   *shortcut)
{
  HDTaskShortcutPrivate *priv = shortcut->priv;

  priv->button_pressed = FALSE;

  return FALSE;
}

static void
hd_task_shortcut_init (HDTaskShortcut *applet)
{
  HDTaskShortcutPrivate *priv;
  GtkWidget *ebox, *vbox, *alignment;

  priv = HD_TASK_SHORTCUT_GET_PRIVATE (applet);
  applet->priv = priv;

  ebox = gtk_event_box_new ();
  gtk_widget_show (ebox);
  gtk_widget_add_events (ebox,
                         GDK_BUTTON_PRESS_MASK |
                         GDK_BUTTON_RELEASE_MASK |
                         GDK_LEAVE_NOTIFY_MASK);
  g_signal_connect (ebox, "button-press-event",
                    G_CALLBACK (button_press_event_cb), applet);
  g_signal_connect (ebox, "button-release-event",
                    G_CALLBACK (button_release_event_cb), applet);
  g_signal_connect (ebox, "leave-notify-event",
                    G_CALLBACK (leave_notify_event_cb), applet);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox);

  alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_widget_show (alignment);
  gtk_widget_set_size_request (alignment, 100, 100);

  priv->label = gtk_label_new ("Skype");
  gtk_widget_show (priv->label);
  gtk_widget_set_size_request (priv->label, 140, -1);

  priv->icon = gtk_image_new ();
  gtk_widget_show (priv->icon);
  gtk_image_set_pixel_size (GTK_IMAGE (priv->icon), 64);
  gtk_widget_set_size_request (priv->icon, 64, 64);

  gtk_container_add (GTK_CONTAINER (applet), ebox);
  gtk_container_add (GTK_CONTAINER (ebox), vbox);
  gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 0);
  gtk_container_add (GTK_CONTAINER (alignment), priv->icon);
  gtk_box_pack_start (GTK_BOX (vbox), priv->label, FALSE, FALSE, 0);

/*  gtk_window_set_opacity (GTK_WINDOW (applet), 0); */
}

HDTaskShortcut *
hd_task_shortcut_new (const gchar *desktop_id)
{
  gchar *id;
  HDTaskShortcut *shortcut;

  id = g_strdup_printf (PLUGIN_ID_FORMAT, desktop_id);

  shortcut = g_object_new (HD_TYPE_TASK_SHORTCUT,
                           "plugin-id", id,
                           "desktop-id", desktop_id,
                           NULL);
  g_free (id);

  return shortcut;
}
