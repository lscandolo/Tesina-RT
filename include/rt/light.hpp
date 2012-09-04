#pragma once
#ifndef RT_LIGHT_HPP
#define RT_LIGHT_HPP

#include <rt/material.hpp>

//RT_ALIGN(16)
struct directional_light_cl {

	cl_float3 dir;
	color_cl  color;

	void set_dir(float x, float y, float z) 
		{
			dir.s[0] = x;
			dir.s[1] = y;
			dir.s[2] = z;
		}

	void set_color(float r, float g, float b) 
		{
			color[0] = r;
			color[1] = g;
			color[2] = b;
		}
};

//RT_ALIGN(16)
struct spot_light_cl {

	cl_float3 pos;
	cl_float  radius;
	cl_float  angle;
	cl_float3 dir;
	color_cl  color;

	void set_radius(float r)
                {
                        radius = r;
                }

	void set_angle(float a)
                {
                        angle = a;
                }

	void set_pos(float x, float y, float z) 
		{
			pos.s[0] = x;
			pos.s[1] = y;
			pos.s[2] = z;
		}

	void set_dir(float x, float y, float z) 
		{
			dir.s[0] = x;
			dir.s[1] = y;
			dir.s[2] = z;
		}

	void set_color(float r, float g, float b) 
		{
			color[0] = r;
			color[1] = g;
			color[2] = b;
		}
};

typedef enum {
        SPOT_L = 0,
        DIR_L = 1
} light_cl_type;

//RT_ALIGN(16)
struct light_cl {

        light_cl_type type;
        union {
                directional_light_cl directional;
                spot_light_cl spot;
        };

};

//RT_ALIGN(16)
struct lights_cl {

	color_cl ambient;
	light_cl light;
};

#endif /* RT_LIGHT_HPP */
