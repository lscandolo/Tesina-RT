#pragma once
#ifndef RT_HPP
#define RT_HPP

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

#include <stdint.h>

#include <misc/log.hpp>
#include <misc/ini.hpp>
#include <gpu/interface.hpp>
#include <gpu/function-library.hpp>
#include <rt/math.hpp>
#include <rt/lu.hpp>
#include <rt/ray.hpp>
#include <rt/light.hpp>
#include <rt/timing.hpp>
#include <rt/camera.hpp>
#include <rt/camera-trajectory.hpp>
#include <rt/primary-ray-generator.hpp>
#include <rt/secondary-ray-generator.hpp>
#include <rt/cubemap.hpp>
#include <rt/framebuffer.hpp>
#include <rt/ray-shader.hpp>
#include <rt/tracer.hpp>
#include <rt/scene.hpp>
#include <rt/obj-loader.hpp>
#include <rt/scene.hpp>
#include <rt/bvh.hpp>
#include <rt/bvh-builder.hpp>
#include <rt/kdtree.hpp>
#include <rt/cl_aux.hpp>
#include <rt/renderer.hpp>
//#include <rt/renderer-threaded.hpp>

static void pause_and_exit(int val)
{
        getchar();
        exit(val);
}

#endif /* RT_HPP */
