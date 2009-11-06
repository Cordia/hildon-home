/*
 * This file is part of hildon-home
 *
 * Copyright (C) 2009 Nokia Corporation.
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

#include <gdk/gdk.h>

#include "hd-command-thread-pool.h"

#define HD_COMMAND_THREAD_POOL_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_COMMAND_THREAD_POOL, HDCommandThreadPoolPrivate))

typedef struct
{
  HDCommandCallback command;
  gpointer data;
  GDestroyNotify destroy_data;
} ThreadCommand;

static void           thread_command_execute (ThreadCommand *thread_command,
                                              gpointer       data);
static ThreadCommand *thread_command_new     (HDCommandCallback command,
                                              gpointer          data,
                                              GDestroyNotify    destroy_data);
static void           thread_command_free    (ThreadCommand *thread_command);

typedef struct
{
  gint priority;
  GSourceFunc function;
  gpointer data;
  GDestroyNotify destroy_data;
} IdleCommandData;

static IdleCommandData *idle_command_data_new  (gint           priority,
                                                GSourceFunc    function,
                                                gpointer       data,
                                                GDestroyNotify destroy_data);
static void             idle_command_execute   (IdleCommandData *command_data);
static void             idle_command_data_free (IdleCommandData *command_data);

struct _HDCommandThreadPoolPrivate
{
  GThreadPool *thread_pool;
};

G_DEFINE_TYPE (HDCommandThreadPool, hd_command_thread_pool, G_TYPE_OBJECT);

static void
hd_command_thread_pool_dipose (GObject *object)
{
  HDCommandThreadPoolPrivate *priv = HD_COMMAND_THREAD_POOL (object)->priv;

  if (priv->thread_pool)
    {
      g_thread_pool_free (priv->thread_pool,
                          FALSE,
                          TRUE);
      priv->thread_pool = NULL;
    }

  G_OBJECT_CLASS (hd_command_thread_pool_parent_class)->dispose (object);
}

static void
hd_command_thread_pool_class_init (HDCommandThreadPoolClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = hd_command_thread_pool_dipose;

  g_type_class_add_private (klass, sizeof (HDCommandThreadPoolPrivate));
}

static void
hd_command_thread_pool_init (HDCommandThreadPool *command_thread_pool)
{
  HDCommandThreadPoolPrivate *priv;

  priv = HD_COMMAND_THREAD_POOL_GET_PRIVATE (command_thread_pool);
  command_thread_pool->priv = priv;

  priv->thread_pool = g_thread_pool_new ((GFunc) thread_command_execute,
                                         NULL,
                                         1,
                                         FALSE,
                                         NULL);
}

static void
thread_command_execute (ThreadCommand *thread_command,
                        gpointer       data)
{
  thread_command->command (thread_command->data);
  thread_command_free (thread_command);
}

static ThreadCommand *
thread_command_new (HDCommandCallback command,
                    gpointer          data,
                    GDestroyNotify    destroy_data)
{
  ThreadCommand *thread_command = g_slice_new (ThreadCommand);

  thread_command->command = command;
  thread_command->data = data;
  thread_command->destroy_data = destroy_data;

  return thread_command;
}

static void
thread_command_free (ThreadCommand *thread_command)
{
  if (!thread_command)
    return;

  if (thread_command->destroy_data)
    thread_command->destroy_data (thread_command->data);

  g_slice_free (ThreadCommand, thread_command);
}

HDCommandThreadPool *
hd_command_thread_pool_new (void)
{
  return g_object_new (HD_TYPE_COMMAND_THREAD_POOL, NULL);
}

void
hd_command_thread_pool_push (HDCommandThreadPool *pool,
                             HDCommandCallback    command,
                             gpointer             data,
                             GDestroyNotify       destroy_data)
{
  HDCommandThreadPoolPrivate *priv;
  GError *error = NULL;

  g_return_if_fail (HD_IS_COMMAND_THREAD_POOL (pool));

  priv = pool->priv;

  ThreadCommand *thread_command = thread_command_new (command,
                                                      data,
                                                      destroy_data);
  g_thread_pool_push (priv->thread_pool,
                      thread_command,
                      &error);

  if (error)
    {
      g_debug ("%s. Error: %s", __FUNCTION__, error->message);
      g_error_free (error);
    }
}

void
hd_command_thread_pool_push_idle (HDCommandThreadPool *pool,
                                  gint                 priority,
                                  GSourceFunc          function,
                                  gpointer             data,
                                  GDestroyNotify       destroy_data)
{
  IdleCommandData *command_data;

  g_return_if_fail (HD_IS_COMMAND_THREAD_POOL (pool));

  command_data = idle_command_data_new (priority,
                                        function,
                                        data,
                                        destroy_data);

  hd_command_thread_pool_push (pool,
                               (HDCommandCallback) idle_command_execute,
                               command_data,
                               (GDestroyNotify) idle_command_data_free);
}

static IdleCommandData *
idle_command_data_new (gint           priority,
                       GSourceFunc    function,
                       gpointer       data,
                       GDestroyNotify destroy_data)
{
  IdleCommandData *command_data = g_slice_new (IdleCommandData);

  command_data->priority = priority;
  command_data->function = function;
  command_data->data = data;
  command_data->destroy_data = destroy_data;

  return command_data;
}

static void
idle_command_execute (IdleCommandData *command_data)
{
  gdk_threads_add_idle_full (command_data->priority,
                             command_data->function,
                             command_data->data,
                             command_data->destroy_data);
}

static void
idle_command_data_free (IdleCommandData *command_data)
{
  if (!command_data)
    return;

  g_slice_free (IdleCommandData, command_data);
}

