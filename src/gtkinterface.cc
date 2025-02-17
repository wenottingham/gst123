/* GST123 - GStreamer based command line media player
 * Copyright (C) 2010 Stefan Westerfeld
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "gtkinterface.h"
#include "options.h"
#include "msg.h"

#include <gtk/gtk.h>
#include <X11/Xlib.h>
#include <gdk/gdkkeysyms.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif
#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif
#include <stdlib.h>
#include <string.h>

using std::string;
using namespace Gst123;

static gboolean
key_press_event_cb (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
  GtkInterface *gtk_interface = static_cast<GtkInterface *> (data);

  return gtk_interface->handle_keypress_event (event);
}

static gboolean
button_press_event_cb (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
  GtkInterface *gtk_interface = static_cast<GtkInterface *> (data);

  return gtk_interface->handle_buttonpress_event (event);
}

static gboolean
motion_notify_event_cb (GtkWidget *widget, GdkEventMotion *event, gpointer data)
{
  GtkInterface *gtk_interface = static_cast<GtkInterface *> (data);

  return gtk_interface->handle_motion_notify_event (event);
}

static gboolean
timeout_cb (gpointer data)
{
  GtkInterface *gtk_interface = static_cast<GtkInterface *> (data);

  return gtk_interface->handle_timeout();
}

static gboolean
close_cb (GtkWidget *widget,
          GdkEvent  *event,
          gpointer   data)
{
  GtkInterface *gtk_interface = static_cast<GtkInterface *> (data);

  return gtk_interface->handle_close();
}

static gboolean
window_state_event_cb (GtkWidget *widget,
                       GdkEvent  *event,
                       gpointer   data)
{
  GtkInterface *gtk_interface = static_cast<GtkInterface *> (data);
  GdkEventWindowState *ws_event = reinterpret_cast<GdkEventWindowState *> (event);

  return gtk_interface->handle_window_state_event (ws_event);
}

GtkInterface::GtkInterface() :
  video_width (0),
  video_height (0),
  video_fullscreen (false),
  video_maximized (false),
  need_resize_window (false)
{
}

void
GtkInterface::init (int *argc, char ***argv, KeyHandler *handler)
{
  key_handler = handler;

  if (gtk_init_check (argc, argv))
    {
      gtk_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      headerbar = gtk_header_bar_new ();
      gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (headerbar), true);
      gtk_window_set_titlebar (GTK_WINDOW (gtk_window), headerbar);
      gtk_window_set_icon_name (GTK_WINDOW (gtk_window), "multimedia-player");
      g_signal_connect (G_OBJECT (gtk_window), "key-press-event", G_CALLBACK (key_press_event_cb), this);
      g_signal_connect (G_OBJECT (gtk_window), "motion-notify-event", G_CALLBACK (motion_notify_event_cb), this);
      g_signal_connect (G_OBJECT (gtk_window), "delete-event", G_CALLBACK (close_cb), this);
      g_signal_connect (G_OBJECT (gtk_window), "window-state-event", G_CALLBACK (window_state_event_cb), this);
      g_object_set (G_OBJECT (gtk_window), "events", GDK_POINTER_MOTION_MASK, NULL);

      gtk_widget_realize (gtk_window);

      video_fullscreen = Options::the().fullscreen;    // initially fullscreen?

      // make background black
      GtkCssProvider* provider = gtk_css_provider_new();
      GdkDisplay* display = gdk_display_get_default();
      GdkScreen* screen = gdk_display_get_default_screen(display);

      gtk_style_context_add_provider_for_screen(screen,
                                                GTK_STYLE_PROVIDER (provider),
                                                GTK_STYLE_PROVIDER_PRIORITY_USER);

      gtk_css_provider_load_from_data(GTK_CSS_PROVIDER(provider),
                                      "GtkWindow {background-color: black;}\n", -1, NULL);
      g_object_unref(provider);

      visible_cursor = NULL;
      invisible_cursor = gdk_cursor_new_for_display (display, GDK_BLANK_CURSOR);

      cursor_timeout = 3;
      g_timeout_add (500, (GSourceFunc) timeout_cb, this);
    }
  else
    {
      gtk_window = NULL;
    }
  gtk_window_visible = false;

  /* initialize map from Gdk keysyms to KeyHandler codes */
  key_map[GDK_KEY_Page_Up]     = KEY_HANDLER_PAGE_UP;
  key_map[GDK_KEY_Page_Down]   = KEY_HANDLER_PAGE_DOWN;
  key_map[GDK_KEY_Left]        = KEY_HANDLER_LEFT;
  key_map[GDK_KEY_Right]       = KEY_HANDLER_RIGHT;
  key_map[GDK_KEY_Up]          = KEY_HANDLER_UP;
  key_map[GDK_KEY_Down]        = KEY_HANDLER_DOWN;
  key_map[GDK_KEY_BackSpace]   = KEY_HANDLER_BACKSPACE;
  key_map[GDK_KEY_KP_Add]      = '+';
  key_map[GDK_KEY_KP_Subtract] = '-';
}

