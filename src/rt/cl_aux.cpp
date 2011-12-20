#include <algorithm>

#include <rt/cl_aux.hpp>

cl_float3 vec3_to_float3(const vec3& v)
{
	cl_float3 f;
	f.x = v[0];
	f.y = v[1];
	f.z = v[2];
	return f;
}

cl_float4 vec4_to_float4(const vec4& v)
{
	cl_float4 f;
	f.x = v[0];
	f.y = v[1];
	f.z = v[2];
	f.w = v[3];
	return f;
}

cl_float4 vec3_to_float4(const vec3& v, float w)
{
	cl_float4 f;
	f.x = v[0];
	f.y = v[1];
	f.z = v[2];
	f.w = w;
	return f;
}


vec3 float3_to_vec3(const cl_float3& f)
{
	return makeVector(f.x,f.y,f.z);
}

vec4 float4_to_vec4(const cl_float4& f)
{
	return makeVector(f.x,f.y,f.z,f.w);
}

vec4 float3_to_vec4(const cl_float3& f, float w)
{
	return makeVector(f.x,f.y,f.z,w);
}

cl_float2 makeFloat2(const float x, const float y)
{
	cl_float2 f;
	f.x = x;
	f.y = y;
	return f;
}

cl_float3 makeFloat3(const float x, const float y, const float z)
{
	cl_float3 f;
	f.x = x;
	f.y = y;
	f.z = z;
	return f;
}

cl_float4 makeFloat4(const float x, const float y, const float z, const float w)
{
	cl_float4 f;
	f.x = x;
	f.y = y;
	f.z = z;
	f.w = w;
	return f;
}

cl_float2 makeFloat2(const float* v)
{
	return makeFloat2(v[0],v[1]);
}

cl_float3 makeFloat3(const float* v)
{
	return makeFloat3(v[0],v[1],v[2]);
}

cl_float4 makeFloat4(const float* v)
{
	return makeFloat4(v[0],v[1],v[2],v[3]);
}


cl_float3 max(const cl_float3 a, const cl_float3 b)
{
	cl_float3 r;
	r.x = std::max(a.x,b.x);
	r.y = std::max(a.y,b.y);
	r.z = std::max(a.z,b.z);
	return r;
}
cl_float3 min(const cl_float3 a, const cl_float3 b)
{
	cl_float3 r;
	r.x = std::min(a.x,b.x);
	r.y = std::min(a.y,b.y);
	r.z = std::min(a.z,b.z);
	return r;
}

