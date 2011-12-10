#include <algorithm>

#include <rt/vector.hpp>


vec2 makeVector(const float& v0, const float& v1)
{
	vec2 v;
	v[0] = v0; 
	v[1] = v1;
	return v;
}

vec3 makeVector(const float& v0, const float& v1, const float& v2)
{
	vec3 v;
	v[0] = v0; 
	v[1] = v1; 
	v[2] = v2;
	return v;
}

vec4 makeVector(const float& v0, const float& v1, const float& v2, const float& v3)
{
	vec4 v;
	v[0] = v0; 
	v[1] = v1; 
	v[2] = v2; 
	v[3] = v3;
	return v;
}


