import os

extra_path = []

VariantDir('build/rt', 'src/rt', duplicate=0)
VariantDir('build/cl-gl', 'src/cl-gl', duplicate=0)

env = Environment(ENV = os.environ)
env.AppendENVPath('PATH', extra_path)

# env.Append(CCFLAGS = '-g -Wall -O3')
env.Append(CCFLAGS = '-g -Wall ')

env.Replace(CXX = 'llvm-clang')

cl_root = env['ENV']['CL_ROOT']

base_libs = ['GL' , 'glut' , 'GLEW' , 'OpenCL', 'rt', 'm', 'freeimageplus']
libpath = [cl_root + '/lib/x86_64' , '/usr/lib/fglrx' ]
cpppath = [cl_root + '/include' , 'include' ]

env['LIBS'] = base_libs
env['CPPPATH'] = cpppath
env['LIBPATH'] = libpath


rt_primitives_lib = env.StaticLibrary('lib/rt-primitives' ,
                                      ['build/rt/vector.cpp',
                                       'build/rt/matrix.cpp',
                                       'build/rt/math.cpp',
                                       'build/rt/geom.cpp',
                                       'build/rt/cl_aux.cpp',
                                       'build/rt/timing.cpp',
                                       'build/rt/ray.cpp',
                                       'build/rt/primary-ray-generator.cpp',
                                       'build/rt/secondary-ray-generator.cpp',
                                       'build/rt/mesh.cpp',
                                       'build/rt/material.cpp',
                                       'build/rt/obj-loader.cpp',
                                       'build/rt/camera.cpp',
                                       'build/rt/obj-loader.cpp',
                                       'build/rt/bvh.cpp',
                                       'build/rt/multi-bvh.cpp',
                                       'build/rt/scene.cpp',
                                       'build/rt/cubemap.cpp',
                                       'build/rt/framebuffer.cpp',
                                       'build/rt/ray-shader.cpp',
                                       'build/rt/tracer.cpp'
                                       ])

clgl_lib = env.StaticLibrary('lib/clgl' ,
                             Glob('build/cl-gl/open[cg]l-init.cpp')
                             )

clgl_test = env.Program('bin/cl-gl-test' ,
                        'build/cl-gl/clgl-test.cpp' ,
                        LIBS = base_libs + clgl_lib
                        )   

rt = env.Program('bin/rt' ,
                 'build/rt/rt.cpp' ,
                 LIBS= base_libs + clgl_lib + rt_primitives_lib
                 )   

rt_seq = env.Program('bin/rt-seq' ,
                     'build/rt/rt-seq.cpp' ,
                     LIBS= base_libs + clgl_lib + rt_primitives_lib
                     )   

rt_wave = env.Program('bin/rt-wave' ,
                      'build/rt/rt-wave.cpp' ,
                      LIBS= base_libs + clgl_lib + rt_primitives_lib
                      )   
