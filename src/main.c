/* main.c
 *
 * Copyright 2021 Eugenio Paolantonio (g7)
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE X CONSORTIUM BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name(s) of the above copyright
 * holders shall not be used in advertising or otherwise to promote the sale,
 * use or other dealings in this Software without prior written
 * authorization.
 */

#include <glib-2.0/glib.h>
#include <stdlib.h>

#include "wakelocks.h"
#include "sleep.h"
#include "devicestate.h"
#include "stated-config.h"

static gboolean
handle_unix_signal (void* data)
{
  GMainLoop *loop = data;

  g_warning ("Asked to exit...");

  g_main_loop_quit (loop);

  return G_SOURCE_REMOVE;
}

int
main (int   argc,
      char *argv[])
{
  g_autoptr(GOptionContext) context = NULL;
  g_autoptr(GError) error = NULL;
  gboolean version = FALSE;
  GOptionEntry main_entries[] = {
    { "version", 0, 0, G_OPTION_ARG_NONE, &version, "Show program version" },
    { NULL }
  };

  context = g_option_context_new ("- keeper of the state");
  g_option_context_add_main_entries (context, main_entries, NULL);

  if (!g_option_context_parse (context, &argc, &argv, &error)) {
    g_printerr ("%s\n", error->message);
    return EXIT_FAILURE;
  }

  if (version) {
    g_printerr ("%s\n", PACKAGE_VERSION);
    return EXIT_SUCCESS;
  }

  StatedDevicestate *devicestate = stated_devicestate_new ();

  /* TODO: Enable autosleep only after system startup */
  autosleep_enable ();

  GMainLoop *loop = g_main_loop_new (NULL, FALSE);
  g_unix_signal_add (SIGTERM, G_SOURCE_FUNC (handle_unix_signal), loop);
  g_main_loop_run (loop);

  /* Cleanup */
  autosleep_disable ();
  wakelock_cancel_all ();
  g_clear_object (&devicestate);

  return EXIT_SUCCESS;
}
