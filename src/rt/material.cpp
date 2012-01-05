#include <rt/material.hpp>

float 
Color::operator[](int32_t i) const
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
Color::operator[](int32_t i)
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

