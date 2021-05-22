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

#define G_LOG_DOMAIN "stated-utils"

#include "utils.h"

/**
 * Helper function that allows to write the given content to a file
 *
 * Returns 0 on success, -1 on failure.
 */
int
sysfs_write (char *content, char *sysfs_file)
{
  /* TODO: Check if we're actually going to write in /sys? */

  FILE* file = fopen (sysfs_file, "w");
  if (file == NULL) {
    g_warning ("Unable to open file (%s) for writing",
               sysfs_file);
    return -1;
  }

  fputs (content, file);
  fclose (file);

  return 0;
}

static uint64_t
time_get_current (uint8_t clk)
{
  struct timespec tspec;

  clock_gettime (clk, &tspec);

  return (uint64_t)tspec.tv_sec * 1000 + tspec.tv_nsec / 1000000;
}

/**
 * Helper function that gets the current monotonic time, in milliseconds.
 */
uint64_t
time_get_monotonic (void)
{
  return time_get_current (CLOCK_MONOTONIC);
}

/**
 * Helper function that gets the current boottime, in milliseconds.
 */
uint64_t
time_get_boottime (void)
{
  return time_get_current (CLOCK_BOOTTIME);
}
