/*
 * This file is part of hildon-home
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Based on hildon-desktop-notification-manager.c (hildon-desktop)
 *   Copyright (C) 2007 Nokia Corporation.
 *
 *   Author:  Lucas Rocha <lucas.rocha@nokia.com>
 *            Moises Martinez <moises.martinez@nokia.com>
 *   Contact: Karoliina Salminen <karoliina.t.salminen@nokia.com>
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
#include "hd-marshal.h"

#include <libgnomevfs/gnome-vfs.h>

#include <string.h>
#include <stdio.h>
#include <gtk/gtk.h>
#include <sqlite3.h>

/* To trace _db-related things. */
#if 0
# define DBDBG                          g_warning
#else
# define DBDBG(...)                     /* */
#endif

#if 0
# define ACTION                         g_warning
#else
# define ACTION(...)                    /* */
#endif

/* Macros for hd_notification_manager_bind_params() to make it easier
 * to bind an integer, a string etc. to an SQL placeholder.
 * Always terminate the arguments with %DB_BIND_END. */
#define DB_BIND_INT(val)                G_TYPE_INT,     val
#define DB_BIND_STR(val)                G_TYPE_STRING,  val
#define DB_BIND_FLOAT(val)              G_TYPE_FLOAT,   val
#define DB_BIND_UCHAR(val)              G_TYPE_UCHAR,   val
#define DB_BIND_INT64(val)              G_TYPE_INT64,   val
#define DB_BIND_END                     G_TYPE_INVALID

#define HD_NOTIFICATION_MANAGER_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_NOTIFICATION_MANAGER, HDNotificationManagerPrivate))

G_DEFINE_TYPE (HDNotificationManager, hd_notification_manager, G_TYPE_OBJECT);

enum {
    NOTIFIED,
    N_SIGNALS
};

static guint signals[N_SIGNALS];  

#define HD_NOTIFICATION_MANAGER_DBUS_NAME  "org.freedesktop.Notifications" 
#define HD_NOTIFICATION_MANAGER_DBUS_PATH  "/org/freedesktop/Notifications"

#define HD_NOTIFICATION_MANAGER_ICON_SIZE  48

struct _HDNotificationManagerPrivate
{
  DBusGConnection *connection;
  GMutex          *mutex;
  guint            current_id;
  GHashTable      *notifications;

  /*
   * @prepared_statements is a map between SQL statement strings
   * and SQLite prepared statements.  Can be %NULL.  Destroying
   * the hash table destroys all the prepared statements.
   * @db should be valid as long as the hash table is not empty.
   *
   * Database modifications are done in a common transaction.
   * After a modification is complete a COMMIT is scheduled
   * at @commit_timeout.  If there are more modifications until
   * that time COMMIT is further deferred.  This may lead to
   * starvation.
   *
   * @commit_callback is the #GSource ID of the deferred committing
   * function.  If not 0 a transaction is open.  This case the
   * function must be called when hildon-home quits.
   */
  sqlite3         *db;
  GHashTable      *prepared_statements;
  time_t           commit_timeout;
  gulong           commit_callback;

};

/* IPC structure between _insert_hints() and _insert_hint(). */
typedef struct 
{
  /* @stmt is the prepared statement to insert the hint with. */
  sqlite3_stmt *stmt;
  gint          id;
  gint          result;
} HildonNotificationHintInfo;

/* Notification hint value type codes, as used in the database.
 * For upgrade compatibility with ourselves new values should be
 * added at the end and existing ones should not be changed. */
enum
{
  HD_NM_HINT_TYPE_NONE,
  HD_NM_HINT_TYPE_STRING,
  HD_NM_HINT_TYPE_INT,
  HD_NM_HINT_TYPE_FLOAT,
  HD_NM_HINT_TYPE_UCHAR,
  HD_NM_HINT_TYPE_INT64,
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
  char *sql;
  gint nrow = 0, ncol;
  char **results;
  char *error;

  g_mutex_lock (nm->priv->mutex);

  do
    {
      next_id = ++nm->priv->current_id;

      if (nm->priv->db)
        {
          sql = sqlite3_mprintf ("SELECT id FROM notifications WHERE id=%d",
                                 next_id);

          error = NULL;
          sqlite3_get_table (nm->priv->db, sql,
                             &results, &nrow, &ncol, &error);

          sqlite3_free_table (results);
          sqlite3_free (sql); 
          if (error)
            {
              g_warning ("%s: SQL error: %s", __func__, error);
              sqlite3_free (error);
            }
        }
    }
  while (nrow > 0);

  if (nm->priv->current_id == G_MAXUINT)
    nm->priv->current_id = 0;

  g_mutex_unlock (nm->priv->mutex);

