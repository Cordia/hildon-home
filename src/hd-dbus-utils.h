/*
 * This file is part of hildon-home
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

#ifndef __HD_DBUS_UTILS_H__
#define __HD_DBUS_UTILS_H__

#include <dbus/dbus-glib-bindings.h>

G_BEGIN_DECLS

DBusGConnection *hd_get_session_dbus_connection (void);
DBusGConnection *hd_get_system_dbus_connection (void);

G_END_DECLS

#endif /* __HD_DBUS_UTILS_H__ */