bool
GtkInterface::is_fullscreen()
{
  g_return_val_if_fail (gtk_window != NULL && gtk_window_visible, false);

  GdkWindowState state = gdk_window_get_state (gtk_widget_get_window (gtk_window));
  return (state & GDK_WINDOW_STATE_FULLSCREEN);
}

bool
GtkInterface::is_maximized()
{
  g_return_val_if_fail (gtk_window != NULL && gtk_window_visible, false);

  GdkWindowState state = gdk_window_get_state (gtk_widget_get_window (gtk_window));
  return (state & GDK_WINDOW_STATE_MAXIMIZED);
}

void
GtkInterface::toggle_fullscreen()
{
  if (gtk_window != NULL && gtk_window_visible)
    {
      if (is_fullscreen()) {
        gtk_window_unfullscreen (GTK_WINDOW (gtk_window));
        gtk_widget_show (headerbar);
      } else {
        gtk_window_fullscreen (GTK_WINDOW (gtk_window));
        gtk_widget_hide (headerbar);
      }
    }
}

bool
GtkInterface::is_wayland()
{
#ifdef GDK_WINDOWING_WAYLAND
  GdkDisplay* display = gdk_display_get_default();

  if (GDK_IS_WAYLAND_DISPLAY (display)) {
    return true;
  }
#endif
  return false;
}

bool
GtkInterface::init_ok()
{
  return gtk_window != NULL;
}

void
GtkInterface::show(int width, int height)
{
  video_width = width;
  video_height = height;
  if (gtk_window != NULL && !gtk_window_visible)
    {
      // resize avoids showing big windows (avoids vertical or horizontal maximization)
      gtk_window_resize (GTK_WINDOW (gtk_window), width, height);

      gtk_widget_show_all (gtk_window);

      // restore fullscreen & maximized state
      if (video_fullscreen) {
        gtk_window_fullscreen (GTK_WINDOW (gtk_window));
        gtk_widget_hide (headerbar);
        need_resize_window = true;
      } else {
        gtk_widget_show (headerbar);
      }

      if (video_maximized) {
        gtk_window_maximize (GTK_WINDOW (gtk_window));
        need_resize_window = true;
      }

      // get cursor, so we can restore it after hiding it
      if (!visible_cursor)
        visible_cursor = gdk_window_get_cursor (gtk_widget_get_window (gtk_window));

      // sync, to make the window really visible before we return
      gdk_display_sync (gdk_display_get_default());

      screen_saver (SUSPEND);

      gtk_window_present (GTK_WINDOW (gtk_window));

      gtk_window_visible = true;
    }
}

void
GtkInterface::hide()
{
  if (gtk_window != NULL && gtk_window_visible)
    {
      video_fullscreen = is_fullscreen();
      if (video_fullscreen)
        gtk_window_unfullscreen (GTK_WINDOW (gtk_window));

      video_maximized  = is_maximized();
      if (video_maximized)
        gtk_window_unmaximize (GTK_WINDOW (gtk_window));

      gtk_widget_hide (gtk_window);

      screen_saver (RESUME);
      gtk_window_visible = false;
    }
}

void
GtkInterface::end()
{
  if (gtk_window != NULL)
    {
      screen_saver (RESUME);
    }
}

void
GtkInterface::resize_window_if_needed()
{
  if (gtk_window != NULL && gtk_window_visible && need_resize_window)
    {
      // when window is fullscreen or maximized, we cannot resize it to the
      // video size

      // in these cases we keep the need_resize_window flag true and resizing
      // will be done later on, when exiting fullscreen and/or maximization
      if (is_fullscreen() || is_maximized())
        return;

      gtk_window_resize (GTK_WINDOW (gtk_window), video_width, video_height);
      need_resize_window = false;
    }
}

