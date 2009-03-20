/*
 * This file is part of hildon-home
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

#include <hildon/hildon.h>

#include "hd-incoming-event-window.h"

static void
response (GtkWidget *win, int code)
{
  if (code == GTK_RESPONSE_OK)
    g_warning ("GTK_RESPONSE_OK");
  else if (code == GTK_RESPONSE_DELETE_EVENT)
    g_warning ("GTK_RESPONSE_DELETE_EVENT");
  else
    g_warning ("response is unknown (%d)", code);

  gtk_main_quit ();
}

int
main (int argc, char **argv)
{
  GtkWidget *window;

  gtk_init (&argc, &argv);
  hildon_init ();

  /* Demo notification window */
  window = hd_incoming_event_window_new (argv[1] == NULL,
                                         "Osso_calculator",
                                         "Email Sender Freaked Out Badly",
                                         "Email Subject",
                                         12220784,
                                         "qgn_list_messagin");
  g_signal_connect (window, "response", G_CALLBACK (response), NULL);
  gtk_widget_show (window);

  /* Start the main loop */
  gtk_main ();

  return 0;
}
