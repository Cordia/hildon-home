/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2009 Nokia Corporation.
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

#include <dbus/dbus-glib-bindings.h>
#include <mce/dbus-names.h>
#include <mce/mode-names.h>

#include "hd-led-pattern.h"

#define HD_LED_PATTERN_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_LED_PATTERN, HDLedPatternPrivate))

struct _HDLedPatternPrivate
{
  gchar *name;
};

enum
{
  PROP_0,
  PROP_NAME,
};

static void hd_led_pattern_dispose      (GObject *object);
static void hd_led_pattern_get_property (GObject      *object,
                                         guint         prop_id,
                                         GValue       *value,
                                         GParamSpec   *pspec);
static void hd_led_pattern_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec);
static void hd_led_pattern_constructed  (GObject *object);

static void            activate_pattern   (HDLedPattern *pattern);
static void            deactivate_pattern (HDLedPattern *pattern);

static GHashTable      *get_pattern_map            (void);
static DBusGProxy      *get_mce_proxy              (void);
static DBusGConnection *get_system_dbus_connection (void);

G_DEFINE_TYPE (HDLedPattern, hd_led_pattern, G_TYPE_INITIALLY_UNOWNED);

HDLedPattern *
hd_led_pattern_get (const gchar *name)
{
  GHashTable *pattern_map = get_pattern_map ();
  HDLedPattern *pattern;

  pattern = g_hash_table_lookup (pattern_map,
                                 name);

  if (!pattern)
    {
      pattern = g_object_new (HD_TYPE_LED_PATTERN,
                              "name", name,
                              NULL);
      g_hash_table_insert (pattern_map,
                           g_strdup (name),
                           pattern);
    }

  return g_object_ref_sink (pattern);
}

static GHashTable *
get_pattern_map (void)
{
  static GHashTable *pattern_map = NULL;

  if (G_UNLIKELY (!pattern_map))
    {
      pattern_map = g_hash_table_new_full (g_str_hash, g_str_equal,
                                           (GDestroyNotify) g_free,
                                           NULL);
    }

  return pattern_map;
}

static void
hd_led_pattern_class_init (HDLedPatternClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = hd_led_pattern_dispose;
  object_class->get_property = hd_led_pattern_get_property;
  object_class->set_property = hd_led_pattern_set_property;
  object_class->constructed = hd_led_pattern_constructed;

  g_type_class_add_private (klass, sizeof (HDLedPatternPrivate));

  g_object_class_install_property (object_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name",
                                                        "Name",
                                                        "Pattern name",
                                                        NULL,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hd_led_pattern_init (HDLedPattern *pattern)
{
  pattern->priv = HD_LED_PATTERN_GET_PRIVATE (pattern);
}

static void
hd_led_pattern_constructed (GObject *object)
{
  HDLedPattern *pattern = HD_LED_PATTERN (object);

  if (G_OBJECT_CLASS (hd_led_pattern_parent_class)->constructed)
    G_OBJECT_CLASS (hd_led_pattern_parent_class)->constructed (object);

  activate_pattern (pattern);
}

static void
activate_pattern (HDLedPattern *pattern)
{
  HDLedPatternPrivate *priv = pattern->priv;
  DBusGProxy *mce_proxy;

  mce_proxy = get_mce_proxy ();

  if (mce_proxy)
    {
      GError *error = NULL;

      dbus_g_proxy_call (mce_proxy,
                         MCE_ACTIVATE_LED_PATTERN,
                         &error,
                         G_TYPE_STRING,
                         priv->name,
                         G_TYPE_INVALID,
                         G_TYPE_INVALID);

      if (error)
        {
          g_debug ("%s. Could not activate LED pattern: %s. %s", __FUNCTION__, priv->name, error->message);
          g_error_free (error);
        }
      else
        g_debug ("%s. Activated LED pattern: %s", __FUNCTION__, priv->name);
    }
}

static void
deactivate_pattern (HDLedPattern *pattern)
{
  HDLedPatternPrivate *priv = pattern->priv;
  DBusGProxy *mce_proxy;

  mce_proxy = get_mce_proxy ();

  if (mce_proxy)
    {
      g_debug ("%s. Dectivate LED pattern: %s", __FUNCTION__, priv->name);

      dbus_g_proxy_call_no_reply (mce_proxy,
                                  MCE_DEACTIVATE_LED_PATTERN,
                                  G_TYPE_STRING,
                                  priv->name,
                                  G_TYPE_INVALID,
                                  G_TYPE_INVALID);
    }
}

static DBusGProxy *
get_mce_proxy (void)
{
  static DBusGProxy *mce_proxy = NULL;

  if (G_UNLIKELY (!mce_proxy))
    {
      DBusGConnection *system_dbus = get_system_dbus_connection ();

      if (system_dbus)
        mce_proxy = dbus_g_proxy_new_for_name (system_dbus,
                                               MCE_SERVICE,
                                               MCE_REQUEST_PATH,
                                               MCE_REQUEST_IF);
    }

  return mce_proxy;
}

static DBusGConnection *
get_system_dbus_connection (void)
{
  DBusGConnection *system_dbus;
  GError *error = NULL;

  system_dbus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);

  if (error)
    {
      g_warning ("%s. Could not connect to System D-Bus. %s",
                 __FUNCTION__,
                 error->message);
      g_error_free (error);
    }

  return system_dbus;
}

static void
hd_led_pattern_dispose (GObject *object)
{
  HDLedPattern *pattern = HD_LED_PATTERN (object);
  HDLedPatternPrivate *priv = pattern->priv;

  if (priv->name)
    {
      GHashTable *pattern_map = get_pattern_map ();

      g_hash_table_remove (pattern_map,
                           priv->name);

      deactivate_pattern (pattern);

      priv->name = (g_free (priv->name), NULL);
    }

  G_OBJECT_CLASS (hd_led_pattern_parent_class)->dispose (object);
}

static void
hd_led_pattern_get_property (GObject      *object,
                             guint         prop_id,
                             GValue       *value,
                             GParamSpec   *pspec)
{
  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
hd_led_pattern_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  HDLedPatternPrivate *priv = HD_LED_PATTERN (object)->priv;

  switch (prop_id)
    {
    case PROP_NAME:
      priv->name = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

const static char *default_notification_pattern[] = 
{
  "PatternCommunicationCall",
  "PatternCommunicationIM",
  "PatternCommunicationSMS",
  "PatternCommunicationEmail",
  "PatternCommonNotification",
  NULL
};

void
hd_led_pattern_deactivate_all (void)
{
  DBusGProxy *mce_proxy;
  guint i;

  mce_proxy = get_mce_proxy ();

  if (mce_proxy)
    {
      for (i = 0; default_notification_pattern[i]; i++)
        {
          g_debug ("%s. Dectivate LED pattern: %s",
                   __FUNCTION__,
                   default_notification_pattern[i]);

          dbus_g_proxy_call_no_reply (mce_proxy,
                                      MCE_DEACTIVATE_LED_PATTERN,
                                      G_TYPE_STRING,
                                      default_notification_pattern[i],
                                      G_TYPE_INVALID,
                                      G_TYPE_INVALID);
        }
    }
}
