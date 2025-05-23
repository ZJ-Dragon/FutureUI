panels_list += cappletname
desktop = 'gnome-@0@-panel.desktop'.format(cappletname)

i18n.merge_file(
  type: 'desktop',
  input: desktop + '.in',
  output: desktop,
  po_dir: po_dir,
  install: true,
  install_dir: control_center_desktopdir
)

polkit_conf = configuration_data()
polkit_conf.set('libexecdir', control_center_libexecdir)

polkit = 'org.gnome.controlcenter.remote-login-helper.policy'

polkit_in = configure_file(
  input: polkit + '.in.in',
  output: polkit + '.in',
  configuration: polkit_conf
)

i18n.merge_file(
  input: polkit_in,
  output: polkit,
  po_dir: po_dir,
  install: true,
  install_dir: join_paths(control_center_datadir, 'polkit-1', 'actions')
)

sources = files(
  'cc-sharing-panel.c',
  'cc-media-sharing.c',
  'cc-remote-login.c',
  'cc-sharing-networks.c',
  'cc-gnome-remote-desktop.c',
  'cc-tls-certificate.c',
  'cc-systemd-service.c',
  'file-share-properties.c',
)

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  c_name: 'cc_' + cappletname,
  export: true
)

settings_daemon = 'org.gnome.SettingsDaemon'
gdbus = settings_daemon + '.Sharing'

sources += gnome.gdbus_codegen(
  gdbus,
  gdbus + '.xml',
  interface_prefix: settings_daemon + '.',
  namespace: 'Gsd'
)

cflags += [
  '-DLIBEXECDIR="@0@"'.format(control_center_libexecdir),
  '-DSYSCONFDIR="@0@"'.format(control_center_sysconfdir)
]

libsecret_dep = dependency('libsecret-1')
gnutls_dep = dependency('gnutls')

panels_libs += static_library(
  cappletname,
  sources: sources,
  include_directories: [ top_inc, common_inc ],
  dependencies: [
    common_deps,
    libsecret_dep,
    gnutls_dep,
    gcr_dep,
    pwquality_dep,
  ],
  c_args: cflags
)

name = 'cc-remote-login-helper'

deps = [
  gio_dep,
  glib_dep,
]

executable(
  name,
  sources: [
    name + '.c',
    'cc-systemd-service.c',
  ],
  include_directories: top_inc,
  dependencies: deps,
  c_args: cflags,
  install: true,
  install_dir: control_center_libexecdir
)

subdir('icons')