  return next_id;
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
    case HD_NM_HINT_TYPE_INT64:
        {
          gint64 value_i64;

          sscanf (argv[2], "%Ld", &value_i64);
          g_value_init (value, G_TYPE_INT64);
          g_value_set_int64 (value, value_i64);
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
  gchar *error = NULL;
  guint id;
  HDNotification *notification;

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
      sqlite3_free (error);
    }

  sqlite3_free (sql);

  hints = g_hash_table_new_full (g_str_hash, 
                                 g_str_equal,
                                 (GDestroyNotify) g_free,
                                 (GDestroyNotify) hint_value_free);

  hint = g_new0 (GValue, 1);
  hint = g_value_init (hint, G_TYPE_UCHAR);
  g_value_set_uchar (hint, TRUE);

  g_hash_table_insert (hints, g_strdup("persistent"), hint);

  sql = sqlite3_mprintf ("SELECT * FROM hints WHERE nid=%d", id);

  if (sqlite3_exec (nm->priv->db, 
                    sql,
                    hd_notification_manager_load_hint,
                    hints,
                    &error) != SQLITE_OK)
    {
      g_warning ("Unable to load hints: %s", error);
      sqlite3_free (error);
    }

  sqlite3_free (sql);

  notification = hd_notification_new (id,
                                      argv[2],
                                      argv[3],
                                      argv[4],
                                      (gchar **) g_array_free (actions, FALSE),
                                      hints,
                                      (gint) g_ascii_strtod (argv[5], NULL),
                                      argv[6]);

  g_hash_table_insert (nm->priv->notifications,
                       GUINT_TO_POINTER (id),
                       notification);

  g_signal_emit (nm, signals[NOTIFIED], 0, notification, TRUE);

  return 0;
}

void 
hd_notification_manager_db_load (HDNotificationManager *nm)
{
  gchar *error = NULL;

  g_return_if_fail (nm->priv->db != NULL);

  if (sqlite3_exec (nm->priv->db, 
                    "SELECT * FROM notifications",
                    hd_notification_manager_load_row,
                    nm,
                    &error) != SQLITE_OK)
    {
      g_warning ("Unable to load notifications: %s", error);
      sqlite3_free (error);
    }
}

static gint 
hd_notification_manager_db_exec (HDNotificationManager *nm,
                                 const gchar *sql)
{
  gchar *error = NULL;

  g_return_val_if_fail (nm->priv->db != NULL, SQLITE_ERROR);
  g_return_val_if_fail (sql != NULL, SQLITE_ERROR);

  if (sqlite3_exec (nm->priv->db, sql, NULL, 0, &error) != SQLITE_OK)
    {
      g_warning ("%s. Unable to execute the query %s: %s",
                 __FUNCTION__,
                 sql,
                 error);
      sqlite3_free (error);

      return SQLITE_ERROR;
    }

  return SQLITE_OK;
}

/*
 * Prepares and caches an SQL query.  You should not finalize the
 * returned statement.  Returns %NULL on error.  Prepared statements
 * can be executed with hd_notification_manager_db_exec_prepared().
 * For the caching to be effective @sql should be a string literal.
 */
static sqlite3_stmt *
hd_notification_manager_db_prepare (HDNotificationManager *nm,
                                    const gchar           *sql)
{
  gint ret;
  sqlite3_stmt *stmt;

  if (G_UNLIKELY (!nm->priv->prepared_statements))
    /* We can use `direct' operations on the key because we know
     * they will be string literals. */
    nm->priv->prepared_statements = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                                           NULL, (GDestroyNotify) sqlite3_finalize);
  else if ((stmt = g_hash_table_lookup (nm->priv->prepared_statements, sql)))
    return stmt;

  g_return_val_if_fail (nm->priv->db != NULL, NULL);
  if ((ret = sqlite3_prepare_v2 (nm->priv->db, sql, -1,
                                 &stmt, NULL)) != SQLITE_OK)
    g_critical ("sqlite3_prepare_v2(%s): %d", sql, ret);

  g_hash_table_insert (nm->priv->prepared_statements,
                       (gpointer) sql,
                       stmt);

  return stmt;
}

/*
 * Wrapper around sqlite3_bind_*() to bind actual parameters to @stmt.
 * The arguments are %GType--value pairs, terminated by a %G_TYPE_INVALID.
 * Only INT:s, STRING:s, FLOAT:s and UCHAR:s are handled.  Use %DB_BIND_*()
 * to specify the parameter values.  Returns an sqlite status code.
 */
