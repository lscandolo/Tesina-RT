#pragma once
#ifndef RT_LIGHT_HPP
#define RT_LIGHT_HPP

#include <rt/material.hpp>

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


struct lights_cl {

	color_cl ambient;
	directional_light_cl directional;
};

#endif /* RT_LIGHT_HPP */
