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

#define G_LOG_DOMAIN "stated-input"

#include "input.h"

struct _StatedInput
{
  GObject parent_instance;

  /* TODO: support multiple keys */
  uint key;

  struct libevdev *watched_dev;
  int watched_fd;
  GIOChannel *watched_channel;
  GSource *watched_source;
};

typedef enum {
  STATED_INPUT_PROP_KEY = 1,
  STATED_INPUT_PROP_LAST
} StatedInputProperty;

static GParamSpec *props[STATED_INPUT_PROP_LAST] = { NULL, };

enum {
  SIGNAL_POWERKEY_PRESSED,
  N_SIGNALS
};
static uint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE (StatedInput, stated_input, G_TYPE_OBJECT)

static gboolean
on_input_change (GIOChannel *source,
                 GIOCondition  cond,
                 StatedInput  *self)
{
  struct input_event ev;
  int rc;
  do {
    rc = libevdev_next_event (self->watched_dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
    if (rc == LIBEVDEV_READ_STATUS_SUCCESS
        && ev.type == EV_KEY && ev.code == self->key
        && ev.value == 1) {
      g_signal_emit (G_OBJECT (self), signals[SIGNAL_POWERKEY_PRESSED], 0);
    }
  } while (rc != -EAGAIN);

  return G_SOURCE_CONTINUE;
}


/**
 * Searches for a suitable input device for the specified key.
 * Returns an (opened) fd, or -1 if no suitable input
 * device has been found.
 */
static int
open_input_device_for_key (uint                     key,
                           struct libevdev **target_dev)
{
  int fd, rc;
  struct libevdev *dev;
  g_autoptr(GFileEnumerator) enumerator = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) source = NULL;
  GFileInfo *info;
  GFile *child;

  source = g_file_new_for_path ("/dev/input");
  enumerator = g_file_enumerate_children (source,
                                          G_FILE_ATTRIBUTE_STANDARD_NAME,
                                          G_FILE_QUERY_INFO_NONE,
                                          NULL, &error);

  if (error == NULL) {
    while (g_file_enumerator_iterate (enumerator,
                                      &info, &child,
                                      NULL, &error)) {
      g_autofree char *name = NULL, *lowered_devname = NULL;
      GFileType filetype;
      g_autoptr(GFile) candidate = NULL;

      if (child == NULL || info == NULL)
        break;

      filetype = g_file_info_get_file_type (info);
      if (!(filetype == G_FILE_TYPE_SPECIAL || filetype == G_FILE_TYPE_SYMBOLIC_LINK))
        continue;

      name = g_file_get_path (child);
      if (name != NULL && g_str_has_prefix (name, "/dev/input/event")) {
        g_debug ("Opening %s", name);
        fd = open (name, O_RDONLY|O_NONBLOCK);
        if (fd == -1) {
          g_warning ("Unable to open %s, %s",
                     name, g_strerror (errno));
          goto cleanup_and_continue;
        }

        rc = libevdev_new_from_fd (fd, &dev);
        if (rc < 0) {
          g_warning ("Unable to open libevdev device %s, %s",
                     name, g_strerror (errno));
          goto cleanup_and_continue;
        }


        lowered_devname = g_ascii_strdown (libevdev_get_name (dev), -1);
        if (libevdev_has_event_code (dev, EV_KEY, key)
            && !g_strrstr (lowered_devname, "keyboard")) { /* FIXME: Shouldn't exclude keyboards */
          /* Found! */
          g_debug ("Found key on device %s", name);
          *target_dev = dev;
          return fd;
        } else {
          g_debug ("Device %s doesn't support the specified key", name);
          goto cleanup_and_continue;
        }

cleanup_and_continue:
        if (dev != NULL) {
          libevdev_free (dev);
          dev = NULL;
        }
        if (fd > 0) {
          close (fd);
          fd = -1;
        }
      }
    }
  }

  return -1;
}

static void
stated_input_constructed (GObject *obj)
{
  StatedInput *self = STATED_INPUT (obj);
  GIOChannel *channel;
  GSource *source;

  /* Search for a suitable input device */
  self->watched_fd = open_input_device_for_key (self->key, &self->watched_dev);
  if (self->watched_fd < 0) {
    g_warning ("Unable to find suitable device for key %d", self->key);
    return;
  }

  /* Attach the fd to glib's event loop */
  self->watched_channel = g_io_channel_unix_new (self->watched_fd);
  g_io_channel_set_encoding (self->watched_channel, NULL, NULL);

  self->watched_source = g_io_create_watch (self->watched_channel, G_IO_IN);
  g_source_set_callback (self->watched_source,
                         G_SOURCE_FUNC (on_input_change),
                         self, NULL);
  g_source_attach (self->watched_source, g_main_context_default ());

  G_OBJECT_CLASS (stated_input_parent_class)->constructed (obj);
}

static void
stated_input_dispose (GObject *obj)
{
  StatedInput *self = STATED_INPUT (obj);

  g_source_destroy (self->watched_source);

  g_io_channel_unref (self->watched_channel);

  if (self->watched_dev != NULL) {
    libevdev_free (self->watched_dev);
    self->watched_dev = NULL;
  }

  if (self->watched_fd > 0) {
    close (self->watched_fd);
    self->watched_fd = -1;
  }

  G_OBJECT_CLASS (stated_input_parent_class)->dispose (obj);
}

static void
stated_input_set_property (GObject      *obj,
                           uint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  StatedInput *self = STATED_INPUT (obj);

  switch ((StatedInputProperty) property_id)
    {
    case STATED_INPUT_PROP_KEY:
      self->key = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
      break;
    }

}

static void
stated_input_get_property (GObject    *obj,
                           uint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  StatedInput *self = STATED_INPUT (obj);

  switch ((StatedInputProperty) property_id)
    {
    case STATED_INPUT_PROP_KEY:
      g_value_set_uint (value, self->key);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
      break;
    }

}


static void
stated_input_class_init (StatedInputClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed  = stated_input_constructed;
  object_class->dispose      = stated_input_dispose;
  object_class->set_property = stated_input_set_property;
  object_class->get_property = stated_input_get_property;

  signals[SIGNAL_POWERKEY_PRESSED] =
  g_signal_new ("powerkey-pressed",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0,
                NULL,
                NULL,
                NULL,
                G_TYPE_NONE,
                0);

  props[STATED_INPUT_PROP_KEY] =
    g_param_spec_uint ("key",
                       "key",
                       "The key to watch events for",
                       0,
                       -1,
                       KEY_POWER,
                       G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, STATED_INPUT_PROP_LAST, props);

}


static void
stated_input_init (StatedInput *self)
{

}

StatedInput *
stated_input_new_for_key (uint key)
{
  return g_object_new (STATED_TYPE_INPUT, "key", key, NULL);
}