static gint
hd_notification_manager_db_bind_params (sqlite3_stmt *stmt, ...)
{
  guint i;
  gint ret;
  GType type;
  va_list types;
  const gchar *str;

  ret = SQLITE_OK;
  g_return_val_if_fail (stmt != NULL, SQLITE_ERROR);
  va_start(types, stmt);
  for (i = 1; (type = va_arg (types, GType)) != DB_BIND_END && ret == SQLITE_OK;
       i++)
    if      (type == G_TYPE_INT)
      ret = sqlite3_bind_int  (stmt, i, va_arg (types, gint));
    else if (type == G_TYPE_STRING)
      /* @str needs to be saved because the commit is delayed. */
      ret = (str = va_arg (types, const gchar *)) != NULL
        ? sqlite3_bind_text (stmt, i, str, -1, SQLITE_TRANSIENT)
        : sqlite3_bind_null (stmt, i);
    else if (type == G_TYPE_INT64)
      ret = sqlite3_bind_int64 (stmt, i, va_arg (types, gint64));
    else if (type == G_TYPE_FLOAT)
      /* Quoting gcc: 'gfloat' is promoted to 'double' when passed
       * through '...' */
      ret = sqlite3_bind_double (stmt, i, va_arg (types, gdouble));
    else if (type == G_TYPE_UCHAR)
      /* Same for guchar -> int. */
      ret = sqlite3_bind_int (stmt, i, va_arg (types, gint));
    else
      g_assert_not_reached();
  va_end (types);

  return ret;
}

/* Like hd_notification_manager_db_exec() executes a non-SELECT statement
 * and returns %SQLITE_OK/not-OK.  @stmt is reset in any case. */
static gint
hd_notification_manager_db_exec_prepared (sqlite3_stmt *stmt)
{
  gint ret;

  g_return_val_if_fail (stmt != NULL, SQLITE_ERROR);

  /* @stmt is expected to be reset.  SELECT, INSERT, UPDATE return
   * DONE on success, COMMIT returns OK. */
  if ((ret = sqlite3_step (stmt)) != SQLITE_DONE && ret != SQLITE_OK)
    g_warning ("Unable to execute query: %d", ret);
  else /* Be sqlite3_exec() like. */
    ret = SQLITE_OK;
  sqlite3_reset(stmt);

  return ret;
}

/* Prepare, cache and execute @sql. */
static gint
hd_notification_manager_db_prepare_and_exec (HDNotificationManager *nm,
                                             const gchar           *sql)
{
  return hd_notification_manager_db_exec_prepared (
                          hd_notification_manager_db_prepare (nm, sql));
}

/* #GSourceFunc to COMMIT an active transaction. */
static gboolean
hd_notification_manager_db_commit (HDNotificationManager *nm)
{
  DBDBG(__FUNCTION__);

  if (nm->priv->commit_timeout > time(NULL))
    /* Not yet. */
    return TRUE;

  if (hd_notification_manager_db_prepare_and_exec (nm, "COMMIT")
      != SQLITE_OK)
    /* We can lose more than one notification here but if COMMIT
     * fails something is very wrong anyway. */
    hd_notification_manager_db_prepare_and_exec (nm, "ROLLBACK");

  nm->priv->commit_callback = 0;
  return FALSE;
}

/* Like a plain BEGIN but allows you to batch multiple atomic units of work
 * in one transaction.  This is faster because writing back a transaction
 * is slow. */
static int
hd_notification_manager_db_begin (HDNotificationManager *nm)
{ DBDBG(__FUNCTION__);

  /* Open a transaction if it hasn't been. */
  if (!nm->priv->commit_callback)
    {
      if (hd_notification_manager_db_prepare_and_exec (nm, "BEGIN")
          != SQLITE_OK)
        return SQLITE_ERROR;
      nm->priv->commit_callback = g_timeout_add_seconds (10,
                      (GSourceFunc)hd_notification_manager_db_commit, nm);
    }

  /* Create the savepoint we can revert to on error. */
  if (hd_notification_manager_db_prepare_and_exec (nm, "SAVEPOINT willie")
      != SQLITE_OK)
    /* It's okay to leave the transaction open, it's only that the caller
     * needs to know it shouldn't continue.  But other callers may. */
    return SQLITE_ERROR;

  return SQLITE_OK;
}

/* Record the last unit of work in the transaction as done,
 * but don't commit yet.  On error you must _revert(). */
static int
hd_notification_manager_db_finish (HDNotificationManager *nm)
{ DBDBG(__FUNCTION__);
  g_assert (nm->priv->commit_callback != 0);

  if (hd_notification_manager_db_prepare_and_exec (nm, "RELEASE willie")
      != SQLITE_OK)
    /* Caller will revert. */
    return SQLITE_ERROR;

  /* Commit in 8 seconds or so. */
  nm->priv->commit_timeout = time(NULL) + 8;
  return SQLITE_OK;
}

/* Reverts the last unit of work.  Earlier work is unaffected
 * (unless something reall bad is in the air). */
static void
hd_notification_manager_db_revert (HDNotificationManager *nm)
{ DBDBG(__FUNCTION__);
  g_assert (nm->priv->commit_callback != 0);
  if (hd_notification_manager_db_prepare_and_exec (nm, "ROLLBACK TO willie")
      != SQLITE_OK)
    { /* It is very nasty if ROLLBACK fails but what can we do? */
      hd_notification_manager_db_prepare_and_exec (nm, "ROLLBACK");
      g_source_remove (nm->priv->commit_callback);
      nm->priv->commit_callback = 0;
    }
}

