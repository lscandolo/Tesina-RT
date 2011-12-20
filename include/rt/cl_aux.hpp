#ifndef RT_CL_AUX
#define RT_CL_AUX

#include <CL/opencl.h>
#include <rt/vector.hpp>

cl_float3 vec3_to_float3(const vec3& v);

cl_float4 vec4_to_float4(const vec4& v);

cl_float4 vec3_to_float4(const vec3& v, float w = 0);

#endif /* RT_CL_AUX */
