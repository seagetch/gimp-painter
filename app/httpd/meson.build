# No HTTP headers, transport, or linker dependency when painter-http is disabled.
httpd_sources = files('httpd.cpp', 'httpd-resource.cpp', 'httpd-images.cpp', 'httpd-pdb.cpp')
libapphttpd = static_library('apphttpd', httpd_sources,
  include_directories: [rootInclude, rootAppInclude],
  dependencies: [gegl, gtk3, json_glib, painter_http_soup],
  cpp_args: painter_cpp_args, override_options: ['cpp_std=c++14'])
libapphttpdgui = static_library('apphttpdgui', 'httpd-navigation.cpp',
  include_directories: [rootInclude, rootAppInclude],
  dependencies: [gegl, gtk3, json_glib],
  cpp_args: painter_cpp_args, override_options: ['cpp_std=c++14'])