static gint
hd_notification_manager_db_create (HDNotificationManager *nm)
{
  gchar **results;
  gint nrow, ncol;
  gchar *error = NULL;
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
  if (error)
    {
      g_warning ("%s: SQL error: %s", __func__, error);
      sqlite3_free (error);
    }

  return result; 
}

static int
hd_notification_manager_db_insert_actions (HDNotificationManager  *nm,
                                           guint                  id,
                                           gchar                 **actions)
{
  guint i;
  sqlite3_stmt *insert;

  /* Insert the actions. */
  insert = hd_notification_manager_db_prepare (nm,
             "INSERT INTO actions (id, label, nid) VALUES (?, ?, ?)");
  for (i = 0; actions && actions[i] != NULL; i += 2)
    {
      if (hd_notification_manager_db_bind_params (insert,
                 DB_BIND_STR(actions[i]), DB_BIND_STR(actions[i+1]),
                 DB_BIND_INT(id), DB_BIND_END) != SQLITE_OK)
        return SQLITE_ERROR;
      if (hd_notification_manager_db_exec_prepared (insert) != SQLITE_OK)
        return SQLITE_ERROR;
    }

  return SQLITE_OK;
}

static void 
hd_notification_manager_db_insert_hint (gpointer key, gpointer value,
                                        gpointer data)
{
  HildonNotificationHintInfo *hinfo = (HildonNotificationHintInfo *) data;
  GValue *hvalue = (GValue *) value;
  gchar *hkey = (gchar *) key;

  /* Don't bother if we have an error already. */
  if (hinfo->result != SQLITE_OK)
    return;

  /* Compile the statement. */
  switch (G_VALUE_TYPE (hvalue))
    {
    case G_TYPE_STRING:
      hinfo->result = hd_notification_manager_db_bind_params (hinfo->stmt,
             DB_BIND_STR (hkey), DB_BIND_INT (HD_NM_HINT_TYPE_STRING),
             DB_BIND_STR (g_value_get_string (hvalue)),
             DB_BIND_INT (hinfo->id), DB_BIND_END);
      break;
    case G_TYPE_INT:
      hinfo->result = hd_notification_manager_db_bind_params (hinfo->stmt,
             DB_BIND_STR (hkey), DB_BIND_INT (HD_NM_HINT_TYPE_INT),
             DB_BIND_INT (g_value_get_int (hvalue)),
             DB_BIND_INT (hinfo->id), DB_BIND_END);
      break;
    case G_TYPE_INT64:
      hinfo->result = hd_notification_manager_db_bind_params (hinfo->stmt,
             DB_BIND_STR (hkey), DB_BIND_INT (HD_NM_HINT_TYPE_INT64),
             DB_BIND_INT64 (g_value_get_int64 (hvalue)),
             DB_BIND_INT (hinfo->id), DB_BIND_END);
      break;
    case G_TYPE_FLOAT:
      hinfo->result = hd_notification_manager_db_bind_params (hinfo->stmt,
             DB_BIND_STR (hkey), DB_BIND_INT (HD_NM_HINT_TYPE_FLOAT),
             DB_BIND_FLOAT (g_value_get_float (hvalue)),
             DB_BIND_INT (hinfo->id), DB_BIND_END);
      break;
    case G_TYPE_UCHAR:
      hinfo->result = hd_notification_manager_db_bind_params (hinfo->stmt,
             DB_BIND_STR (hkey), DB_BIND_INT (HD_NM_HINT_TYPE_UCHAR),
             DB_BIND_UCHAR (g_value_get_uchar (hvalue)),
             DB_BIND_INT (hinfo->id), DB_BIND_END);
      break;
    default:
      g_warning ("Hint `%s' of notification %d has invalid value type %u",
                 hkey, hinfo->id, G_VALUE_TYPE (hvalue));
      hinfo->result = SQLITE_ERROR;
      return;
    }

  if (hinfo->result == SQLITE_OK)
    hinfo->result = hd_notification_manager_db_exec_prepared (hinfo->stmt);
}

