#pragma once
#ifndef MULTI_BVH_HPP
#define MULTI_BVH_HPP

#include <cl-gl/opencl-init.hpp>
#include <rt/math.hpp>
#include <rt/cl_aux.hpp>

RT_ALIGN(16)
struct BVHRoot {
        cl_int node;
        cl_sqmat4 tr;
        cl_sqmat4 trInv;
};

struct MultiBVH {
        cl_int cant;
        BVHRoot* roots;
};



#endif /*MULTI_BVH_HPP*/
