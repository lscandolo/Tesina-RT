#ifndef RT_RAY_HPP
#define RT_RAY_HPP

#include <stdint.h>
#include <stdlib.h>
#include <limits>
#include <new>

#include <rt/vector.hpp>
#include <rt/math.hpp>

#include <rt/cl_aux.hpp>

class Ray
{
public:

	cl_float3 ori; /* Ray origin */
	cl_float3 dir; /* Ray direction */
	cl_float3 invDir; /* Ray inverse direction */
	float tMin;
	float tMax;  /*Maximum and minimum valid ray intersection values*/

	/* Constructors */
        inline Ray(){}
	Ray( const vec3& origin, const vec3& direction);

	/* Setter */
	void set(const vec3& origin, const vec3& direction);

        /// Tests whether a value is within the ray's valid t interval
        bool validT(float t) const; 
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
				rays = new Ray[ray_count];
			}
			catch (std::bad_alloc ba)
			{ 
				return false;
			}
			return true;
		};

	Ray& operator[](int32_t i)
		{
			return rays[i];
		};

	int32_t size()
		{
			return ray_count;
		};

	int32_t size_in_bytes()
		{
			return ray_count * sizeof(Ray);
		};


	static int32_t expected_size_in_bytes(int32_t expected_ray_count)
		{
			return expected_ray_count * sizeof(Ray);
		}

	Ray* ray_array()
		{
			return rays;
		}

private:

	Ray* rays;
	int32_t ray_count;

};

class RayHitInfo {

	int hit;
	float t;
	int id;
	cl_float2 uv;

};

class RayReflectInfo {

	cl_float3 n;
	cl_float3 r;
	float  cosL;
	float  spec;

};

#endif /* RT_RAY_HPP */










 