static int
hd_notification_manager_db_insert_hints (HDNotificationManager *nm,
                                         guint                  id,
                                         GHashTable            *hints)
{
  HildonNotificationHintInfo hinfo;

  /* Insert the notification hints. */
  hinfo.id = id;
  hinfo.result = SQLITE_OK; 
  hinfo.stmt = hd_notification_manager_db_prepare (nm,
             "INSERT INTO hints (id, type, value, nid) "
             "VALUES (?, ?, ?, ?)");
  g_hash_table_foreach (hints, hd_notification_manager_db_insert_hint, &hinfo);
  return hinfo.result;
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
  sqlite3_stmt *insert;

  /* Prepare and begin.  We needn't begin before prepare. */
  insert = hd_notification_manager_db_prepare (nm,
             "INSERT INTO notifications "
             "(id, app_name, icon_name, summary, body, timeout, dest) " 
             "VALUES (?, ?, ?, ?, ?, ?, ?)");
  if (hd_notification_manager_db_bind_params (insert,
             DB_BIND_INT(id), DB_BIND_STR(app_name), DB_BIND_STR(icon),
             DB_BIND_STR(summary), DB_BIND_STR(body), DB_BIND_INT(timeout),
             DB_BIND_STR(dest), DB_BIND_END) != SQLITE_OK)
    return SQLITE_ERROR;

  if (hd_notification_manager_db_begin (nm) != SQLITE_OK)
    return SQLITE_ERROR;

  /* Insert the notification, its actions and hints. */
  if (hd_notification_manager_db_exec_prepared (insert) != SQLITE_OK)
    goto rollback;
  if (hd_notification_manager_db_insert_actions (nm, id, actions) != SQLITE_OK)
    goto rollback;
  if (hd_notification_manager_db_insert_hints (nm, id, hints) != SQLITE_OK)
    goto rollback;

  /* Finish. */
  if (hd_notification_manager_db_finish(nm) == SQLITE_OK)
    return SQLITE_OK;

rollback:
  hd_notification_manager_db_revert (nm);
  return SQLITE_ERROR;
}

static gint
hd_notification_manager_db_delete_actions_and_hints (
                                    HDNotificationManager *nm,
                                    guint                  id)
{
  sqlite3_stmt *delete;

  /* Delete actions. */
  delete = hd_notification_manager_db_prepare (nm,
             "DELETE FROM actions WHERE nid = ?");
  if (hd_notification_manager_db_bind_params (delete,
             DB_BIND_INT (id), DB_BIND_END) != SQLITE_OK)
    return SQLITE_ERROR;
  if (hd_notification_manager_db_exec_prepared (delete) != SQLITE_OK)
    return SQLITE_ERROR;

  /* Delete hints. */
  delete = hd_notification_manager_db_prepare (nm,
             "DELETE FROM hints WHERE nid = ?");
  if (hd_notification_manager_db_bind_params (delete,
             DB_BIND_INT (id), DB_BIND_END) != SQLITE_OK)
    return SQLITE_ERROR;
  if (hd_notification_manager_db_exec_prepared (delete) != SQLITE_OK)
    return SQLITE_ERROR;

  return SQLITE_OK;
}

static gint
hd_notification_manager_db_delete (HDNotificationManager *nm,
                                   guint                  id)
{
  sqlite3_stmt *delete;

  /* Prepare and begin. */
  delete = hd_notification_manager_db_prepare (nm,
             "DELETE FROM notifications WHERE id = ?");
  if (hd_notification_manager_db_bind_params (delete,
             DB_BIND_INT (id), DB_BIND_END) != SQLITE_OK)
    return SQLITE_ERROR;

  if (hd_notification_manager_db_begin (nm) != SQLITE_OK)
    return SQLITE_ERROR;

  /* Delete. */
  if (hd_notification_manager_db_delete_actions_and_hints (nm, id)
      != SQLITE_OK)
    goto rollback;
  if (hd_notification_manager_db_exec_prepared (delete)
      != SQLITE_OK)
    goto rollback;

  /* Finish. */
  if (hd_notification_manager_db_finish (nm) == SQLITE_OK)
    return SQLITE_OK;

rollback:
  hd_notification_manager_db_revert (nm);
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
  sqlite3_stmt *update;

  /* Prepare and begin. */
  update = hd_notification_manager_db_prepare (nm,
             "UPDATE notifications SET "
             "  app_name = ?, icon_name = ?, "
             "  summary = ?, body = ?, timeout = ? " 
             "WHERE id = ?");
  if (hd_notification_manager_db_bind_params (update,
             DB_BIND_STR(app_name), DB_BIND_STR(icon), DB_BIND_STR(summary),
             DB_BIND_STR(body), DB_BIND_INT(timeout), DB_BIND_INT(id),
             DB_BIND_END) != SQLITE_OK)
    return SQLITE_ERROR;

  if (hd_notification_manager_db_begin (nm) != SQLITE_OK)
    return SQLITE_ERROR;

  /* Update the notification, then wipe out and re-add its actions
   * and hints. */
  if (hd_notification_manager_db_exec_prepared (update) != SQLITE_OK)
    goto rollback;
  if (hd_notification_manager_db_delete_actions_and_hints (nm, id) != SQLITE_OK)
    goto rollback;
  if (hd_notification_manager_db_insert_actions (nm, id, actions) != SQLITE_OK)
    goto rollback;
  if (hd_notification_manager_db_insert_hints (nm, id, hints) != SQLITE_OK)
    goto rollback;

  /* Finish. */
  if (hd_notification_manager_db_finish (nm) == SQLITE_OK)
    return SQLITE_OK;

rollback:
  hd_notification_manager_db_revert (nm);
  return SQLITE_ERROR;
}

