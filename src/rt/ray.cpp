#include <rt/ray.hpp>

// ray_cl methods

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


// RayBundle methods 

RayBundle::RayBundle()
{
	initialized = false;
	ray_count = 0;
}

RayBundle::~RayBundle()
{
	if (initialized) {
		clReleaseMemObject(ray_mem);
	}
}

bool 
RayBundle::initialize(const int32_t rays, const CLInfo& clinfo)
{
	if (rays <= 0)
		return false;

	if (create_empty_cl_mem(clinfo, 
				CL_MEM_READ_WRITE,
				rays * sizeof(ray_plus_cl),
				&ray_mem))
		return false;

	initialized = true;
	ray_count = rays;
	return true;
}

bool 
RayBundle::is_valid()
{
	return initialized;
}

int32_t 
RayBundle::count()
{
	return ray_count;
}

cl_mem&
RayBundle::mem()
{
	return ray_mem;
}

// HitBundle methods 

 
HitBundle::HitBundle()
{
	size = 0;
	initialized = false;
}

bool 
HitBundle::initialize(const int32_t sz, const CLInfo& clinfo)
{
	if (sz <= 0 || initialized == true)
		return false;

	if (create_empty_cl_mem(clinfo, 
				CL_MEM_READ_WRITE,
				sz * sizeof(ray_hit_info_cl),
				&hit_mem))
		return false;

	initialized = true;
	size = sz;
	return true;
}

cl_mem& 
HitBundle::mem()
{
	return hit_mem;
}
