
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib-bindings.h>

#include "hd-sv-notification-daemon.h"
#include "hd-sv-notification-daemon-glue.h"

#define HD_SV_NOTIFICATION_DAEMON_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_SV_NOTIFICATION_DAEMON, HDSVNotificationDaemonPrivate))

typedef void (* NSVPluginLoad)      (void);
typedef void (* NSVPluginUnload)    (void);
typedef gint (* NSVPluginPlayEvent) (GHashTable *hints,
                                     const char *sender);
typedef void (* NSVPluginStopEvent) (gint        id);

struct _HDSVNotificationDaemonPrivate
{
  GModule            *nsv_module;

  NSVPluginLoad       nsv_plugin_load;
  NSVPluginUnload     nsv_plugin_unload;
  NSVPluginPlayEvent  nsv_plugin_play_event;
  NSVPluginStopEvent  nsv_plugin_stop_event;
};

#define HD_SV_NOTIFICATION_DAEMON_DBUS_NAME  "com.nokia.HildonSVNotificationDaemon" 
#define HD_SV_NOTIFICATION_DAEMON_DBUS_PATH  "/com/nokia/HildonSVNotificationDaemon"

#define HILDON_NOTIFY_SV_PLUGIN_PATH "/usr/lib/hildon-desktop/libhildon-plugins-notify-sv.so"

#define MEMLOCK_LIMIT (1024 * 1024 * 64) /* 64 megabytes */

G_DEFINE_TYPE (HDSVNotificationDaemon, hd_sv_notification_daemon, G_TYPE_OBJECT);

static void
load_sv_plugin (HDSVNotificationDaemon *sv_nd)
{
  HDSVNotificationDaemonPrivate *priv = sv_nd->priv;
  gpointer symbol;

  priv->nsv_module = g_module_open (HILDON_NOTIFY_SV_PLUGIN_PATH,
                                    G_MODULE_BIND_LAZY);
  if (!priv->nsv_module)
    {
      g_warning ("%s. Could not load sound/vibra notification module %s. %s",
                 __FUNCTION__,
                 HILDON_NOTIFY_SV_PLUGIN_PATH,
                 g_module_error ());

      return;
    }

  if (g_module_symbol (priv->nsv_module, "nsv_plugin_load", &symbol))
    {
      priv->nsv_plugin_load = symbol;
    }
  else
    {
      g_warning ("%s. Could not get symbol nsv_plugin_load. %s",
                 __FUNCTION__,
                 g_module_error ());
      goto close_module;
    }

  if (g_module_symbol (priv->nsv_module, "nsv_plugin_unload", &symbol))
    {
      priv->nsv_plugin_unload = symbol;
    }
  else
    {
      g_warning ("%s. Could not get symbol nsv_plugin_unload. %s",
                 __FUNCTION__,
                 g_module_error ());
      goto close_module;
    }

  if (g_module_symbol (priv->nsv_module, "nsv_plugin_play_event", &symbol))
    {
      priv->nsv_plugin_play_event = symbol;
    }
  else
    {
      g_warning ("%s. Could not get symbol nsv_plugin_play_event. %s",
                 __FUNCTION__,
                 g_module_error ());
      goto close_module;
    }


  if (g_module_symbol (priv->nsv_module, "nsv_plugin_stop_event", &symbol))
    {
      priv->nsv_plugin_stop_event = symbol;
    }
  else
    {
      g_warning ("%s. Could not get symbol nsv_plugin_stop_event. %s",
                 __FUNCTION__,
                 g_module_error ());
      goto close_module;
    }

  return;

close_module:
  g_module_close (priv->nsv_module);
  priv->nsv_module = NULL;
}

static void
hd_sv_notification_daemon_init (HDSVNotificationDaemon *sv_nd)
{
  DBusGConnection *connection;
  DBusGProxy *bus_proxy = NULL;
  guint result;
  GError *error = NULL;
  HDSVNotificationDaemonPrivate *priv;

  sv_nd->priv = HD_SV_NOTIFICATION_DAEMON_GET_PRIVATE (sv_nd);
  priv = sv_nd->priv;

  connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
  if (error != NULL)
    {
      g_warning ("Failed to open connection to session bus: %s\n",
                 error->message);
      g_error_free (error);
      goto cleanup;
    }

  dbus_g_object_type_install_info (HD_TYPE_SV_NOTIFICATION_DAEMON,
                                   &dbus_glib_hd_sv_notification_daemon_object_info);

  bus_proxy = dbus_g_proxy_new_for_name (connection,
                                         DBUS_SERVICE_DBUS,
                                         DBUS_PATH_DBUS,
                                         DBUS_INTERFACE_DBUS);

  if (!org_freedesktop_DBus_request_name (bus_proxy,
                                          HD_SV_NOTIFICATION_DAEMON_DBUS_NAME,
                                          DBUS_NAME_FLAG_ALLOW_REPLACEMENT |
                                          DBUS_NAME_FLAG_REPLACE_EXISTING |
                                          DBUS_NAME_FLAG_DO_NOT_QUEUE,
                                          &result, 
                                          &error))
    {
      g_warning ("Could not register name: %s", error->message);
      g_error_free (error);
      goto cleanup;
    }

  if (result == DBUS_REQUEST_NAME_REPLY_EXISTS ||
      result == DBUS_REQUEST_NAME_REPLY_IN_QUEUE)
    {
      g_warning ("%s. %s already exists",
                 __FUNCTION__,
                 HD_SV_NOTIFICATION_DAEMON_DBUS_NAME);
      goto cleanup;
    }

  dbus_g_connection_register_g_object (connection,
                                       HD_SV_NOTIFICATION_DAEMON_DBUS_PATH,
                                       G_OBJECT (sv_nd));

  load_sv_plugin (sv_nd);

  if (priv->nsv_plugin_load)
    priv->nsv_plugin_load ();

cleanup:
  if (bus_proxy)
    g_object_unref (bus_proxy);
}

