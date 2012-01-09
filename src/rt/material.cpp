#include <rt/material.hpp>

float 
color_cl::operator[](int32_t i) const
{
	switch (i) {
	case 0:
		return rgb.x;
	case 1:
		return rgb.y;
	case 2:
		return rgb.z;
	default:
		ASSERT(false);
	}

}

float& 
color_cl::operator[](int32_t i)
{
	switch (i) {
	case 0:
		return rgb.x;
	case 1:
		return rgb.y;
	case 2:
		return rgb.z;
	default:
		ASSERT(false);
	}

}

