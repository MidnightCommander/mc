# Meson build file sample

# Project definition
project(
    'sample-project',
    'c', 'cpp',
    version : '1.0.0',
    license : 'MIT',
    default_options : [
        'c_std=c11',
        'cpp_std=c++17',
        'warning_level=3',
    ],
)

# Comments use hash
# Built-in objects
compiler = meson.get_compiler('c')
build_type = meson.get_option('buildtype')
src_dir = meson.current_source_dir()
bld_dir = meson.current_build_dir()
proj_name = meson.project_name()
proj_ver = meson.project_version()
is_cross = meson.is_cross_build()
host_sys = host_machine.system()
host_cpu = host_machine.cpu_family()
build_sys = build_machine.system()

# Boolean values
debug_mode = true
release_mode = false

# String operations
greeting = 'Hello, World!'
multiline = '''This is a
multi-line string'''
name = 'meson'
upper = name.to_upper()

# Dependencies
glib_dep = dependency('glib-2.0', required : true)
thread_dep = dependency('threads')
math_dep = compiler.find_library('m', required : false)
zlib_dep = dependency('zlib', required : false)

# Source files
sources = files(
    'src/main.c',
    'src/utils.c',
    'src/parser.c',
)

extra_sources = files('src/extra.c')

# Include directories
inc = include_directories('include', 'src')

# Conditionals
if debug_mode
    add_project_arguments('-DDEBUG', language : 'c')
    message('Debug mode enabled')
elif build_type == 'release'
    add_project_arguments('-DNDEBUG', language : 'c')
    message('Release mode')
else
    warning('Unknown build type')
endif

# Logical operators
if debug_mode and not release_mode
    message('Development build')
endif

if host_sys == 'linux' or host_sys == 'freebsd'
    message('Unix-like system')
endif

# Foreach loop
test_names = ['unit', 'integration', 'functional']
foreach t : test_names
    test_src = files('tests' / t + '_test.c')
    test_exe = executable(
        t + '_test',
        test_src,
        dependencies : [glib_dep],
        include_directories : inc,
    )
    test(t, test_exe)
endforeach

# Dictionary
config_data = {
    'version' : proj_ver,
    'prefix' : get_option('prefix'),
    'debug' : debug_mode,
}

# Configuration
conf = configuration_data()
conf.set('VERSION', proj_ver)
conf.set('PACKAGE', proj_name)
conf.set10('HAVE_ZLIB', zlib_dep.found())

configure_file(
    input : 'config.h.in',
    output : 'config.h',
    configuration : conf,
)

# Main executable
main_exe = executable(
    'sample-app',
    sources,
    dependencies : [glib_dep, thread_dep, math_dep],
    include_directories : inc,
    install : true,
)

# Library targets
shared_library(
    'sample-shared',
    sources,
    dependencies : [glib_dep],
    install : true,
    version : proj_ver,
)

static_library(
    'sample-static',
    sources,
    dependencies : [glib_dep],
)

# Subdir
subdir('docs')
subdir('tests')

# Custom target
custom_target(
    'generate-docs',
    output : 'docs.html',
    command : [find_program('doxygen'), '@INPUT@'],
    input : 'Doxyfile',
    install : true,
    install_dir : get_option('datadir') / 'doc',
)

# Run command
result = run_command('pkg-config', '--version', check : true)
message('pkg-config version: ' + result.stdout().strip())

# Install data
install_data('data/config.ini', install_dir : 'etc')
install_headers('include/sample.h')

# Summary
summary(
    {
        'Build type' : build_type,
        'Host system' : host_sys,
        'Debug mode' : debug_mode,
    },
    section : 'Configuration',
)
