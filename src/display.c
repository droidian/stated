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

#define G_LOG_DOMAIN "stated-display"

#include "display.h"

static const char qcom_display_state_file[] = "/sys/class/graphics/fb0/show_blank_event"; /* FIXME: support other displays */
/* TODO: allow detecting screen status on other devices / allow feeding state from compositor */

struct _StatedDisplay
{
  GObject parent_instance;

  /* instance members */
  uint display_id;
  gboolean on;

  GFile *watched_file;
  GFileMonitor *watched_file_monitor;
};

typedef enum {
  STATED_DISPLAY_PROP_ON = 1,
  STATED_DISPLAY_PROP_LAST
} StatedDisplayProperty;

static GParamSpec *props[STATED_DISPLAY_PROP_LAST] = { NULL, };

G_DEFINE_TYPE (StatedDisplay, stated_display, G_TYPE_OBJECT)

static void
on_qcom_display_state_changed (GFileMonitor      *monitor,
                               GFile             *file,
                               GFile             *other_file,
                               GFileMonitorEvent event_type,
                               StatedDisplay     *self)
{
  g_return_if_fail (STATED_IS_DISPLAY (self));

  char *file_contents = NULL;

  if (event_type == G_FILE_MONITOR_EVENT_CHANGED) {
    if (!g_file_get_contents (qcom_display_state_file, /* FIXME: read from GFile instead */
                             &file_contents, NULL, NULL))
      goto end;

    if (g_strcmp0 (file_contents, "panel_power_on = 1\n") == 0) {
      g_debug ("qcom display powered on!");
      self->on = TRUE;
    } else {
      g_debug ("qcom display powered off!");
      self->on = FALSE;
    }

    /* We should manually notify since the property is read-only */
    g_object_notify (G_OBJECT (self), "on");
  }

end:
  if (file_contents != NULL) {
    g_free (file_contents);
  }
  return;
}

static void
stated_display_constructed (GObject *obj)
{
  StatedDisplay *self = STATED_DISPLAY (obj);

  G_OBJECT_CLASS (stated_display_parent_class)->constructed (obj);

  if (access (qcom_display_state_file, F_OK) == 0) {
    g_debug ("Found qcom display state file");

    self->watched_file = g_file_new_for_path (qcom_display_state_file);
    self->watched_file_monitor = g_file_monitor_file (self->watched_file,
                                                      G_FILE_MONITOR_NONE,
                                                      NULL,
                                                      NULL);

    if (self->watched_file_monitor != NULL) {
      g_signal_connect_object (self->watched_file_monitor, "changed",
                               G_CALLBACK (on_qcom_display_state_changed), self,
                               0);

      /* Trigger initial check */
      on_qcom_display_state_changed (self->watched_file_monitor,
                                     self->watched_file,
                                     NULL, G_FILE_MONITOR_EVENT_CHANGED,
                                     self);
    }

  }
}

static void
stated_display_dispose (GObject *obj)
{
  StatedDisplay *self = STATED_DISPLAY (obj);

  if (self->watched_file_monitor) {
    g_file_monitor_cancel (self->watched_file_monitor);
    g_clear_object (&self->watched_file_monitor);
  }

  if (self->watched_file) {
    g_clear_object (&self->watched_file);
  }

   G_OBJECT_CLASS (stated_display_parent_class)->dispose (obj);
}

static void
stated_display_set_property (GObject      *obj,
                             uint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  StatedDisplay *self = STATED_DISPLAY (obj);

  switch ((StatedDisplayProperty) property_id)
    {
    case STATED_DISPLAY_PROP_ON:
      /* Read-only */
      g_warning ("The 'on' property is read only!");
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
      break;
    }

}

static void
stated_display_get_property (GObject    *obj,
                             uint       property_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  StatedDisplay *self = STATED_DISPLAY (obj);

  switch ((StatedDisplayProperty) property_id)
    {
    case STATED_DISPLAY_PROP_ON:
      g_value_set_boolean (value, self->on);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
      break;
    }

}


static void
stated_display_class_init (StatedDisplayClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed  = stated_display_constructed;
  object_class->dispose      = stated_display_dispose;
  object_class->set_property = stated_display_set_property;
  object_class->get_property = stated_display_get_property;

  props[STATED_DISPLAY_PROP_ON] =
    g_param_spec_boolean ("on",
                          "on",
                          "Whether the display is on or off",
                          TRUE,
                          G_PARAM_READABLE);

  g_object_class_install_properties (object_class, STATED_DISPLAY_PROP_LAST, props);
}

static void
stated_display_init (StatedDisplay *self)
{
}

StatedDisplay *
stated_display_new (void)
{
  return g_object_new (STATED_TYPE_DISPLAY, NULL);
}
