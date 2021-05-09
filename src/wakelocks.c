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

#define G_LOG_DOMAIN "stated-wakelocks"

#include "utils.h"

static const char wakelock_lock_file[]   = "/sys/power/wake_lock";
static const char wakelock_unlock_file[] = "/sys/power/wake_unlock";

/* -1: not checked, 0: not supported, 1: supported */
static int wakelocks_supported = -1;

static void
check_if_supported ()
{
  if (access (wakelock_lock_file, F_OK) == 0) {
    wakelocks_supported = 1;
    g_debug ("Wakelocks supported");
  } else {
    wakelocks_supported = 0;
    g_warning ("Wakelocks not supported");
  }
}


/**
 * Adds a new wakelock.
 */
void
wakelock_lock (char* lock_name)
{
  if (wakelocks_supported < 0)
    check_if_supported ();

  if (wakelocks_supported && sysfs_write (lock_name, wakelock_lock_file) == 0) {
    g_debug ("Added wakelock %s", lock_name);
  }
}

/**
 * Removes a wakelock.
 */
void
wakelock_unlock (char* lock_name)
{
  if (wakelocks_supported < 0)
    check_if_supported ();

  if (wakelocks_supported && sysfs_write (lock_name, wakelock_unlock_file) == 0) {
    g_debug ("Removed wakelock %s", lock_name);
  }
}
