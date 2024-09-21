project('morsesrc', 'c',
  default_options: ['warning_level=3', 'optimization=0', 'debug=true'])

gst_dep = dependency('gstreamer-1.0')
gstbase_dep = dependency('gstreamer-base-1.0')
gstaudio_dep = dependency('gstreamer-audio-1.0')
glib_dep = dependency('glib-2.0')
gobject_dep = dependency('gobject-2.0')

cc = meson.get_compiler('c')
math_lib = cc.find_library('m', required: true)

src = [
  'src/gstmorsesrc.c',
]

libgstmorsesrc = shared_library('gstmorsesrc', src,
  dependencies: [gstaudio_dep, gstbase_dep, gst_dep, glib_dep, gobject_dep, math_lib],
  install: true
)

install_dir = join_paths(get_option('prefix'), 'lib', 'gstreamer-1.0')
