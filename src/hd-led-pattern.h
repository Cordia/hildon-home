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

#ifndef __HD_LED_PATTERN_H__
#define __HD_LED_PATTERN_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define HD_TYPE_LED_PATTERN             (hd_led_pattern_get_type ())
#define HD_LED_PATTERN(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_LED_PATTERN, HDLedPattern))
#define HD_LED_PATTERN_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_LED_PATTERN, HDLedPatternClass))
#define HD_IS_LED_PATTERN(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_LED_PATTERN))
#define HD_IS_LED_PATTERN_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_LED_PATTERN))
#define HD_LED_PATTERN_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_LED_PATTERN, HDLedPatternClass))

typedef struct _HDLedPattern        HDLedPattern;
typedef struct _HDLedPatternClass   HDLedPatternClass;
typedef struct _HDLedPatternPrivate HDLedPatternPrivate;

struct _HDLedPattern
{
  GInitiallyUnowned parent;

  HDLedPatternPrivate *priv;
};

struct _HDLedPatternClass
{
  GInitiallyUnownedClass parent;
};

GType            hd_led_pattern_get_type       (void);

HDLedPattern    *hd_led_pattern_get            (const gchar  *name);

G_END_DECLS

#endif

