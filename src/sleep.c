/* wakelocks.c
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

#define G_LOG_DOMAIN "stated-sleep"

#include "sleep.h"
#include "utils.h"

static const char autosleep_file[]   = "/sys/power/autosleep";

/* -1: not checked, 0: not supported, 1: supported */
static int autosleep_supported = -1;

static void
check_if_supported ()
{
  if (access (autosleep_file, F_OK) == 0) {
    autosleep_supported = 1;
    g_debug ("Autosleep supported");
  } else {
    autosleep_supported = 0;
    g_warning ("Autosleep not supported");
  }
}

int
autosleep_enable (void)
{
  if (autosleep_supported < 0)
    check_if_supported ();

  if (autosleep_supported && sysfs_write ("mem", autosleep_file) == 0) {
    g_debug ("Autosleep enabled!");
    return 0;
  } else {
    g_warning ("Unable to enable autosleep");
    return -1;
  }
}

int
autosleep_disable (void)
{
  if (autosleep_supported < 0)
    check_if_supported ();

  if (autosleep_supported && sysfs_write ("off", autosleep_file) == 0) {
    g_debug ("Autosleep disabled!");
    return 0;
  } else {
    g_warning ("Unable to disable autosleep");
    return -1;
  }
}
