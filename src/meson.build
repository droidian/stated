stated_sources = [
  'main.c',
  'utils.c',
  'wakelocks.c',
  'devicestate.c',
  'display.c',
  'display-file.c',
  'input.c',
  'sleep.c',
  'sleeptracker.c',
]

stated_deps = [
  dependency('glib-2.0'),
  dependency('gobject-2.0'),
  dependency('gio-2.0'),
  dependency('libevdev'),
]

executable('stated', stated_sources,
  dependencies: stated_deps,
  install: true,
)