static void
hd_notification_manager_init (HDNotificationManager *nm)
{
  DBusGProxy *bus_proxy;
  GError *error = NULL;
  gchar *config_dir;
  guint result;

  nm->priv = HD_NOTIFICATION_MANAGER_GET_PRIVATE (nm);

  nm->priv->mutex = g_mutex_new ();

  nm->priv->current_id = 0;

  nm->priv->notifications = g_hash_table_new_full (g_direct_hash,
                                                   g_direct_equal,
                                                   NULL,
                                                   (GDestroyNotify) g_object_unref);

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
                                          DBUS_NAME_FLAG_ALLOW_REPLACEMENT |
                                          DBUS_NAME_FLAG_REPLACE_EXISTING |
                                          DBUS_NAME_FLAG_DO_NOT_QUEUE,
                                          &result, 
                                          &error))
    {
      g_warning ("Could not register name: %s", error->message);

      g_error_free (error);

      return;
    }

  g_object_unref (bus_proxy);

  if (result == DBUS_REQUEST_NAME_REPLY_EXISTS)
    {
      g_warning ("%s. %s already exists",
                 __FUNCTION__,
                 HD_NOTIFICATION_MANAGER_DBUS_NAME);

      return;
    }

  if (result == DBUS_REQUEST_NAME_REPLY_IN_QUEUE)
    {
      g_warning ("%s. %s already exists",
                 __FUNCTION__,
                 HD_NOTIFICATION_MANAGER_DBUS_NAME);

      return;
    }

  dbus_g_object_type_install_info (HD_TYPE_NOTIFICATION_MANAGER,
                                   &dbus_glib_hd_notification_manager_object_info);

  dbus_g_connection_register_g_object (nm->priv->connection,
                                       HD_NOTIFICATION_MANAGER_DBUS_PATH,
                                       G_OBJECT (nm));

  g_debug ("%s registered to session bus at %s", HD_NOTIFICATION_MANAGER_DBUS_NAME, HD_NOTIFICATION_MANAGER_DBUS_PATH);

  nm->priv->db = NULL;

  config_dir = g_build_filename (g_get_home_dir (),
                                 ".config",
                                 "hildon-desktop",
                                 NULL);
  if (!g_mkdir_with_parents (config_dir,
                             S_IRWXU |
                             S_IRGRP | S_IXGRP |
                             S_IROTH | S_IXOTH))
    {
      gchar *notifications_db = NULL;

      notifications_db = g_build_filename (g_get_home_dir (), 
                                           ".config",
                                           "hildon-desktop",
                                           "notifications.db",
                                           NULL); 

      result = sqlite3_open (notifications_db, &nm->priv->db);

      g_free (notifications_db);

      if (result != SQLITE_OK)
        {
          g_warning ("Can't open database: %s", sqlite3_errmsg (nm->priv->db));
          sqlite3_close (nm->priv->db);
          nm->priv->db = NULL;
        } else {
            result = hd_notification_manager_db_create (nm);

            if (result != SQLITE_OK)
              {
                g_warning ("Can't create database: %s", sqlite3_errmsg (nm->priv->db));
              }
        }
    }
  else
    {
      /* User config dir could not be created */
      result = gnome_vfs_result_from_errno ();
      g_warning ("Could not mkdir '%s', %s",
                 config_dir,
                 gnome_vfs_result_to_string (result));
    }

  g_free (config_dir);
}

static void 
hd_notification_manager_dispose (GObject *object)
{
  HDNotificationManagerPrivate *priv = HD_NOTIFICATION_MANAGER (object)->priv;

  if (priv->connection)
    priv->connection = (dbus_g_connection_unref (priv->connection), NULL);

  G_OBJECT_CLASS (hd_notification_manager_parent_class)->dispose (object);
}

static void 
hd_notification_manager_finalize (GObject *object)
{
  HDNotificationManagerPrivate *priv = HD_NOTIFICATION_MANAGER (object)->priv;

  if (priv->mutex)
    priv->mutex = (g_mutex_free (priv->mutex), NULL);

  if (priv->db)
    {
      /* Save uncommitted work. */
      if (priv->commit_callback)
        { /* Remove the source first beca use _commit() clears it. */
          priv->commit_timeout = 0;
          g_source_remove (priv->commit_callback);
          hd_notification_manager_db_commit (HD_NOTIFICATION_MANAGER (object));
        }

      /* Release the prepared statements we know about. */
      if (priv->prepared_statements)
        {
          g_hash_table_destroy (priv->prepared_statements);
          priv->prepared_statements = NULL;
        }

      /* Now we can close the shop. */
      priv->db = (sqlite3_close (priv->db), NULL);
    }

  if (priv->notifications)
    priv->notifications = (g_hash_table_destroy (priv->notifications), NULL);

  G_OBJECT_CLASS (hd_notification_manager_parent_class)->finalize (object);
}

