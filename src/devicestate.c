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
#define DEFAULT_WAIT_TIME 10

#include "wakelocks.h"
#include "devicestate.h"
#include "display.h"
#include "input.h"

struct _StatedDevicestate
{
  GObject parent_instance;

  /* instance members */
  StatedDisplay *primary_display;
  StatedInput *powerkey_input;
  gboolean primary_display_on;
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
stated_devicestate_constructed (GObject *obj)
{
  StatedDevicestate *self = STATED_DEVICESTATE (obj);

  G_OBJECT_CLASS (stated_devicestate_parent_class)->constructed (obj);

  self->primary_display = stated_display_new ();

  self->powerkey_input = stated_input_new_for_key (KEY_POWER);

  g_signal_connect_object (self->primary_display, "notify::on",
                           G_CALLBACK (on_display_status_changed),
                           self, G_CONNECT_SWAPPED);

  g_signal_connect_object (self->powerkey_input, "powerkey-pressed",
                           G_CALLBACK (on_powerkey_pressed),
                           self, G_CONNECT_SWAPPED);
}

static void
stated_devicestate_dispose (GObject *obj)
{
  StatedDevicestate *self = STATED_DEVICESTATE (obj);

  g_clear_object (&self->primary_display);
  g_clear_object (&self->powerkey_input);

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
