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

#ifndef __HD_COMMAND_THREAD_POOL_H__
#define __HD_COMMAND_THREAD_POOL_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define HD_TYPE_COMMAND_THREAD_POOL            (hd_command_thread_pool_get_type ())
#define HD_COMMAND_THREAD_POOL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_COMMAND_THREAD_POOL, HDCommandThreadPool))
#define HD_COMMAND_THREAD_POOL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  HD_TYPE_COMMAND_THREAD_POOL, HDCommandThreadPoolClass))
#define HD_IS_COMMAND_THREAD_POOL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_COMMAND_THREAD_POOL))
#define HD_IS_COMMAND_THREAD_POOL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  HD_TYPE_COMMAND_THREAD_POOL))
#define HD_COMMAND_THREAD_POOL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  HD_TYPE_COMMAND_THREAD_POOL, HDCommandThreadPoolClass))

typedef struct _HDCommandThreadPool        HDCommandThreadPool;
typedef struct _HDCommandThreadPoolClass   HDCommandThreadPoolClass;
typedef struct _HDCommandThreadPoolPrivate HDCommandThreadPoolPrivate;

struct _HDCommandThreadPool 
{
  GObject gobject;

  HDCommandThreadPoolPrivate *priv;
};

struct _HDCommandThreadPoolClass 
{
  GObjectClass parent_class;
};

typedef void (*HDCommandCallback) (gpointer data);

GType                hd_command_thread_pool_get_type  (void);

HDCommandThreadPool *hd_command_thread_pool_new       (void);

void                 hd_command_thread_pool_push      (HDCommandThreadPool *pool,
                                                       HDCommandCallback    command,
                                                       gpointer             data,
                                                       GDestroyNotify       destroy_data);
void                 hd_command_thread_pool_push_idle (HDCommandThreadPool *pool,
                                                       gint                 priority,
                                                       GSourceFunc          function,
                                                       gpointer             data,
                                                       GDestroyNotify       destroy_data);

G_END_DECLS

#endif /* __HD_COMMAND_THREAD_POOL_H__ */
