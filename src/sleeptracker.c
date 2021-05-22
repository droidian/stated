/* sleeptracker.c
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

#define G_LOG_DOMAIN "stated-sleeptracker"

#define SLEEPTRACKER_WAKELOCK "stated_sleeptracker"

#include "sleeptracker.h"

/**
 * StatedSleeptracker allows to keep track of the sleep status and to
 * get notified on resumes.
 *
 * This uses the same idea as mce, where a timerfd on the REALTIME clock
 * is created and watched. When a resume happens, receiving ECANCELED means
 * that the realtime clock is getting synced back after suspend - thus
 * giving us a clue.
 */

struct _StatedSleeptracker
{
  GObject parent_instance;

  int watched_fd;
  GIOChannel *watched_channel;
  GSource *watched_source;

  uint64_t previous_boottime;
};

enum {
  SIGNAL_RESUME,
  N_SIGNALS
};
static uint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE (StatedSleeptracker, stated_sleeptracker, G_TYPE_OBJECT)

static void stated_sleeptracker_rearm_timer (StatedSleeptracker *self);

static gboolean
on_timer_changed (GIOChannel *source,
                  GIOCondition  cond,
                  StatedSleeptracker  *self)
{
  ssize_t cnt = 0;
  int8_t ret;
  uint64_t now;

  ret = read (self->watched_fd, &cnt, sizeof cnt);

  if (ret == -1 && errno == ECANCELED) {
    /* FIXME: handle real time changes! */
    now = time_get_boottime ();
    g_debug ("Resume detected");
    g_signal_emit (G_OBJECT (self), signals[SIGNAL_RESUME], 0,
                   self->previous_boottime, now);

    /* Update previous_boottime */
    self->previous_boottime = now;
  }

  stated_sleeptracker_rearm_timer (self);

cleanup:
  wakelock_unlock (SLEEPTRACKER_WAKELOCK);

  return G_SOURCE_CONTINUE;
}


static void
stated_sleeptracker_close_timer (StatedSleeptracker *self)
{
  if (self->watched_fd > 0) {
    g_source_destroy (self->watched_source);

    g_io_channel_unref (self->watched_channel);

    close (self->watched_fd);
    self->watched_fd = -1;
  }
}

static void
stated_sleeptracker_rearm_timer (StatedSleeptracker *self)
{
  static const struct itimerspec tspec = {
    .it_value.tv_sec = INT_MAX,
  };
  GIOChannel *channel;
  GSource *source;

  g_return_if_fail (STATED_IS_SLEEPTRACKER (self));

  stated_sleeptracker_close_timer (self);

  self->watched_fd = timerfd_create (CLOCK_REALTIME, O_NONBLOCK | O_CLOEXEC);
  if (self->watched_fd < 0) {
    g_warning ("Unable to create sleep tracker timerfd: %s", g_strerror (errno));
    return;
  }

  /* Attach the fd to glib's event loop */
  self->watched_channel = g_io_channel_unix_new (self->watched_fd);
  g_io_channel_set_encoding (self->watched_channel, NULL, NULL);

  self->watched_source = g_io_create_watch (self->watched_channel, G_IO_IN);
  g_source_set_callback (self->watched_source,
                         G_SOURCE_FUNC (on_timer_changed),
                         self, NULL);
  g_source_attach (self->watched_source, g_main_context_default ());

  if (timerfd_settime (self->watched_fd, TFD_TIMER_CANCEL_ON_SET | TFD_TIMER_ABSTIME,
                       &tspec, NULL) < 0) {
    g_error ("Unable to rearm sleep tracker timer: %s", g_strerror (errno));
  }
}

static void
stated_sleeptracker_constructed (GObject *obj)
{
  StatedSleeptracker *self = STATED_SLEEPTRACKER (obj);

  G_OBJECT_CLASS (stated_sleeptracker_parent_class)->constructed (obj);

  self->previous_boottime = time_get_boottime ();

  /* Arm the timer */
  stated_sleeptracker_rearm_timer (self);

}

static void
stated_sleeptracker_dispose (GObject *obj)
{
  StatedSleeptracker *self = STATED_SLEEPTRACKER (obj);

   G_OBJECT_CLASS (stated_sleeptracker_parent_class)->dispose (obj);

  stated_sleeptracker_close_timer (self);
}

static void
stated_sleeptracker_class_init (StatedSleeptrackerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed  = stated_sleeptracker_constructed;
  object_class->dispose      = stated_sleeptracker_dispose;

  signals[SIGNAL_RESUME] =
  g_signal_new ("resume",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0,
                NULL,
                NULL,
                NULL,
                G_TYPE_NONE,
                2,
                G_TYPE_UINT64,
                G_TYPE_UINT64);
}

static void
stated_sleeptracker_init (StatedSleeptracker *self)
{
}

StatedSleeptracker *
stated_sleeptracker_new (void)
{
  return g_object_new (STATED_TYPE_SLEEPTRACKER, NULL);
}