void
GtkInterface::normal_size()
{
  if (gtk_window != NULL && gtk_window_visible)
    {
      gtk_window_unfullscreen (GTK_WINDOW (gtk_window));
      gtk_window_unmaximize (GTK_WINDOW (gtk_window));

      gtk_window_resize (GTK_WINDOW (gtk_window), video_width, video_height);
    }
}

void
GtkInterface::set_opacity (double alpha_change)
{
  if (gtk_window != NULL && gtk_window_visible)
    {
      double alpha;

      alpha = gtk_widget_get_opacity (GTK_WIDGET (gtk_window));
      alpha = CLAMP (alpha + alpha_change, 0.0, 1.0);
      Msg::update_status ("Opacity: %3.1f%%", alpha * 100);
      gtk_widget_set_opacity (GTK_WIDGET (gtk_window), alpha);
    }
}

bool
GtkInterface::handle_keypress_event (GdkEventKey *event)
{
  int ch = 0;

  if (event->keyval > 0 && event->keyval <= 127)
    ch = event->keyval;
  else
    ch = key_map[event->keyval];

  if (ch != 0)
    {
      key_handler->process_input (ch);
      return true;
    }
  return false;
}

bool
GtkInterface::handle_buttonpress_event (GdkEventButton *event)
{
  if (event->button == 1 && event->state == 0)
    {
      key_handler->process_input (' ');
      return true;
    }
  return false;
}

void
GtkInterface::set_title (const string& title)
{
  if (gtk_window != NULL)
    gtk_window_set_title (GTK_WINDOW (gtk_window), title.c_str());
}

void
GtkInterface::set_video_widget (GtkWidget *widget)
{
  video_widget = widget;
  gtk_container_add (GTK_CONTAINER(gtk_window), video_widget);
  g_signal_connect (G_OBJECT (video_widget), "button-press-event", G_CALLBACK (button_press_event_cb), this);
  gtk_widget_show (video_widget);
}

bool
GtkInterface::handle_timeout()
{
  if (gtk_window != NULL && gtk_window_visible)
    {
      if (cursor_timeout == 0)
        {
          gdk_window_set_cursor (gtk_widget_get_window (gtk_window), invisible_cursor);
          cursor_timeout = -1;
        }
      else if (cursor_timeout > 0)
        {
          cursor_timeout--;
        }
    }
  return true;
}

bool
GtkInterface::handle_motion_notify_event (GdkEventMotion *event)
{
  if (gtk_window != NULL && gtk_window_visible)
    {
      gdk_window_set_cursor (gtk_widget_get_window (gtk_window), visible_cursor);
      cursor_timeout = 3;
    }
  return true;
}

bool
GtkInterface::handle_window_state_event (GdkEventWindowState *event)
{
  if (gtk_window != NULL)
    {
      bool maximized_changed = (event->changed_mask & GDK_WINDOW_STATE_MAXIMIZED);
      bool maximized = (event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED);

      if (maximized_changed && !maximized)  // window unmaximized
        resize_window_if_needed();

      //----------------------------------------------------------------------------

      bool fullscreen_changed = (event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN);
      bool fullscreen = (event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN);

      if (fullscreen_changed && !fullscreen)  // window unfullscreened
        resize_window_if_needed();
    }
  return true;
}

bool
GtkInterface::handle_close()
{
  g_return_val_if_fail (gtk_window != NULL, true);

  // quit on close
  key_handler->process_input ('q');

  return true;
}

void
GtkInterface::screen_saver (ScreenSaverSetting setting)
{
  GdkWindow *window = gtk_widget_get_window (gtk_window);
  if (gtk_window != NULL && window && !is_wayland())
    {
      guint64 wid = GDK_WINDOW_XID (window);

      const char *setting_str = (setting == SUSPEND) ? "suspend" : "resume";

      char *cmd = g_strdup_printf ("xdg-screensaver %s %" G_GUINT64_FORMAT " >/dev/null 2>&1", setting_str, wid);
      int rc = system (cmd);   // don't complain if xdg-screensaver is not installed
      (void) rc;
      g_free (cmd);
    }
}