static void
hd_notification_manager_class_init (HDNotificationManagerClass *class)
{
  GObjectClass *g_object_class = (GObjectClass *) class;

  g_object_class->dispose = hd_notification_manager_dispose;
  g_object_class->finalize = hd_notification_manager_finalize;

  signals[NOTIFIED] =
    g_signal_new ("notified",
                  G_OBJECT_CLASS_TYPE (g_object_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (HDNotificationManagerClass, notified),
                  NULL, NULL,
                  hd_cclosure_marshal_VOID__OBJECT_BOOLEAN,
                  G_TYPE_NONE, 2,
                  HD_TYPE_NOTIFICATION, G_TYPE_BOOLEAN);

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
                                             HDNotification        *notification)
{
  DBusMessage *message;

  message = hd_notification_manager_create_signal (nm,
                                                   hd_notification_get_id (notification),
                                                   hd_notification_get_sender (notification),
                                                   "NotificationClosed");

  if (message == NULL) return;

  dbus_connection_send (dbus_g_connection_get_connection (nm->priv->connection), 
                        message, 
                        NULL);

  dbus_message_unref (message);

  if (hd_notification_get_persistent (notification))
    hd_notification_manager_db_delete (nm, hd_notification_get_id (notification));
}

static gboolean 
hd_notification_manager_timeout (guint id)
{
  HDNotificationManager *nm = hd_notification_manager_get ();
  HDNotification *notification;

  notification = g_hash_table_lookup (nm->priv->notifications,
                                      GUINT_TO_POINTER (id));

  if (!notification)
    return FALSE;

  /* Notify the client */
  hd_notification_manager_notification_closed (HD_NOTIFICATION_MANAGER (nm), 
                                               notification);
  hd_notification_closed (notification);

  g_hash_table_remove (nm->priv->notifications,
                       GUINT_TO_POINTER (id));

  return FALSE;
}

/**
 * hd_notification_manager_get:
 *
 * Returns %NULL if the singleton instance is finalized.
 *
 * Returns: the singleton HDNotificationManager instance or %NULL. Should not be refed or unrefed.
 */
HDNotificationManager *
hd_notification_manager_get (void)
{
  static gsize initialized = 0;
  static gpointer nm = NULL;

  if (g_once_init_enter (&initialized))
    {
      nm = g_object_new (HD_TYPE_NOTIFICATION_MANAGER, NULL);
      g_object_add_weak_pointer (G_OBJECT (nm), &nm);

      g_once_init_leave (&initialized, 1);
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

static gboolean
idle_emit (gpointer data)
{
  HDNotificationManager *nm = hd_notification_manager_get ();

  if (nm)
    g_signal_emit (nm, signals[NOTIFIED], 0, data, FALSE);

  g_object_unref (data);

  return FALSE;
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
  GHashTable *hints_copy;
  GValue *hint;
  gchar **actions_copy;
  gboolean valid_actions = TRUE;
  gboolean persistent = FALSE;
  gint i;
  HDNotification *notification;
  gboolean replace = FALSE;
  const gchar *category;

/*  g_return_val_if_fail (summary != '\0', FALSE);
  g_return_val_if_fail (body != '\0', FALSE);*/

  /* Get "persisitent" hint */
  hint = g_hash_table_lookup (hints, "persistent");
  persistent = (G_VALUE_HOLDS_BOOLEAN (hint) && g_value_get_boolean (hint)) ||
               (G_VALUE_HOLDS_UCHAR (hint) && g_value_get_uchar (hint));

  /* Do not be persistent when "no-notification-window" is used */
  hint = g_hash_table_lookup (hints, "no-notification-window");
  if ((G_VALUE_HOLDS_BOOLEAN (hint) && g_value_get_boolean (hint)) ||
      (G_VALUE_HOLDS_UCHAR (hint) && g_value_get_uchar (hint)))
    persistent = FALSE;

  /* Get "category" hint */
  hint = g_hash_table_lookup (hints, "category");
  category = G_VALUE_HOLDS_STRING (hint) ? g_value_get_string (hint) : NULL;

  /* Try to find an existing notification */
  if (id)
    {
      notification = g_hash_table_lookup (nm->priv->notifications, GUINT_TO_POINTER (id));
      replace = notification != NULL;
      if (!replace)
        g_warning ("Cannot replace notification: notification with id %u not found", id);
    }

  if (!replace)
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

      /* If there is no time hint use the current time */
      if (!g_hash_table_lookup (hints_copy, "time"))
        {
          GValue *value = g_new0 (GValue, 1);
          time_t t;

          time (&t);

          g_value_init (value, G_TYPE_INT64);
          g_value_set_int64 (value, (gint64) t);
          g_hash_table_insert (hints_copy, g_strdup ("time"), value);
        }

      sender = dbus_g_method_get_sender (context);

      id = hd_notification_manager_next_id (nm);

      notification = hd_notification_new (id,
                                          icon,
                                          summary,
                                          body,
                                          actions_copy,
                                          hints_copy,
                                          timeout,
                                          sender);

      g_object_ref (notification);

      g_hash_table_insert (nm->priv->notifications,
                           GUINT_TO_POINTER (id),
                           notification);

      gdk_threads_add_idle (idle_emit, g_object_ref (notification));

      if (persistent && nm->priv->db)
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

      g_strfreev (actions_copy);
      g_object_unref (notification);
      g_free (sender);
    }
  else 
    {
      /* Update new data */
      g_object_set (notification,
                    "icon", icon,
                    "summary", summary,
                    "body", body,
                    NULL);

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
  hint = g_value_init (hint, G_TYPE_UINT);
  g_value_set_uint (hint, type);

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
  HDNotification *notification;

  notification = g_hash_table_lookup (nm->priv->notifications,
                                      GUINT_TO_POINTER (id));

  if (notification)
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
      /* Notify the client */
      hd_notification_manager_notification_closed (nm,
                                                   notification);
      hd_notification_closed (notification);

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
          g_warning ("Invalid list of parameters for the notification"
                     " D-Bus callback.");
          g_scanner_destroy (scanner);
          return NULL;
        }

      g_scanner_destroy (scanner);
    }

  g_strfreev (message_elements);

  return message;
}

void
hd_notification_manager_call_action (HDNotificationManager *nm,
                                     HDNotification        *notification,
                                     const gchar           *action_id)
{ ACTION(__FUNCTION__);
  DBusMessage *message = NULL;
  const gchar *dbus_cb;

  g_return_if_fail (nm != NULL);
  g_return_if_fail (HD_IS_NOTIFICATION_MANAGER (nm));

  dbus_cb = hd_notification_get_dbus_cb (notification, action_id);

  if (dbus_cb != NULL)
    {
      message = hd_notification_manager_message_from_desc (nm, 
                                                           dbus_cb);
    }

  if (message != NULL)
    {
      dbus_connection_send (dbus_g_connection_get_connection (nm->priv->connection), 
                            message, 
                            NULL);
      dbus_message_unref (message);
    }

  if (hd_notification_get_sender (notification) != NULL)
    {
      message = hd_notification_manager_create_signal (nm, 
                                                       hd_notification_get_id (notification),
                                                       hd_notification_get_sender (notification),
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
{ ACTION(__FUNCTION__);
  GHashTableIter iter;
  gpointer key, value;

  g_hash_table_iter_init (&iter, nm->priv->notifications);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      HDNotification *notification = value;

      hd_notification_manager_notification_closed (nm, 
                                                   notification);
      hd_notification_closed (notification);

      g_hash_table_iter_remove (&iter);
    }	  
}

void
hd_notification_manager_call_dbus_callback (HDNotificationManager *nm,
                                            const gchar *dbus_call)
{ ACTION(__FUNCTION__); 
  DBusMessage *message;

  g_return_if_fail (HD_IS_NOTIFICATION_MANAGER (nm));
  g_return_if_fail (dbus_call != NULL);

  message = hd_notification_manager_message_from_desc (nm, dbus_call); 

  if (message != NULL)
    {
      dbus_connection_send (dbus_g_connection_get_connection (nm->priv->connection), 
                            message, 
                            NULL);
      dbus_message_unref (message);
    }
}

void
hd_notification_manager_call_dbus_callback_with_arg (HDNotificationManager *nm,
                                                     const gchar           *dbus_call,
                                                     const gchar           *arg)
{ ACTION(__FUNCTION__); 
  DBusMessage *message;

  g_return_if_fail (HD_IS_NOTIFICATION_MANAGER (nm));
  g_return_if_fail (dbus_call != NULL);

  message = hd_notification_manager_message_from_desc (nm, dbus_call); 

  if (message != NULL)
    {
      dbus_message_append_args (message,
                                DBUS_TYPE_STRING, &arg,
                                DBUS_TYPE_INVALID);
      dbus_connection_send (dbus_g_connection_get_connection (nm->priv->connection), 
                            message, 
                            NULL);
      dbus_message_unref (message);
    }
}

void
hd_notification_manager_call_message (HDNotificationManager *nm,
                                      DBusMessage           *message)
{ ACTION(__FUNCTION__);
  g_return_if_fail (HD_IS_NOTIFICATION_MANAGER (nm));

  if (message != NULL)
    dbus_connection_send (dbus_g_connection_get_connection (nm->priv->connection), 
                          message, 
                          NULL);
} 
