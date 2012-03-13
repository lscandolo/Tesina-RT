#include <algorithm>

#include <rt/cl_aux.hpp>

cl_float3 vec3_to_float3(const vec3& v)
{
	cl_float3 f;
	f.s[0] = v[0];
	f.s[1] = v[1];
	f.s[2] = v[2];
	return f;
}

cl_float4 vec4_to_float4(const vec4& v)
{
	cl_float4 f;
	f.s[0] = v[0];
	f.s[1] = v[1];
	f.s[2] = v[2];
	f.s[3] = v[3];
	return f;
}

cl_float4 vec3_to_float4(const vec3& v, float w)
{
	cl_float4 f;
	f.s[0] = v[0];
	f.s[1] = v[1];
	f.s[2] = v[2];
	f.s[3] = w;
	return f;
}


vec3 float3_to_vec3(const cl_float3& f)
{
	return makeVector(f.s[0],f.s[1],f.s[2]);
}

vec4 float4_to_vec4(const cl_float4& f)
{
	return makeVector(f.s[0],f.s[1],f.s[2],f.s[3]);
}

vec4 float3_to_vec4(const cl_float3& f, float w)
{
	return makeVector(f.s[0],f.s[1],f.s[2],w);
}

cl_float2 makeFloat2(const float x, const float y)
{
	cl_float2 f;
	f.s[0] = x;
	f.s[1] = y;
	return f;
}

cl_float3 makeFloat3(const float x, const float y, const float z)
{
	cl_float3 f;
	f.s[0] = x;
	f.s[1] = y;
	f.s[2] = z;
	return f;
}

cl_float4 makeFloat4(const float x, const float y, const float z, const float w)
{
	cl_float4 f;
	f.s[0] = x;
	f.s[1] = y;
	f.s[2] = z;
	f.s[3] = w;
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
	r.s[0] = std::max(a.s[0],b.s[0]);
	r.s[1] = std::max(a.s[1],b.s[1]);
	r.s[2] = std::max(a.s[2],b.s[2]);
	return r;
}
cl_float3 min(const cl_float3 a, const cl_float3 b)
{
	cl_float3 r;
	r.s[0] = std::min(a.s[0],b.s[0]);
	r.s[1] = std::min(a.s[1],b.s[1]);
	r.s[2] = std::min(a.s[2],b.s[2]);
	return r;
}

