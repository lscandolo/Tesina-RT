#include <rt/ray.hpp>


/* Value setter */
void Ray::set( const vec3& origin, const vec3& direction)
{
	ori = origin; dir = direction;
	invDir = inv(direction);
	tMin = 0.f; 
	tMax = std::numeric_limits<float>::max();
}

/// Tests whether a 'T-value' is within the ray's valid interval

bool Ray::validT(float t) const 
{ return ( t >= tMin && t < tMax ); };
 

 
