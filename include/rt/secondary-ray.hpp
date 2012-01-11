#ifndef SECONDARY_RAY_HPP
#define SECONDARY_RAY_HPP

#include <stdint.h>

#include <rt/cl_aux.hpp>
#include <rt/ray.hpp>
#include <rt/material.hpp>

// struct ray_level_cl {

// 	cl_int    hit;
// 	cl_int    material_id;
// 	cl_int    reflect_ray;
// 	cl_int    refract_ray;
// 	color_cl  color;
// };
struct ray_level_cl {

	cl_int    flags;
	cl_int    mat_id;
	color_cl  color1;
	color_cl  color2;
};

class screen_shading_info {

public:
	static size_t size_in_bytes(size_t screen_width, 
				    size_t screen_height, 
				    uint32_t max_bounce)
		{
			return (sizeof(ray_level_cl) * 
				max_bounce * screen_width * screen_height);
		}

};

struct bounce_cl
{
	cl_float3 hit_point;
	cl_float3 dir;
	cl_float3 normal;
	cl_int flags;
	cl_float refractive_index;
};

class ray_bounce_info {
public:
	static size_t size_in_bytes(size_t screen_width, 
				    size_t screen_height, 
				    uint32_t max_bounce)
		{
			return (sizeof(bounce_cl) *
				max_bounce * screen_width * screen_height);
		}


};


#endif /* SECONDARY_RAY_HPP */
