import os

extra_path = []

VariantDir('build/rt', 'src/rt', duplicate=0)
VariantDir('build/cl-gl', 'src/cl-gl', duplicate=0)
VariantDir('build/misc', 'src/misc', duplicate=0)
VariantDir('build/gpu', 'src/gpu', duplicate=0)

env = Environment(ENV = os.environ)
env.AppendENVPath('PATH', extra_path)

# env.Append(CCFLAGS = '-g -Wall -Wextra -O3')
env.Append(CCFLAGS = '-g -Wall ')

env.Replace(CXX = 'llvm-clang')
# env.Replace(CXX = 'g++')

cl_root = env['ENV']['CL_ROOT']

base_libs = ['GL' , 'glut' , 'GLEW' , 'OpenCL', 'm', 'freeimageplus', 'rt']
libpath = [cl_root + '/lib/x86_64' , '/usr/lib/fglrx' ]
cpppath = [cl_root + '/include' , 'include'  ]

env['LIBS'] = base_libs
env['CPPPATH'] = cpppath
env['LIBPATH'] = libpath
env.Append(CPPDEFINES=['GLEW_STATIC'])

misc_lib = env.StaticLibrary('lib/misc' ,
                             ['build/misc/ini.cpp'] 
                             )

clgl_lib = env.StaticLibrary('lib/clgl' ,
                             Glob('build/cl-gl/open[cg]l-init.cpp')
                             )

gpu_lib = env.StaticLibrary('lib/gpu' ,
                            ['build/gpu/function.cpp',
                             'build/gpu/memory.cpp',
                             'build/gpu/interface.cpp',
                             'build/gpu/function-library.cpp',
                             'build/gpu/scan.cpp'
                             ])

rt_primitives_lib = env.StaticLibrary('lib/rt-primitives' ,
                                      ['build/rt/vector.cpp',
                                       'build/rt/matrix.cpp',
                                       'build/rt/math.cpp',
                                       'build/rt/geom.cpp',
                                       'build/rt/timing.cpp',
                                       'build/rt/cl_aux.cpp',
                                       'build/rt/ray.cpp',
                                       'build/rt/primary-ray-generator.cpp',
                                       'build/rt/secondary-ray-generator.cpp',
                                       'build/rt/mesh.cpp',
                                       'build/rt/material.cpp',
                                       'build/rt/obj-loader.cpp',
                                       'build/rt/camera.cpp',
                                       'build/rt/camera-trajectory.cpp',
                                       'build/rt/obj-loader.cpp',
                                       'build/rt/bbox.cpp',
                                       'build/rt/bvh.cpp',
                                       'build/rt/kdtree.cpp',
                                       'build/rt/multi-bvh.cpp',
                                       'build/rt/scene.cpp',
                                       'build/rt/texture-atlas.cpp',
                                       'build/rt/cubemap.cpp',
                                       'build/rt/framebuffer.cpp',
                                       'build/rt/bvh-builder.cpp',
                                       'build/rt/ray-shader.cpp',
                                       'build/rt/tracer.cpp',
                                       'build/rt/renderer.cpp',
                                       'build/rt/renderer-config.cpp',
                                       'build/rt/renderer-threaded.cpp'
                                       ])

clgl_test = env.Program('bin/cl-gl-test' ,
                        'build/cl-gl/clgl-test.cpp' ,
                        LIBS = clgl_lib + base_libs 
                        )   

clgl_test_threaded = env.Program('bin/cl-gl-test-threaded' ,
                                 'build/cl-gl/clgl-test-threaded.cpp' ,
                                 LIBS = clgl_lib + base_libs + ["pthread"] 
                                 )   

rt = env.Program('bin/rt' ,
                 'build/rt/rt.cpp' ,
                 LIBS= rt_primitives_lib + gpu_lib + misc_lib + clgl_lib + base_libs
                 )   

# rtt = env.Program('bin/rtt' ,
#                   'build/rt/rtt.cpp' ,
#                   LIBS= base_libs + clgl_lib + rt_primitives_lib + gpu_lib + misc_lib
#                   )   

# rt_seq = env.Program('bin/rt-seq' ,
#                      'build/rt/rt-seq.cpp' ,
#                      LIBS= base_libs + clgl_lib + rt_primitives_lib + gpu_lib + misc_lib
#                      )   

rt_wave = env.Program('bin/rt-wave' ,
                      'build/rt/rt-wave.cpp' ,
                      LIBS= rt_primitives_lib + gpu_lib + misc_lib + clgl_lib + base_libs
                      )   

rt_tester = env.Program('bin/rt-test' ,
                        ['build/rt/tester.cpp', 'build/rt/rt-test.cpp'] ,
                        LIBS= rt_primitives_lib + gpu_lib + misc_lib + clgl_lib + base_libs
                        )   

sort = env.Program('bin/sort' ,
                   'build/rt/sort.cpp' ,
                   LIBS= rt_primitives_lib + gpu_lib + misc_lib + clgl_lib + base_libs
                   )   
