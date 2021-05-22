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

#define G_LOG_DOMAIN "stated-devicestate"

#define DISPLAY_WAKELOCK "stated_display"
#define POWERKEY_WAKELOCK "stated_powerkey_timer"
#define RESUME_WAKELOCK "stated_resume_timer"
#define DEFAULT_WAIT_TIME 10

/* Resume behaviour */
#define RESUME_LOCK_WAIT_TIME 2
#define RESUME_MAX_CEILING 7

#include "wakelocks.h"
#include "devicestate.h"
#include "display.h"
#include "input.h"
#include "sleeptracker.h"

static uint64_t RESUME_LOOP_THRESHOLD = (uint64_t)15000; /* 15 secs */

struct _StatedDevicestate
{
  GObject parent_instance;

  /* instance members */
  StatedDisplay *primary_display;
  StatedInput *powerkey_input;
  StatedSleeptracker *sleep_tracker;
  gboolean primary_display_on;

  uint8_t subsequent_resumes;
};

G_DEFINE_TYPE (StatedDevicestate, stated_devicestate, G_TYPE_OBJECT)

static void
on_display_status_changed (StatedDevicestate *self,
                           GParamSpec    *pspec,
                           StatedDisplay *display)
{
  GValue value = G_VALUE_INIT;

  g_return_if_fail (STATED_IS_DEVICESTATE (self));
  g_return_if_fail (STATED_IS_DISPLAY (display));

  g_value_init (&value, pspec->value_type);
  g_object_get_property (G_OBJECT (display), pspec->name, &value);

  if (g_value_get_boolean (&value) == TRUE) {
    g_debug ("Display on, setting wakelock");
    wakelock_lock (DISPLAY_WAKELOCK);

    /* Cancel an eventual timeout triggered by a previous display shutdown */
    wakelock_cancel (DISPLAY_WAKELOCK, TRUE);
  } else {
    g_debug ("Display off, scheduling wakelock removal");

    wakelock_timed (DISPLAY_WAKELOCK, DEFAULT_WAIT_TIME);
  }

  g_value_unset (&value);
}

static void
on_powerkey_pressed (StatedDevicestate *self,
                     StatedInput      *input)
{
  g_return_if_fail (STATED_IS_DEVICESTATE (self));
  g_return_if_fail (STATED_IS_INPUT (input));

  /* Add a timeout to remove the wakelock */
  wakelock_timed (POWERKEY_WAKELOCK, DEFAULT_WAIT_TIME);
}

static void
on_resume (StatedDevicestate  *self,
           uint64_t           previous_boottime,
           uint64_t           new_boottime,
           StatedSleeptracker *sleep_tracker)
{
  uint64_t time_offset;

  g_return_if_fail (STATED_IS_DEVICESTATE (self));
  g_return_if_fail (STATED_IS_SLEEPTRACKER (sleep_tracker));

  /* Always obtain a wakelock for RESUME_WAKELOCK */
  wakelock_lock (RESUME_WAKELOCK);

  /* Try to detect subsequent sleep/resume loops and damper them,
   * the logic is as follows:
   *
   * - Always obtain a timed wakelock, using subsequent_resumes * RESUME_LOCK_WAIT_TIME
   * - If a "sleep/resume loop" is detected, increment subsequent_resumes so that
   *   the device spends more time awake. The ceiling is RESUME_MAX_CEILING (7),
   *   so that means that the timed wakelock will last at most for 14 seconds.
   */

  /* StatedSleeptracker only tracks resumes for now, so we're unable to precisely
   * get the time of sleep. Calculate a time offset with what has been possibly
   * been the previous wakelock timeout and use it to augment the supplied
   * previous_boottime.
   *
   * TODO: once StatedSleeptracker supports tracking sleep entries, remove
   * this.
   */
  time_offset = (self->subsequent_resumes == 0) ? 0
                : RESUME_LOCK_WAIT_TIME * (self->subsequent_resumes + 1) * 1000;

  g_debug ("now - previous_bootime: %lu", (new_boottime - previous_boottime + time_offset));

  if ((new_boottime - previous_boottime + time_offset) < RESUME_LOOP_THRESHOLD) {
    /* Assume this is a sleep/resume loop. */
    self->subsequent_resumes = MIN (self->subsequent_resumes + 1,
                                    RESUME_MAX_CEILING);
    g_warning ("Resume loop detected, subsequent_resumes raised to %d",
             self->subsequent_resumes);
  } else {
    /* Clear counter */
    self->subsequent_resumes = 1;
  }

  /* Add a timer for the lock we previously obtained */
  wakelock_timed (RESUME_WAKELOCK,
                  RESUME_LOCK_WAIT_TIME * self->subsequent_resumes);
}

static void
stated_devicestate_constructed (GObject *obj)
{
  StatedDevicestate *self = STATED_DEVICESTATE (obj);

  G_OBJECT_CLASS (stated_devicestate_parent_class)->constructed (obj);

  self->primary_display = stated_display_new ();

  self->powerkey_input = stated_input_new_for_key (KEY_POWER);

  self->sleep_tracker = stated_sleeptracker_new ();
  self->subsequent_resumes = 1;

  g_signal_connect_object (self->primary_display, "notify::on",
                           G_CALLBACK (on_display_status_changed),
                           self, G_CONNECT_SWAPPED);

  g_signal_connect_object (self->powerkey_input, "powerkey-pressed",
                           G_CALLBACK (on_powerkey_pressed),
                           self, G_CONNECT_SWAPPED);

  g_signal_connect_object (self->sleep_tracker, "resume",
                           G_CALLBACK (on_resume),
                           self, G_CONNECT_SWAPPED);
}

static void
stated_devicestate_dispose (GObject *obj)
{
  StatedDevicestate *self = STATED_DEVICESTATE (obj);

  g_clear_object (&self->primary_display);
  g_clear_object (&self->powerkey_input);
  g_clear_object (&self->sleep_tracker);

  G_OBJECT_CLASS (stated_devicestate_parent_class)->dispose (obj);
}

static void
stated_devicestate_class_init (StatedDevicestateClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = stated_devicestate_constructed;
  object_class->dispose = stated_devicestate_dispose;
}

static void
stated_devicestate_init (StatedDevicestate *self)
{
}

StatedDevicestate *
stated_devicestate_new (void)
{
  return g_object_new (STATED_TYPE_DEVICESTATE, NULL);
}
