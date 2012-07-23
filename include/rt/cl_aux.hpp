#pragma once
#ifndef RT_CL_AUX
#define RT_CL_AUX

#include <CL/opencl.h>
#include <rt/vector.hpp>
#include <rt/matrix.hpp>

#ifdef _WIN32
  #define RT_ALIGN(x) _declspec(align(x))
#else
  #define RT_ALIGN(x)
#endif

RT_ALIGN(16)
struct cl_sqmat4 {
        cl_float4 row[4];
};

cl_float3 vec3_to_float3(const vec3& v);
cl_float4 vec3_to_float4(const vec3& v, float w = 0);
cl_float4 vec4_to_float4(const vec4& v);

vec3 float3_to_vec3(const cl_float3& f);
vec4 float3_to_vec4(const cl_float3& f, float w = 0);
vec4 float4_to_vec4(const cl_float4& f);

cl_sqmat4 mat4x4_to_cl_sqmat4(const mat4x4& M);

cl_float2 makeFloat2(const float x, const float y);
cl_float3 makeFloat3(const float x, const float y, const float z);
cl_float4 makeFloat4(const float x, const float y, const float z, const float w);

cl_float2 makeFloat2(const float* v);
cl_float3 makeFloat3(const float* v);
cl_float4 makeFloat4(const float* v);

cl_float3 max(const cl_float3 a, const cl_float3 b);
cl_float3 min(const cl_float3 a, const cl_float3 b);

#endif /* RT_CL_AUX */
