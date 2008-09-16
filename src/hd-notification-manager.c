/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2007, 2008 Nokia Corporation.
 *
 * Author:  Lucas Rocha <lucas.rocha@nokia.com>
 * 	    Moises Martinez <moises.martinez@nokia.com>
 * Contact: Karoliina Salminen <karoliina.t.salminen@nokia.com>
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

#include "hd-notification-manager.h"
#include "hd-notification-manager-glue.h"

#include <string.h>
#include <stdio.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <sqlite3.h>

#define HD_NOTIFICATION_MANAGER_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_NOTIFICATION_MANAGER, HDNotificationManagerPrivate))

G_DEFINE_TYPE (HDNotificationManager, hd_notification_manager, G_TYPE_OBJECT);

enum {
    SIGNAL_NOTIFICATION_CLOSED,
    N_SIGNALS
};

static gint signals[N_SIGNALS];  

#define HD_NOTIFICATION_MANAGER_DBUS_NAME  "org.freedesktop.Notifications" 
#define HD_NOTIFICATION_MANAGER_DBUS_PATH  "/org/freedesktop/Notifications"

#define HD_NOTIFICATION_MANAGER_ICON_SIZE  48

struct _HDNotificationManagerPrivate
{
  DBusGConnection *connection;
  GMutex          *mutex;
  guint            current_id;
  sqlite3         *db;
  GHashTable      *notifications;
  HDPluginManager *pm;
  GList           *plugins;
};

typedef struct 
{
  HDNotificationManager  *nm;
  gint                    id;
  gint                    result;
} HildonNotificationHintInfo;

typedef struct
{
  gchar       *app_name;
  gchar       *icon_name;
  GdkPixbuf   *icon;
  gchar       *summary;
  gchar       *body;
  gchar      **actions;
  GHashTable  *hints;
  gint         timeout;
  gboolean     removable;
  gboolean     ack;
  gchar       *sender;
} NotificationData;

enum
{
  HD_NM_HINT_TYPE_NONE,
  HD_NM_HINT_TYPE_STRING,
  HD_NM_HINT_TYPE_INT,
  HD_NM_HINT_TYPE_FLOAT,
  HD_NM_HINT_TYPE_UCHAR
};

static void                            
hint_value_free (GValue *value)
{
  g_value_unset (value);
  g_free (value);
}

static guint
hd_notification_manager_next_id (HDNotificationManager *nm)
{
  guint next_id;
  gchar *sql;
  gint nrow, ncol;
  gchar **results;
  gchar *error;

  g_mutex_lock (nm->priv->mutex);

  do
    {
      next_id = ++nm->priv->current_id;

      sql = sqlite3_mprintf ("SELECT id FROM notifications WHERE id=%d", next_id);

      sqlite3_get_table (nm->priv->db, sql,
                         &results, &nrow, &ncol, &error);

      sqlite3_free_table (results);
      sqlite3_free (sql); 
      g_free (error);
    }
  while (nrow > 0);

  if (nm->priv->current_id == G_MAXUINT)
    nm->priv->current_id = 0;

  g_mutex_unlock (nm->priv->mutex);

  return next_id;
}

static GdkPixbuf *
hd_notification_manager_get_icon (const gchar *icon_name)
{
  GdkPixbuf *pixbuf = NULL;
  GError *error = NULL;	
  GtkIconTheme *icon_theme;

  if (!g_str_equal (icon_name, ""))
    {
      if (g_file_test (icon_name, G_FILE_TEST_EXISTS))
        {
          pixbuf = gdk_pixbuf_new_from_file (icon_name, &error);

          if (error)
            {
              pixbuf = NULL; /* It'd be already NULL */
              g_warning ("Notification icon not loaded: %s", error->message);
              g_error_free (error);
            }
        }
      else
        {	    
          icon_theme = gtk_icon_theme_get_default ();

          pixbuf = gtk_icon_theme_load_icon (icon_theme,
                                             icon_name,
                                             40,
                                             GTK_ICON_LOOKUP_NO_SVG,
                                             &error);

          if (error)
            {
              pixbuf = NULL; /* It'd be already NULL */
              g_warning ("Notification Manager %s:",error->message);
              g_error_free (error);
            }
        }
    }

  return pixbuf;
}

static int 
hd_notification_manager_load_hint (void *data, 
                                   gint argc, 
                                   gchar **argv, 
                                   gchar **col_name)
{
  GHashTable *hints = (GHashTable *) data;
  GValue *value;
  gint type;

  type = (guint) g_ascii_strtod (argv[1], NULL);

  value = g_new0 (GValue, 1);

  switch (type)
    {
    case HD_NM_HINT_TYPE_STRING:
      g_value_init (value, G_TYPE_STRING);
      g_value_set_string (value, argv[2]);
      break;

    case HD_NM_HINT_TYPE_INT:
        {
          gint value_i;

          sscanf (argv[2], "%d", &value_i);
          g_value_init (value, G_TYPE_INT);
          g_value_set_int (value, value_i);
          break;
        }
    case HD_NM_HINT_TYPE_FLOAT:
        {
          gfloat value_f;

          sscanf (argv[2], "%f", &value_f);
          g_value_init (value, G_TYPE_FLOAT);
          g_value_set_float (value, value_f);
          break;
        }
    case HD_NM_HINT_TYPE_UCHAR:
        {
          guchar value_c;

          sscanf (argv[2], "%c", &value_c);
          g_value_init (value, G_TYPE_UCHAR);
          g_value_set_uchar (value, value_c);
          break;
        }
    }

  g_hash_table_insert (hints, g_strdup (argv[0]), value);

  return 0;
}

