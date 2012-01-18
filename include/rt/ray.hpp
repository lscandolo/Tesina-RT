#ifndef RT_RAY_HPP
#define RT_RAY_HPP

#include <stdint.h>
#include <stdlib.h>
#include <limits>

#include <rt/vector.hpp>
#include <rt/math.hpp>

#include <rt/cl_aux.hpp>

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

struct ray_plus_cl
{
public:
	ray_cl ray;
	cl_int     pixel;
	cl_float   contribution;
};



class RayBundle {

public:
	RayBundle(int32_t rs)
		{
		ray_count = rs;
		rays = NULL;
		};

	~RayBundle()
		{
			if (rays!=NULL)
				delete[] rays;
		};

	bool initialize()
		{
			if (ray_count <= 0)
				return false;
			try 
			{
				rays = new ray_cl[ray_count];
			}
			catch (std::bad_alloc ba)
			{ 
				return false;
			}
			return true;
		};

	ray_cl& operator[](int32_t i)
		{
			return rays[i];
		};

	int32_t size()
		{
			return ray_count;
		};

	int32_t size_in_bytes()
		{
			return ray_count * sizeof(ray_cl);
		};


	static int32_t expected_size_in_bytes(int32_t expected_ray_count)
		{
			return expected_ray_count * sizeof(ray_cl);
		}

	ray_cl* ray_array()
		{
			return rays;
		}

private:

	ray_cl* rays;
	int32_t ray_count;

};

class RayHitInfo {

	cl_int flags;
	cl_float t;
	cl_int id;
	cl_float2 uv;
	cl_float3 n;

};

class RayReflectInfo {

	cl_float3 n;
	cl_float3 r;
	float  cosL;
	float  spec;

};

#endif /* RT_RAY_HPP */










 
