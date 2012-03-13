#include <rt/material.hpp>

float 
color_cl::operator[](int32_t i) const
{
	switch (i) {
	case 0:
		return rgb.s[0];
	case 1:
		return rgb.s[1];
	case 2:
		return rgb.s[2];
	default:
		ASSERT(false);
        return 0.f;		
	}

}

float& 
color_cl::operator[](int32_t i)
{
	switch (i) {
	case 0:
		return rgb.s[0];
	case 1:
		return rgb.s[1];
	case 2:
		return rgb.s[2];
	default:
		ASSERT(false);
        return rgb.s[0];		
	}

}