static int 
hd_notification_manager_load_action (void *data, 
                                     gint argc, 
                                     gchar **argv, 
                                     gchar **col_name)
{
  GArray *actions = (GArray *) data;

  g_array_append_val (actions, argv[0]);
  g_array_append_val (actions, argv[1]);

  return 0;
}

static int 
hd_notification_manager_load_row (void *data, 
                                  gint argc, 
                                  gchar **argv, 
                                  gchar **col_name)
{
  HDNotificationManager *nm;
  GHashTable *hints;
  GValue *hint;
  GArray *actions;
  gchar *sql;
  gchar *error;
  guint id;
  NotificationData *nd;

  nm = HD_NOTIFICATION_MANAGER (data);

  id = (guint) g_ascii_strtod (argv[0], NULL);

  actions = g_array_new (TRUE, FALSE, sizeof (gchar *)); 

  sql = sqlite3_mprintf ("SELECT * FROM actions WHERE nid=%d", id);

  if (sqlite3_exec (nm->priv->db, 
                    sql,
                    hd_notification_manager_load_action,
                    actions,
                    &error) != SQLITE_OK)
    {
      g_warning ("Unable to load actions: %s", error);
      g_free (error);
    }

  sqlite3_free (sql);

  hints = g_hash_table_new_full (g_str_hash, 
                                 g_str_equal,
                                 (GDestroyNotify) g_free,
                                 (GDestroyNotify) hint_value_free);

  hint = g_new0 (GValue, 1);
  hint = g_value_init (hint, G_TYPE_UCHAR);
  g_value_set_uchar (hint, TRUE);

  g_hash_table_insert (hints, "persistent", hint);

  sql = sqlite3_mprintf ("SELECT * FROM hints WHERE nid=%d", id);

  if (sqlite3_exec (nm->priv->db, 
                    sql,
                    hd_notification_manager_load_hint,
                    hints,
                    &error) != SQLITE_OK)
    {
      g_warning ("Unable to load hints: %s", error);
      g_free (error);
    }

  sqlite3_free (sql);

  nd = g_slice_new0 (NotificationData);
  nd->app_name = g_strdup (argv[1]);
  nd->icon_name = g_strdup (argv[2]);
  nd->icon = hd_notification_manager_get_icon (argv[2]);
  nd->summary = g_strdup (argv[3]);
  nd->body = g_strdup (argv[4]);
  nd->actions = (gchar **) g_array_free (actions, FALSE);
  nd->hints = hints;
  nd->timeout = (gint) g_ascii_strtod (argv[5], NULL);
  nd->removable = TRUE;
  nd->ack = FALSE;
  nd->sender = g_strdup (argv[6]);

  g_hash_table_insert (nm->priv->notifications,
                       GUINT_TO_POINTER (id),
                       nd);

  return 0;
}

static void 
hd_notification_manager_db_load (HDNotificationManager *nm)
{
  gchar *error;

  g_return_if_fail (nm->priv->db != NULL);

  if (sqlite3_exec (nm->priv->db, 
                    "SELECT * FROM notifications",
                    hd_notification_manager_load_row,
                    nm,
                    &error) != SQLITE_OK)
    {
      g_warning ("Unable to load notifications: %s", error);
      g_free (error);
    }
}

static gint 
hd_notification_manager_db_exec (HDNotificationManager *nm,
                                 const gchar *sql)
{
  gchar *error;

  g_return_val_if_fail (nm->priv->db != NULL, SQLITE_ERROR);
  g_return_val_if_fail (sql != NULL, SQLITE_ERROR);

  if (sqlite3_exec (nm->priv->db, sql, NULL, 0, &error) != SQLITE_OK)
    {
      g_warning ("Unable to execute the query: %s", error);
      g_free (error);

      return SQLITE_ERROR;
    }

  return SQLITE_OK;
}

static gint
hd_notification_manager_db_create (HDNotificationManager *nm)
{
  gchar **results;
  gint nrow, ncol;
  gchar *error;
  gint result = SQLITE_OK;

  sqlite3_get_table (nm->priv->db, 
                     "SELECT tbl_name FROM sqlite_master WHERE type='table' ORDER BY tbl_name",
                     &results, &nrow, &ncol, &error);

  if (nrow == 0)
    {
      result = hd_notification_manager_db_exec (nm,
                                                "CREATE TABLE notifications (\n"
                                                "    id        INTEGER PRIMARY KEY,\n"
                                                "    app_name  VARCHAR(30)  NOT NULL,\n"
                                                "    icon_name VARCHAR(50)  NOT NULL,\n"
                                                "    summary   VARCHAR(100) NOT NULL,\n"
                                                "    body      VARCHAR(100) NOT NULL,\n"
                                                "    timeout   INTEGER DEFAULT 0,\n"
                                                "    dest      VARCHAR(100) NOT NULL\n"
                                                ")");

      result = hd_notification_manager_db_exec (nm,
                                                "CREATE TABLE hints (\n"
                                                "    id        VARCHAR(50),\n"
                                                "    type      INTEGER,\n"
                                                "    value     VARCHAR(200) NOT NULL,\n"
                                                "    nid       INTEGER,\n"
                                                "    PRIMARY KEY (id, nid)\n"
                                                ")");

      result = hd_notification_manager_db_exec (nm,
                                                "CREATE TABLE actions (\n"
                                                "    id        VARCHAR(50),\n"
                                                "    label     VARCHAR(100) NOT NULL,\n"
                                                "    nid       INTEGER,\n"
                                                "    PRIMARY KEY (id, nid)\n"
                                                ")");
    }

  sqlite3_free_table (results);
  g_free (error);

  return result; 
}

