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

/* Hashtable that keeps track of expiring timed locks */
static GHashTable *expiring_wakelocks = NULL;
static GMutex expiring_wakelocks_mutex;

static gboolean
on_expiring_wakelocks_removal (void *key_ptr, void * value_ptr, void* data)
{
  char *lock_name = (char *)key_ptr;
  uint source_id = GPOINTER_TO_UINT (value_ptr);

  g_debug ("%s: removing pending wakelock, including source", lock_name);
  g_source_remove (source_id);
  wakelock_unlock (lock_name);

  return TRUE;
}

static void
on_key_should_be_destroyed (void *data)
{
  char *key = (char *)data;

  g_debug ("Destroying hashtable key %s", key);
  g_free (key);
}

static gboolean
on_wakelock_timeout_elapsed (char *lock_name)
{
  /* Remove the wakelock */
  g_debug ("Timeout elapsed for wakelock %s, unlocking", lock_name);
  wakelock_unlock (lock_name);

  g_hash_table_remove (expiring_wakelocks, lock_name);

  return G_SOURCE_REMOVE;
}

static void
check_if_supported ()
{
  if (access (wakelock_lock_file, F_OK) == 0) {
    wakelocks_supported = 1;
    g_debug ("Wakelocks supported");

    expiring_wakelocks = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                (GDestroyNotify) on_key_should_be_destroyed,
                                                NULL);
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

/**
 * Adds a timed wakelock
 */
void
wakelock_timed (char* lock_name, uint timeout)
{
  uint source_id;
  char *dup_lock_name;

  if (wakelocks_supported < 0)
    check_if_supported ();

  if (!wakelocks_supported) {
    g_debug ("Unable to add a timed wakelock: not supported");
    return;
  }

  g_mutex_lock (&expiring_wakelocks_mutex);

  if (!g_hash_table_contains (expiring_wakelocks, lock_name)) {
    /* Add a new wakelock */
    wakelock_lock(lock_name);
  } else {
    source_id = (uint)g_hash_table_lookup(expiring_wakelocks, lock_name);

    /* Rearm by removing the old source */
    if (source_id != NULL) {
      g_debug ("%s: wakelock already tracked; assuming a rearm", lock_name);
      g_source_remove (source_id);
    }
  }

  dup_lock_name = g_strdup (lock_name);

  g_debug ("%s: adding timeout (%d secs)", dup_lock_name, timeout);
  source_id = g_timeout_add_seconds_full (G_PRIORITY_HIGH, timeout,
                                          G_SOURCE_FUNC (on_wakelock_timeout_elapsed),
                                          dup_lock_name, NULL);

  g_debug ("%s: inserting into hash table", dup_lock_name);
  g_hash_table_replace (expiring_wakelocks, dup_lock_name, GUINT_TO_POINTER (source_id));

  g_mutex_unlock (&expiring_wakelocks_mutex);
}

/**
 * Cancels a timed wakelock.
 * If keep_track is TRUE, only the pending source is removed, the wakelock
 * itself will be kept.
 */
void
wakelock_cancel (char* lock_name, gboolean keep_lock)
{
  uint source_id;

  if (wakelocks_supported < 0)
    check_if_supported ();

  if (!wakelocks_supported) {
    g_debug ("Unable to cancel a timed wakelock: not supported");
    return;
  }

  g_mutex_lock (&expiring_wakelocks_mutex);

  if (g_hash_table_contains (expiring_wakelocks, lock_name)) {
    source_id = (uint)g_hash_table_lookup(expiring_wakelocks, lock_name);

    if (source_id != NULL) {
      g_debug ("%s: asked to cancel timeout", lock_name);
      g_source_remove (source_id);
      g_hash_table_remove (expiring_wakelocks, lock_name);
    }

    /* Remove wakelock, if keep_lock is FALSE */
    if (!keep_lock) {
      g_debug ("%s: removing wakelock since keep_lock is FALSE", lock_name);
      wakelock_unlock (lock_name);
    }
  }

  g_mutex_unlock (&expiring_wakelocks_mutex);
}

/**
 * Cancels all pending wakelocks
 */
void
wakelock_cancel_all (void)
{
  if (wakelocks_supported < 0)
    check_if_supported ();

  if (!wakelocks_supported) {
    g_debug ("Unable to cancel all timed wakelocks: not supported");
    return;
  }

  g_mutex_lock (&expiring_wakelocks_mutex);

  g_hash_table_foreach_remove (expiring_wakelocks,
                               (GHRFunc) on_expiring_wakelocks_removal, NULL);

  g_mutex_unlock (&expiring_wakelocks_mutex);
}
