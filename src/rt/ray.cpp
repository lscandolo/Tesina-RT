#include <rt/ray.hpp>


/* Constructor */
ray_cl::ray_cl( const vec3& origin, const vec3& direction) {
	ori = vec3_to_float3(origin);
	dir = vec3_to_float3(direction);
	invDir = vec3_to_float3(inv(direction));
	tMin = 0.f;
	tMax = std::numeric_limits<float>::max();
}


/* Value setter */
void ray_cl::set( const vec3& origin, const vec3& direction)
{
	ori = vec3_to_float3(origin);
	dir = vec3_to_float3(direction);
	invDir = vec3_to_float3(inv(direction));
	tMin = 0.f;
	tMax = std::numeric_limits<float>::max();
}

/// Tests whether a 'T-value' is within the ray's valid interval

bool ray_cl::validT(float t) const 
{ return ( t >= tMin && t < tMax ); };
 

 
