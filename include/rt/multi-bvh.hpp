#ifndef MULTI_BVH_HPP
#define MULTI_BVH_HPP

#include <cl-gl/opencl-init.hpp>
#include <rt/math.hpp>
#include <rt/cl_aux.hpp>

struct BVHRoot {
        cl_int node;
        cl_sqmat4 tr;
};

struct MultiBVH {
        cl_int cant;
        BVHRoot* roots;
};



#endif /*MULTI_BVH_HPP*/
