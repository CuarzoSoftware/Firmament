project(
    'cz-firmament',
    'c', 'cpp',
    version : '0.1.0',
    meson_version: '>= 0.60.0',
    default_options: [
        'warning_level=2',
        'cpp_std=c++20',
    ]
)

cz_core_dep         = dependency('cz-core', fallback:['cz-core', 'cz_core_dep'])
cz_xdgkit_dep       = dependency('cz-xdgkit', fallback:['cz-xdgkit', 'cz_xdgkit_dep'])
cz_skia_dep         = dependency('cz-skia', fallback:['cz-skia', 'cz_skia_dep'])
cz_ream_dep         = dependency('cz-ream', fallback:['cz-ream', 'cz_ream_dep'])
cz_kay_dep          = dependency('cz-kay', fallback:['cz-kay', 'cz_kay_dep'])
cz_marco_dep        = dependency('cz-marco', fallback:['cz-marco', 'cz_marco_dep'])
cz_heaven_dep       = dependency('cz-heaven-bar', fallback:['cz-heaven', 'cz_heaven_bar_dep'])
cz_heaven_client_dep = dependency('cz-heaven-client', fallback:['cz-heaven', 'cz_heaven_client_dep'])

executable(
    'cz-firmament',
    sources : run_command('find', './src', '-type', 'f', '-name', '*[.cpp,.c]', check : false).stdout().strip().split('\n'),
    include_directories: ['./src'],
    dependencies : [
        cz_core_dep,
        cz_xdgkit_dep,
        cz_skia_dep,
        cz_ream_dep,
        cz_kay_dep,
        cz_marco_dep,
        cz_heaven_dep
    ],
    install : true)

executable(
    'cz-firmament-test-client',
    sources : ['test-client/main.cpp'],
    dependencies : [
        cz_core_dep,
        cz_heaven_client_dep
    ],
    install : true)