static void
hd_sv_notification_daemon_finalize (GObject *object)
{
  HDSVNotificationDaemonPrivate *priv = HD_SV_NOTIFICATION_DAEMON (object)->priv;

  if (priv->nsv_plugin_unload)
    priv->nsv_plugin_unload ();

  if (priv->nsv_module)
    priv->nsv_module = (g_module_close (priv->nsv_module), NULL);

  G_OBJECT_CLASS (hd_sv_notification_daemon_parent_class)->finalize (object);
}

static void
hd_sv_notification_daemon_class_init (HDSVNotificationDaemonClass *klass)
{
  GObjectClass *g_object_class = G_OBJECT_CLASS (klass);

/*  g_object_class->dispose = hd_sv_notification_daemon_dispose; */
  g_object_class->finalize = hd_sv_notification_daemon_finalize;

  g_type_class_add_private (klass, sizeof (HDSVNotificationDaemonPrivate));
}

gboolean
hd_sv_notification_daemon_play_event (HDSVNotificationDaemon *sv_nd,
                                      GHashTable             *hints,
                                      const gchar            *notification_sender,
                                      DBusGMethodInvocation  *context)
{
  HDSVNotificationDaemonPrivate *priv = sv_nd->priv;
  gint id = -1;

  if (priv->nsv_plugin_play_event)
    id = priv->nsv_plugin_play_event (hints,
                                      notification_sender);

  dbus_g_method_return (context, id);

  return TRUE;
}

gboolean
hd_sv_notification_daemon_stop_event  (HDSVNotificationDaemon *sv_nd,
                                       gint                    id,
                                       DBusGMethodInvocation  *context)
{
  HDSVNotificationDaemonPrivate *priv = sv_nd->priv;

  if (priv->nsv_plugin_stop_event)
    priv->nsv_plugin_stop_event (id);

  dbus_g_method_return (context, id);

  return TRUE;
}

static void
mlock_process ()
{
  uid_t ruid, euid, suid;
#if 0
  struct rlimit rl;
#endif

  if (getresuid (&ruid, &euid, &suid) != 0)
    {
      g_warning ("getresuid failed: %s", g_strerror (errno));
      return;
    }

/*  g_debug ("process running as ruid: %u, euid: %u, suid: %u",
           ruid, euid, suid);*/

#if 0
  rl.rlim_cur = MEMLOCK_LIMIT;
  rl.rlim_max = MEMLOCK_LIMIT;
  if (setrlimit (RLIMIT_MEMLOCK, &rl) != 0)
    {
      g_warning ("setrlimit failed: %s, no mlock performed",
                 g_strerror (errno));
      return;
    }
#endif

  if (euid != ruid &&
      setresuid (ruid, ruid, ruid) != 0)
    {
      g_warning ("setresuid failed: %s, process still running as %u",
                 g_strerror (errno), euid);
    }

#if 0
  if (mlockall (MCL_CURRENT | MCL_FUTURE) != 0)
    {
      g_warning ("mlockall failed: %s", g_strerror (errno));
    }
/*  else
    {
      g_warning ("process sucessful mlocked");
    }*/
#endif
}

/* Log handler which ignores debug output */
static void
log_ignore_debug_handler (const gchar *log_domain,
                          GLogLevelFlags log_level,
                          const gchar *message,
                          gpointer unused_data)
{
  if (log_level != G_LOG_LEVEL_DEBUG)
    g_log_default_handler (log_domain,
                           log_level,
                           message,
                           unused_data);
}

int
main (int argc, char **argv)
{
  GMainLoop *loop;
  GObject *hd_sv_notification_daemon;

  mlock_process ();

  /* Ignore debug output */
  g_log_set_default_handler (log_ignore_debug_handler, NULL);

  g_type_init ();

  hd_sv_notification_daemon = g_object_new (HD_TYPE_SV_NOTIFICATION_DAEMON,
                                            NULL);

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);
  g_main_loop_unref (loop);

  g_object_unref (hd_sv_notification_daemon);

  return 0;
}
