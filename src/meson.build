src_files = [
  'gstmorsesrc.c',
]

shared_library('gstmorsesrc', src_files,
  dependencies: [gst_dep, gstbase_dep, glib_dep],
  install: true,
  install_dir: join_paths(get_option('libdir'), 'gstreamer-1.0')
)