static void 
hd_notification_manager_db_insert_hint (gpointer key, gpointer value, gpointer data)
{
  HildonNotificationHintInfo *hinfo = (HildonNotificationHintInfo *) data;
  GValue *hvalue = (GValue *) value;
  gchar *hkey = (gchar *) key;
  gchar *sql;
  gchar *sql_value = NULL;
  gint type = HD_NM_HINT_TYPE_NONE;

  switch (G_VALUE_TYPE (hvalue))
    {
    case G_TYPE_STRING:
      sql_value = g_strdup (g_value_get_string (hvalue));
      type = HD_NM_HINT_TYPE_STRING;
      break;

    case G_TYPE_INT:
      sql_value = g_strdup_printf ("%d", g_value_get_int (hvalue));;
      type = HD_NM_HINT_TYPE_INT;
      break;

    case G_TYPE_FLOAT:
      sql_value = g_strdup_printf ("%f", g_value_get_float (hvalue));;
      type = HD_NM_HINT_TYPE_FLOAT;
      type = 3;
      break;

    case G_TYPE_UCHAR:
      sql_value = g_strdup_printf ("%d", g_value_get_uchar (hvalue));;
      type = HD_NM_HINT_TYPE_UCHAR;
      break;
    }

  if (sql_value == NULL || type == HD_NM_HINT_TYPE_NONE)
    {
      hinfo->result = SQLITE_ERROR;
      return;
    }

  sql = sqlite3_mprintf ("INSERT INTO hints \n"
                         "(id, type, value, nid)\n" 
                         "VALUES \n"
                         "('%q', %d, '%q', %d)",
                         hkey, type, sql_value, hinfo->id);

  hinfo->result = hd_notification_manager_db_exec (hinfo->nm, sql);

  sqlite3_free (sql);

}

static gint 
hd_notification_manager_db_insert (HDNotificationManager *nm,
                                   const gchar           *app_name,
                                   guint                  id,
                                   const gchar           *icon,
                                   const gchar           *summary,
                                   const gchar           *body,
                                   gchar                **actions,
                                   GHashTable            *hints,
                                   gint                   timeout,
                                   const gchar           *dest)
{
  HildonNotificationHintInfo *hinfo;
  gchar *sql;
  gint result, i;

  result = hd_notification_manager_db_exec (nm, "BEGIN TRANSACTION");

  if (result != SQLITE_OK) goto rollback;

  sql = sqlite3_mprintf ("INSERT INTO notifications \n"
                         "(id, app_name, icon_name, summary, body, timeout, dest)\n" 
                         "VALUES \n"
                         "(%d, '%q', '%q', '%q', '%q', %d, '%q')",
                         id, app_name, icon, summary, body, timeout, dest);

  result = hd_notification_manager_db_exec (nm, sql);

  sqlite3_free (sql);

  if (result != SQLITE_OK) goto rollback;

  for (i = 0; actions && actions[i] != NULL; i += 2)
    {
      gchar *label = actions[i + 1];

      sql = sqlite3_mprintf ("INSERT INTO actions \n"
                             "(id, label, nid) \n" 
                             "VALUES \n"
                             "('%q', '%q', %d)",
                             actions[i], label, id);

      result = hd_notification_manager_db_exec (nm, sql);

      sqlite3_free (sql);

      if (result != SQLITE_OK) goto rollback;
    }

  hinfo = g_new0 (HildonNotificationHintInfo, 1);

  hinfo->id = id;
  hinfo->nm = nm;
  hinfo->result = SQLITE_OK; 

  g_hash_table_foreach (hints, hd_notification_manager_db_insert_hint, hinfo);

  result = hinfo->result;

  g_free (hinfo);

  if (result != SQLITE_OK) goto rollback;

  result = hd_notification_manager_db_exec (nm, "COMMIT TRANSACTION");

  if (result != SQLITE_OK) goto rollback;

  return SQLITE_OK;

rollback:
  hd_notification_manager_db_exec (nm, "ROLLBACK TRANSACTION");

  return SQLITE_ERROR;
}

