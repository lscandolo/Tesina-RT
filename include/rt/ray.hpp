#ifndef RT_RAY_HPP
#define RT_RAY_HPP

#include <stdint.h>
#include <stdlib.h>
#include <limits>
#include <new>

#include <rt/vector.hpp>
#include <rt/math.hpp>

class Ray
{
public:

	vec3 ori; /* Ray origin */
	vec3 dir; /* Ray direction */
	vec3 invDir; /* Ray inverse direction */
	float tMin;
	float tMax;  /*Maximum and minimum valid ray intersection values*/

	/* Constructors */
        inline Ray(){}
	inline Ray( const vec3& origin, const vec3& direction)
		: ori(origin), dir(direction), invDir(inv(direction)),
		  tMin(0.f), tMax(std::numeric_limits<float>::max()){}

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

	int size()
		{
			return ray_count;
		};

	int size_in_bytes()
		{
			return ray_count * sizeof(Ray);
		};

	Ray* ray_array()
		{
			return rays;
		}

private:

	int32_t ray_count;
	Ray* rays;

};

#endif /* RT_RAY_HPP */










 
