import os

extra_path = []

env = Environment(ENV = os.environ)
env.AppendENVPath('PATH', extra_path)

env.Append(CCFLAGS = '-g -Wall -O3')

env.Replace(CXX = 'llvm-clang')

cl_root = env['ENV']['CL_ROOT']

base_libs = ['GL' , 'glut' , 'GLEW' , 'OpenCL', 'rt', 'm', 'freeimageplus']
libpath = [cl_root + '/lib/x86_64' , '/usr/lib/fglrx' ]
cpppath = [cl_root + '/include' , 'include' ]

env['LIBS'] = base_libs
env['CPPPATH'] = cpppath
env['LIBPATH'] = libpath


rt_primitives_lib = env.StaticLibrary('lib/rt-primitives' ,
                                      ['src/rt/vector.cpp',
                                       'src/rt/matrix.cpp',
                                       'src/rt/math.cpp',
                                       'src/rt/geom.cpp',
                                       'src/rt/cl_aux.cpp',
                                       'src/rt/timing.cpp',
                                       'src/rt/ray.cpp',
                                       'src/rt/mesh.cpp',
                                       'src/rt/material.cpp',
                                       'src/rt/obj-loader.cpp',
                                       'src/rt/camera.cpp',
                                       'src/rt/primary-ray-generator.cpp',
                                       'src/rt/obj-loader.cpp',
                                       'src/rt/bvh.cpp',
                                       'src/rt/scene.cpp'
                                       ])

clgl_lib = env.StaticLibrary('lib/clgl' ,
                             Glob('src/cl-gl/open[cg]l-init.cpp')
                             )

clgl_test = env.Program('bin/cl-gl-test' ,
                        'src/cl-gl/clgl-test.cpp' ,
                        LIBS = base_libs + clgl_lib
                        )   

rt = env.Program('bin/rt' ,
                 'src/rt/rt.cpp' ,
                 LIBS= base_libs + clgl_lib + rt_primitives_lib
                 )   