static gint 
hd_notification_manager_db_update (HDNotificationManager *nm,
                                   const gchar           *app_name,
                                   guint                  id,
                                   const gchar           *icon,
                                   const gchar           *summary,
                                   const gchar           *body,
                                   gchar                **actions,
                                   GHashTable            *hints,
                                   gint                   timeout)
{
  HildonNotificationHintInfo *hinfo;
  gchar *sql;
  gint result, i;

  result = hd_notification_manager_db_exec (nm, "BEGIN TRANSACTION");

  if (result != SQLITE_OK) goto rollback;

  sql = sqlite3_mprintf ("UPDATE notifications SET\n"
                         "app_name='%q', icon_name='%q', \n"
                         "summary='%q', body='%q', timeout=%d\n" 
                         "WHERE id=%d",
                         app_name, icon, summary, body, timeout, id);

  result = hd_notification_manager_db_exec (nm, sql);

  sqlite3_free (sql);

  if (result != SQLITE_OK) goto rollback;

  sql = sqlite3_mprintf ("DELETE FROM actions WHERE nid=%d", id);

  result = hd_notification_manager_db_exec (nm, sql);

  sqlite3_free (sql);

  if (result != SQLITE_OK) goto rollback;

  for (i = 0; actions && actions[i] != NULL; i += 2)
    {
      gchar *label = actions[i + 1];

      sql = sqlite3_mprintf ("INSERT INTO actions \n"
                             "(id, label, nid) \n" 
                             "VALUES \n"
                             "('%q', '%q', %d)",
                             actions[i], label, id);

      result = hd_notification_manager_db_exec (nm, sql);

      sqlite3_free (sql);

      if (result != SQLITE_OK) goto rollback;
    }

  sql = sqlite3_mprintf ("DELETE FROM hints WHERE nid=%d", id);

  result = hd_notification_manager_db_exec (nm, sql);

  sqlite3_free (sql);

  if (result != SQLITE_OK) goto rollback;

  hinfo = g_new0 (HildonNotificationHintInfo, 1);

  hinfo->id = id;
  hinfo->nm = nm;
  hinfo->result = SQLITE_OK; 

  g_hash_table_foreach (hints, hd_notification_manager_db_insert_hint, hinfo);

  result = hinfo->result;

  g_free (hinfo);

  if (result != SQLITE_OK) goto rollback;

  result = hd_notification_manager_db_exec (nm, "COMMIT TRANSACTION");

  if (result != SQLITE_OK) goto rollback;

  return SQLITE_OK;

rollback:
  hd_notification_manager_db_exec (nm, "ROLLBACK TRANSACTION");

  return SQLITE_ERROR;
}

static gint
hd_notification_manager_db_delete (HDNotificationManager *nm,
                                   guint id)
{
  gchar *sql;
  gint result;

  result = hd_notification_manager_db_exec (nm, "BEGIN TRANSACTION");

  if (result != SQLITE_OK) goto rollback;

  sql = sqlite3_mprintf ("DELETE FROM actions WHERE nid=%d", id);

  result = hd_notification_manager_db_exec (nm, sql);

  sqlite3_free (sql);

  sql = sqlite3_mprintf ("DELETE FROM hints WHERE nid=%d", id);

  result = hd_notification_manager_db_exec (nm, sql);

  sqlite3_free (sql);

  sql = sqlite3_mprintf ("DELETE FROM notifications WHERE id=%d", id);

  result = hd_notification_manager_db_exec (nm, sql);

  sqlite3_free (sql);

  if (result != SQLITE_OK) goto rollback;

  result = hd_notification_manager_db_exec (nm, "COMMIT TRANSACTION");

  if (result != SQLITE_OK) goto rollback;

  return SQLITE_OK;

rollback:
  hd_notification_manager_db_exec (nm, "ROLLBACK TRANSACTION");

  return SQLITE_ERROR;
}

static void
hd_notification_manager_init (HDNotificationManager *nm)
{
  DBusGProxy *bus_proxy;
  GError *error = NULL;
  gchar *notifications_db;
  guint result;

  nm->priv = HD_NOTIFICATION_MANAGER_GET_PRIVATE (nm);

  nm->priv->mutex = g_mutex_new ();

  nm->priv->current_id = 0;

  nm->priv->notifications = g_hash_table_new (g_direct_hash,
                                              g_direct_equal);

  nm->priv->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

  if (error != NULL)
    {
      g_warning ("Failed to open connection to bus: %s\n",
                 error->message);

      g_error_free (error);

      return;
    }

  bus_proxy = dbus_g_proxy_new_for_name (nm->priv->connection,
                                         DBUS_SERVICE_DBUS,
                                         DBUS_PATH_DBUS,
                                         DBUS_INTERFACE_DBUS);

  if (!org_freedesktop_DBus_request_name (bus_proxy,
                                          HD_NOTIFICATION_MANAGER_DBUS_NAME,
                                          DBUS_NAME_FLAG_DO_NOT_QUEUE,
                                          &result, 
                                          &error))
    {
      g_warning ("Could not register name: %s", error->message);

      g_error_free (error);

      return;
    }

  g_object_unref (bus_proxy);

  if (result == DBUS_REQUEST_NAME_REPLY_EXISTS) return;

  dbus_g_object_type_install_info (HD_TYPE_NOTIFICATION_MANAGER,
                                   &dbus_glib_hd_notification_manager_object_info);

  dbus_g_connection_register_g_object (nm->priv->connection,
                                       HD_NOTIFICATION_MANAGER_DBUS_PATH,
                                       G_OBJECT (nm));

  nm->priv->db = NULL;

  notifications_db = g_build_filename (g_get_home_dir (), 
                                       ".osso/hildon-desktop",
                                       "notifications.db",
                                       NULL); 

  result = sqlite3_open (notifications_db, &nm->priv->db);

  g_free (notifications_db);

  if (result != SQLITE_OK)
    {
      g_warning ("Can't open database: %s", sqlite3_errmsg (nm->priv->db));
      sqlite3_close (nm->priv->db);
    } else {
        result = hd_notification_manager_db_create (nm);

        if (result != SQLITE_OK)
          {
            g_warning ("Can't create database: %s", sqlite3_errmsg (nm->priv->db));
          }
        else
          {
            hd_notification_manager_db_load (nm);
          }
    }
}

