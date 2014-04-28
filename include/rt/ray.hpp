#pragma once
#ifndef RT_RAY_HPP
#define RT_RAY_HPP

#include <stdint.h>
#include <stdlib.h>
#include <limits>

#include <cl-gl/opencl-init.hpp>
#include <gpu/interface.hpp>
#include <rt/cl_aux.hpp>

#include <rt/vector.hpp>
#include <rt/math.hpp>


RT_ALIGN(16)
class ray_cl
{
public:

	cl_float3 ori; /* Ray origin */
	cl_float3 dir; /* Ray direction */
	cl_float3 invDir; /* Ray inverse direction */
	cl_float tMin;
	cl_float tMax;  /*Maximum and minimum valid ray intersection values*/

	/* Constructors */
        ray_cl(){}
	ray_cl( const vec3& origin, const vec3& direction);

	/* Setter */
	void set(const vec3& origin, const vec3& direction);

        /* Tests whether a value is within the ray's valid t interval */
        bool validT(float t) const; 
};

RT_ALIGN(16)
struct sample_cl
{
public:
	ray_cl     ray;
	cl_int     pixel;
	cl_float   contribution;
};

RT_ALIGN(16)
struct sample_trace_info_cl {


	cl_bool hit;
	cl_bool shadow_hit;
	cl_bool inverse_n;
	cl_bool reserved;
	cl_float t;
	cl_int id;
	cl_float2 uv;
	cl_float3 n;
        cl_float3 hit_point;
};

class RayBundle {

public:

	RayBundle();
	~RayBundle();

	int32_t initialize(const size_t rays); // Create mem object
	int32_t resize(const size_t rays);     // resize mem object
	bool    valid();                    // Check that it's correctly initialized 
	int32_t count();                // Return number of rays in the bundle
        DeviceMemory& mem();           // Return mem object

private:

        memory_id ray_id;

	int32_t m_ray_count;
	bool    m_initialized;

};

class HitBundle {

public:

	HitBundle();
	int32_t initialize(const size_t sz);   // create mem object
	int32_t resize(const size_t rays);     // resize mem object
	bool    valid();                    // Check that it's correctly initialized 
	int32_t count();                // Return number of rays in the bundle
	DeviceMemory& mem();

private:
	
        memory_id hit_id;

	size_t m_size;
	bool m_initialized;
};

#endif /* RT_RAY_HPP */










 
