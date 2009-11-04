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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include <locale.h>

#include "hd-time-difference.h"

#define MINUTE (60)
#define HOUR   (MINUTE * 60)
#define DAY    (HOUR * 24)
#define YEAR   (DAY * 365)

typedef struct
{
  time_t unit;
  time_t next_unit;
  const char *message_id;
  const char *message_id_plural;
} TimeDiffInfo;

const static TimeDiffInfo entries[] = {
  { MINUTE, HOUR, "wdgt_va_ago_one_minute", "wdgt_va_ago_minutes" },
  { HOUR, DAY, "wdgt_va_ago_one_hour", "wdgt_va_ago_hours"},
  { DAY, YEAR, "wdgt_va_ago_one_day", "wdgt_va_ago_days" },
  { YEAR, -1, "wdgt_va_ago_one_year", "wdgt_va_ago_years" }
};

static inline gchar *
get_time_diff_text_for_info (const TimeDiffInfo *info,
                             time_t              difference)
{
  time_t diff_in_unit = (difference + (info->unit / 2)) / info->unit;

  return g_strdup_printf (dngettext ("hildon-libs",
                                     info->message_id,
                                     info->message_id_plural,
                                     diff_in_unit),
                          diff_in_unit);
}

static inline gboolean
should_time_displayed (time_t difference)
{
  return difference >= (MINUTE / 2);
}

static const TimeDiffInfo *
get_time_diff_info_for_difference (time_t difference)
{
  if (should_time_displayed (difference))
    {
      guint i;

      for (i = 0; i < G_N_ELEMENTS (entries); i++)
        {
          time_t unit = entries[i].unit;
          time_t next_unit = entries[i].next_unit;

          if (next_unit == -1 ||
              difference < (next_unit - unit / 2))
            return &entries[i];
        }
    }

  return NULL;
}

char *
hd_time_difference_get_text (time_t difference)
{
  const TimeDiffInfo *info;

  info = get_time_diff_info_for_difference (difference);

  if (info)
    return get_time_diff_text_for_info (info,
                                        difference);
  else
    return NULL;
}

static inline time_t
get_timeout_for_info (const TimeDiffInfo *info,
                      time_t              difference)
{
  time_t timeout = info->unit - ((difference + (info->unit / 2)) % info->unit);

  if (info->next_unit != -1)
    timeout = MIN (timeout,
                   info->next_unit - difference);

  return timeout;
}

time_t
hd_time_difference_get_timeout (time_t difference)
{
  const TimeDiffInfo *info;

  info = get_time_diff_info_for_difference (difference);

  if (info)
    return get_timeout_for_info (info,
                                 difference);
  else
    return (MINUTE / 2) - difference;
}

#ifdef COMPILE_FOR_TEST
typedef struct
{
  time_t      difference;
  const char *expected_text;
  time_t      expected_timeout;
} TestData;

static const TestData test_data[] =
{
    { 0, NULL, MINUTE / 2 },
    { MINUTE / 2 - 1, NULL, 1 },
    { MINUTE / 2, "1 minute ago", MINUTE},
    { MINUTE, "1 minute ago", MINUTE / 2},
    { 3 * MINUTE / 2 - 1, "1 minute ago", 1},
    { 3 * MINUTE / 2, "2 minutes ago", MINUTE},
    { HOUR - 31, "59 minutes ago", 1},
    { HOUR - 30, "1 hour ago", HOUR / 2 + 30},
    { HOUR, "1 hour ago", HOUR / 2},
    { 3 * HOUR / 2 - 1, "1 hour ago", 1},
    { 3 * HOUR / 2, "2 hours ago", HOUR},
};

static void
test_time_difference (gconstpointer test_data)
{
  const TestData *data = test_data;
  time_t difference = data->difference;
  gchar *text = hd_time_difference_get_text (difference);
  time_t timeout = hd_time_difference_get_timeout (difference);

  g_test_message ("Test time difference: %ld\n",
                  difference);

  g_assert_cmpstr (text, ==, data->expected_text);
  g_assert_cmpint (timeout, ==, data->expected_timeout);
}

int main (int argc, char **argv)
{
  guint i;

  setlocale (LC_ALL, "");
  bindtextdomain ("hildon-libs", "/usr/share/locale");
  textdomain ("hildon-libs");

  g_test_init (&argc, &argv, NULL);

  for (i = 0; i < G_N_ELEMENTS (test_data); i++)
    {
      gconstpointer data = &test_data[i];
      g_test_add_data_func ("/time-difference", data, test_time_difference);
    }

  return g_test_run ();
}

#endif