static void 
hd_notification_manager_finalize (GObject *object)
{
  HDNotificationManager *nm = HD_NOTIFICATION_MANAGER (object);

  if (nm->priv->mutex)
    {
      g_mutex_free (nm->priv->mutex);
      nm->priv->mutex = NULL;
    }

  if (nm->priv->db)
    {
      sqlite3_close (nm->priv->db);
      nm->priv->db = NULL;
    }
}

static void
hd_notification_manager_class_init (HDNotificationManagerClass *class)
{
  GObjectClass *g_object_class = (GObjectClass *) class;

  g_object_class->finalize = hd_notification_manager_finalize;

  signals[SIGNAL_NOTIFICATION_CLOSED] =
    g_signal_new ("notification-closed",
                  G_OBJECT_CLASS_TYPE (g_object_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (HDNotificationManagerClass, notification_closed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__INT,
                  G_TYPE_NONE, 1, G_TYPE_INT);

  g_type_class_add_private (class, sizeof (HDNotificationManagerPrivate));
}

static DBusMessage *
hd_notification_manager_create_signal (HDNotificationManager *nm, 
                                       guint id,
                                       const gchar *dest, 
                                       const gchar *signal_name)
{
  DBusMessage *message;

  g_assert(dest != NULL);

  message = dbus_message_new_signal ("/org/freedesktop/Notifications",
                                     "org.freedesktop.Notifications",
                                     signal_name);

  dbus_message_set_destination (message, dest);

  dbus_message_append_args (message,
                            DBUS_TYPE_UINT32, &id,
                            DBUS_TYPE_INVALID);

  return message;
}

static void
hd_notification_manager_notification_closed (HDNotificationManager *nm,
                                             guint id,
                                             const gchar *dest,
                                             gboolean persistent)
{
  DBusMessage *message;

  message = hd_notification_manager_create_signal (nm, 
                                                   id, 
                                                   dest, 
                                                   "NotificationClosed");

  if (message == NULL) return;

  dbus_connection_send (dbus_g_connection_get_connection (nm->priv->connection), 
                        message, 
                        NULL);

  dbus_message_unref (message);

  if (persistent)
    {
      hd_notification_manager_db_delete (nm, id);
    }

  g_signal_emit (nm, signals[SIGNAL_NOTIFICATION_CLOSED], 0, id);
}

static gboolean 
hd_notification_manager_timeout (guint id)
{
  HDNotificationManager *nm = hd_notification_manager_new (NULL);
  NotificationData *nd;
  GValue *hint;
  gboolean persistent = FALSE;

  nd = g_hash_table_lookup (nm->priv->notifications,
                            GUINT_TO_POINTER (id));

  if (!nd)
    return FALSE;

  hint = g_hash_table_lookup (nd->hints, "persistent");

  persistent = hint != NULL && g_value_get_uchar (hint);

  /* Notify the client */
  hd_notification_manager_notification_closed (HD_NOTIFICATION_MANAGER (nm), 
                                               id,
                                               nd->sender,
                                               persistent);

  /* FIXME free nd*/

  g_hash_table_remove (nm->priv->notifications,
                       GUINT_TO_POINTER (id));

  return FALSE;
}

static void
notification_plugin_added (HDPluginManager       *pm,
                           GObject               *plugin,
                           HDNotificationManager *nm)
{
  nm->priv->plugins = g_list_append (nm->priv->plugins, plugin);
}

static void
notification_plugin_removed (HDPluginManager       *pm,
                             GObject               *plugin,
                             HDNotificationManager *nm)
{
  nm->priv->plugins = g_list_remove (nm->priv->plugins, plugin);
}

HDNotificationManager *
hd_notification_manager_new (HDPluginManager *pm)
{
  static HDNotificationManager *nm = NULL;

  if (nm == NULL)
    {
      nm = g_object_new (HD_TYPE_NOTIFICATION_MANAGER, NULL);
      nm->priv->pm = g_object_ref (pm);
      g_signal_connect (pm, "plugin-added",
                        G_CALLBACK (notification_plugin_added), nm);
      g_signal_connect (pm, "plugin-removed",
                        G_CALLBACK (notification_plugin_removed), nm);
    }

  return nm;
}

static void 
copy_hash_table_item (gchar *key, GValue *value, GHashTable *new_hash_table)
{
  GValue *value_copy = g_new0 (GValue, 1);

  value_copy = g_value_init (value_copy, G_VALUE_TYPE (value));

  g_value_copy (value, value_copy);

  g_hash_table_insert (new_hash_table, g_strdup (key), value_copy);
}

gboolean
hd_notification_manager_notify (HDNotificationManager *nm,
                                const gchar           *app_name,
                                guint                  id,
                                const gchar           *icon,
                                const gchar           *summary,
                                const gchar           *body,
                                gchar                **actions,
                                GHashTable            *hints,
                                gint                   timeout, 
                                DBusGMethodInvocation *context)
{
  GdkPixbuf *pixbuf = NULL;
  GHashTable *hints_copy;
  GValue *hint;
  gchar **actions_copy;
  gboolean valid_actions = TRUE;
  gboolean persistent = FALSE;
  gint i;
  NotificationData *nd;
  GList *p;

  g_return_val_if_fail (*icon != '\0', FALSE);
  g_return_val_if_fail (summary != '\0', FALSE);
  g_return_val_if_fail (body != '\0', FALSE);

  pixbuf = hd_notification_manager_get_icon (icon);

  if (!pixbuf)
    {
      g_warning ("Notification icon not found. Ignoring this notification.");
      return FALSE;
    }

  hint = g_hash_table_lookup (hints, "persistent");

  persistent = hint != NULL && g_value_get_uchar (hint);

  nd = g_hash_table_lookup (nm->priv->notifications, GUINT_TO_POINTER (id));

  if (!nd)
    {
      gchar *sender;

      /* Test if we have a valid list of actions */
      for (i = 0; actions && actions[i] != NULL; i += 2)
        {
          gchar *label = actions[i + 1];

          if (label == NULL)
            {
              g_warning ("Invalid action list: no label provided for action %s", actions[i]);

              valid_actions = FALSE;

              break;
            }
        }

      if (valid_actions)
        {
          actions_copy = g_strdupv (actions);
        }
      else
        {
          actions_copy = NULL;
        }

      hints_copy = g_hash_table_new_full (g_str_hash, 
                                          g_str_equal,
                                          (GDestroyNotify) g_free,
                                          (GDestroyNotify) hint_value_free);

      g_hash_table_foreach (hints, (GHFunc) copy_hash_table_item, hints_copy);

      sender = dbus_g_method_get_sender (context);

      nd = g_slice_new0 (NotificationData);
      nd->app_name = g_strdup (app_name);
      nd->icon_name = g_strdup (icon);
      nd->icon = pixbuf;
      nd->summary = g_strdup (summary);
      nd->body = g_strdup (body);
      nd->actions = actions_copy;
      nd->hints = hints_copy;
      nd->timeout = timeout;
      nd->removable = TRUE;
      nd->ack = FALSE;
      nd->sender = sender;

      id = hd_notification_manager_next_id (nm);

      g_hash_table_insert (nm->priv->notifications,
                           GUINT_TO_POINTER (id),
                           nd);

      if (persistent)
        {
          hd_notification_manager_db_insert (nm, 
                                             app_name,
                                             id, 
                                             icon,
                                             summary,
                                             body,
                                             actions_copy,
                                             hints_copy,
                                             timeout,
                                             sender);
        }
    }
  else 
    {
      /* Free old data */
      g_free (nd->icon_name);
      if (nd->icon)
        g_object_unref (nd->icon);
      g_free (nd->summary);
      g_free (nd->body);

      /* Update new data */
      nd->icon_name = g_strdup (icon);
      nd->icon = pixbuf;
      nd->summary = g_strdup (summary);
      nd->body = g_strdup (body);
      nd->removable = FALSE;
      nd->ack = FALSE;

      if (persistent)
        {
          hd_notification_manager_db_update (nm, 
                                             app_name,
                                             id, 
                                             icon,
                                             summary,
                                             body,
                                             actions,
                                             hints,
                                             timeout);
        }
    }

  for (p = nm->priv->plugins; p; p = p->next)
    {
      HDNotificationPlugin *plugin = p->data;

      hd_notification_plugin_notify (plugin,
                                     app_name,
                                     id,
                                     icon,
                                     summary,
                                     body,
                                     actions,
                                     hints,
                                     timeout);
    }

  if (!persistent && timeout > 0)
    {
      g_timeout_add (timeout,
                     (GSourceFunc) hd_notification_manager_timeout,
                     GUINT_TO_POINTER (id));
    }

  dbus_g_method_return (context, id);

  return TRUE;
}

gboolean
hd_notification_manager_system_note_infoprint (HDNotificationManager *nm,
                                               const gchar *message,
                                               DBusGMethodInvocation *context)
{
  GHashTable *hints;
  GValue *hint;

  hints = g_hash_table_new_full (g_str_hash, 
                                 g_str_equal,
                                 NULL,
                                 (GDestroyNotify) hint_value_free);

  hint = g_new0 (GValue, 1);
  hint = g_value_init (hint, G_TYPE_STRING);
  g_value_set_string (hint, "system.note.infoprint");

  g_hash_table_insert (hints, "category", hint);

  hd_notification_manager_notify (nm,
                                  "hildon-desktop",
                                  0,
                                  "qgn_note_infoprint",
                                  "System Note Infoprint",
                                  message,
                                  NULL,
                                  hints,
                                  3000,
                                  context);

  g_hash_table_destroy (hints);

  return TRUE;
}

gboolean
hd_notification_manager_system_note_dialog (HDNotificationManager *nm,
                                            const gchar *message,
                                            guint type,
                                            const gchar *label,
                                            DBusGMethodInvocation *context)
{
  GHashTable *hints;
  GValue *hint;
  gchar **actions;

  g_return_val_if_fail (type >= 0 && type < 5, FALSE);

  static const gchar *icon[5] = {
      "qgn_note_gene_syswarning", /* OSSO_GN_WARNING */
      "qgn_note_gene_syserror",   /* OSSO_GN_ERROR */
      "qgn_note_info",            /* OSSO_GN_NOTICE */
      "qgn_note_gene_wait",       /* OSSO_GN_WAIT */
      "qgn_note_gene_wait"        /* OSSO_GN_PROGRESS */
  };

  hints = g_hash_table_new_full (g_str_hash, 
                                 g_str_equal,
                                 NULL,
                                 (GDestroyNotify) hint_value_free);

  hint = g_new0 (GValue, 1);
  hint = g_value_init (hint, G_TYPE_STRING);
  g_value_set_string (hint, "system.note.dialog");

  g_hash_table_insert (hints, "category", hint);

  hint = g_new0 (GValue, 1);
  hint = g_value_init (hint, G_TYPE_INT);
  g_value_set_int (hint, type);

  g_hash_table_insert (hints, "dialog-type", hint);

  if (!g_str_equal (label, ""))
    {
      GArray *actions_arr;
      gchar *action_id, *action_label;

      actions_arr = g_array_sized_new (TRUE, FALSE, sizeof (gchar *), 2);

      action_id = g_strdup ("default");
      action_label = g_strdup (label);

      g_array_append_val (actions_arr, action_id);
      g_array_append_val (actions_arr, action_label);

      actions = (gchar **) g_array_free (actions_arr, FALSE);
    }
  else
    {
      actions = NULL;
    }

  hd_notification_manager_notify (nm,
                                  "hildon-desktop",
                                  0,
                                  icon[type],
                                  "System Note Dialog",
                                  message,
                                  actions,
                                  hints,
                                  0,
                                  context);

  g_hash_table_destroy (hints);
  g_strfreev (actions);

  return TRUE;
}

gboolean
hd_notification_manager_get_capabilities  (HDNotificationManager *nm, gchar ***caps)
{
  *caps = g_new0 (gchar *, 4);

  (*caps)[0] = g_strdup ("body");
  (*caps)[1] = g_strdup ("body-markup");
  (*caps)[2] = g_strdup ("icon-static");
  (*caps)[3] = NULL;

  return TRUE;
}

gboolean
hd_notification_manager_get_server_info (HDNotificationManager *nm,
                                         gchar                **out_name,
                                         gchar                **out_vendor,
                                         gchar                **out_version/*,
                                                                             gchar                **out_spec_ver*/)
{
  *out_name     = g_strdup ("Hildon Desktop Notification Manager");
  *out_vendor   = g_strdup ("Nokia");
  *out_version  = g_strdup (VERSION);
  /**out_spec_ver = g_strdup ("0.9");*/

  return TRUE;
}

gboolean
hd_notification_manager_close_notification (HDNotificationManager *nm,
                                            guint                  id, 
                                            GError               **error)
{
  NotificationData *nd;
  GValue *hint;
  gboolean persistent = FALSE;

  nd = g_hash_table_lookup (nm->priv->notifications,
                            GUINT_TO_POINTER (id));

  if (nd)
    {
      /* libnotify call close_notification_handler when updating a row 
         that we happend to not want removed */
      /*
         if (!removable)
         {
         gtk_list_store_set (GTK_LIST_STORE (nm),
         &iter,
         HD_NM_COL_REMOVABLE, TRUE,
         -1);
         }
         else
         {*/
      hint = g_hash_table_lookup (nd->hints, "persistent");

      persistent = hint != NULL && g_value_get_uchar (hint);

      /* Notify the client */
      hd_notification_manager_notification_closed (nm, id, nd->sender, persistent);

      /* FIXME free nd */

      g_hash_table_remove (nm->priv->notifications,
                           GUINT_TO_POINTER (id));
      /*}*/

      return TRUE;    
    }
  else
    return FALSE;
}

static guint
parse_parameter (GScanner *scanner, DBusMessage *message)
{
  GTokenType value_token;

  g_scanner_get_next_token (scanner);

  if (scanner->token != G_TOKEN_IDENTIFIER)
    {
      return G_TOKEN_IDENTIFIER;
    }

  if (g_str_equal (scanner->value.v_identifier, "string"))
    {
      value_token = G_TOKEN_STRING;
    }
  else if (g_str_equal (scanner->value.v_identifier, "int"))
    {
      value_token = G_TOKEN_INT;
    }
  else if (g_str_equal (scanner->value.v_identifier, "double"))
    {
      value_token = G_TOKEN_FLOAT;
    } 
  else
    {
      return G_TOKEN_IDENTIFIER;
    }

  g_scanner_get_next_token (scanner);

  if (scanner->token != ':')
    {
      return ':';
    }

  g_scanner_get_next_token (scanner);

  if (scanner->token != value_token)
    {
      return value_token;
    }
  else 
    {
      switch (value_token)
        {
        case G_TOKEN_STRING:
          dbus_message_append_args (message,
                                    DBUS_TYPE_STRING, &scanner->value.v_string,
                                    DBUS_TYPE_INVALID);
          break;
        case G_TOKEN_INT:
          dbus_message_append_args (message,
                                    DBUS_TYPE_INT32, &scanner->value.v_int,
                                    DBUS_TYPE_INVALID);
          break;
        case G_TOKEN_FLOAT:
          dbus_message_append_args (message,
                                    DBUS_TYPE_DOUBLE, &scanner->value.v_float,
                                    DBUS_TYPE_INVALID);
          break;
        default:
          return value_token;
        }
    }

  return G_TOKEN_NONE;
}

static DBusMessage *
hd_notification_manager_message_from_desc (HDNotificationManager *nm,
                                           const gchar *desc)
{
  DBusMessage *message;
  gchar **message_elements;
  gint n_elements;

  message_elements = g_strsplit (desc, " ", 5);

  n_elements = g_strv_length (message_elements);

  if (n_elements < 4)
    {
      g_warning ("Invalid notification D-Bus callback description.");

      return NULL;
    } 

  message = dbus_message_new_method_call (message_elements[0], 
                                          message_elements[1], 
                                          message_elements[2], 
                                          message_elements[3]);

  if (n_elements > 4)
    {
      GScanner *scanner;
      guint expected_token;

      scanner = g_scanner_new (NULL);

      g_scanner_input_text (scanner, 
                            message_elements[4], 
                            strlen (message_elements[4]));

      do
        {
          expected_token = parse_parameter (scanner, message);

          g_scanner_peek_next_token (scanner);
        }
      while (expected_token == G_TOKEN_NONE &&
             scanner->next_token != G_TOKEN_EOF &&
             scanner->next_token != G_TOKEN_ERROR);

      if (expected_token != G_TOKEN_NONE)
        {
          g_warning ("Invalid list of parameters for the notification D-Bus callback.");

          return NULL;
        }

      g_scanner_destroy (scanner);
    }

  return message;
}

void
hd_notification_manager_call_action (HDNotificationManager *nm,
                                     guint                  id,
                                     const gchar           *action_id)
{
  NotificationData *nd;
  DBusMessage *message = NULL;
  GValue *dbus_cb;
  gchar *hint;

  g_return_if_fail (nm != NULL);
  g_return_if_fail (HD_IS_NOTIFICATION_MANAGER (nm));

  nd = g_hash_table_lookup (nm->priv->notifications,
                            GUINT_TO_POINTER (id));
  if (!nd)
    return;

  if (nd->hints != NULL)
    {  
      hint = g_strconcat ("dbus-callback-", action_id, NULL);

      dbus_cb = (GValue *) g_hash_table_lookup (nd->hints, hint);

      if (dbus_cb != NULL)
        {
          message = hd_notification_manager_message_from_desc (nm, 
                                                               (const gchar *) g_value_get_string (dbus_cb));
        }

      if (message != NULL)
        {
          dbus_connection_send (dbus_g_connection_get_connection (nm->priv->connection), 
                                message, 
                                NULL);
        }

      g_free (hint);
    }

  if (nd->sender != NULL)
    {
      message = hd_notification_manager_create_signal (nm, 
                                                       id, 
                                                       nd->sender, 
                                                       "ActionInvoked");

      g_assert (message != NULL);

      dbus_message_append_args (message,
                                DBUS_TYPE_STRING, &action_id,
                                DBUS_TYPE_INVALID);

      dbus_connection_send (dbus_g_connection_get_connection (nm->priv->connection), 
                            message, 
                            NULL);

      dbus_message_unref (message);
    }
}

void
hd_notification_manager_close_all (HDNotificationManager *nm)
{
  GHashTableIter iter;
  gpointer key, value;

  g_hash_table_iter_init (&iter, nm->priv->notifications);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      guint id = GPOINTER_TO_UINT (key);
      NotificationData *nd = value;
      GValue *hint;
      gboolean persistent;

      hint = g_hash_table_lookup (nd->hints, "persistent");

      persistent = hint != NULL && g_value_get_uchar (hint);

      hd_notification_manager_notification_closed (nm, 
                                                   id,
                                                   nd->sender,
                                                   persistent);

      /* FIXME free nd */

      g_hash_table_iter_remove (&iter);
    }	  
}

void
hd_notification_manager_call_dbus_callback (HDNotificationManager *nm,
                                            const gchar *dbus_call)
{ 
  DBusMessage *message;

  g_return_if_fail (HD_IS_NOTIFICATION_MANAGER (nm));
  g_return_if_fail (dbus_call != NULL);

  message = hd_notification_manager_message_from_desc (nm, dbus_call); 

  if (message != NULL)
    {
      dbus_connection_send (dbus_g_connection_get_connection (nm->priv->connection), 
                            message, 
                            NULL);
    }
}
