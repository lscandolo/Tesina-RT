#include <CL/opencl.h>
#include <rt/vector.hpp>

cl_float3 vec3_to_float3(const vec3& v){
	cl_float3 f;
	f.x = v[0];
	f.y = v[1];
	f.z = v[2];
	return f;
}

cl_float4 vec4_to_float4(const vec4& v){
	cl_float4 f;
	f.x = v[0];
	f.y = v[1];
	f.z = v[2];
	f.w = v[3];
	return f;
}

cl_float4 vec3_to_float4(const vec3& v, float w = 0.f){
	cl_float4 f;
	f.x = v[0];
	f.y = v[1];
	f.z = v[2];
	f.w = w;
	return f;
}
