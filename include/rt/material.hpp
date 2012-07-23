#pragma once
#ifndef RT_MATERIAL_HPP
#define RT_MATERIAL_HPP

#include <stdint.h>
#include <vector>

#include <rt/vector.hpp>
#include <rt/cl_aux.hpp>
#include <rt/assert.hpp>


/* RGB color will be represented as floats using  [0,1]  */
/* No constructor, and no privates, so it's POD */
RT_ALIGN(16)
struct color_cl {

	float operator[](int32_t i) const;
	float& operator[](int32_t i);

	cl_float3 rgb;

};

// /* Help color struct for synchronization purposes */
// RT_ALIGN
// struct color_int_cl {

// 	float operator[](int32_t i) const;
// 	float& operator[](int32_t i);

// 	cl_int rgb;
// }

/* Help color struct for synchronization purposes */
RT_ALIGN(4)
struct color_int_cl {

	float operator[](int32_t i) const;
	float& operator[](int32_t i);

	cl_uint r;
	cl_uint g;
	cl_uint b;

};

/* Helpful color definitions */

const color_cl Black = {{{0.f,0.f,0.f}}};
const color_cl Red   = {{{1.f,0.f,0.f}}};
const color_cl Green = {{{0.f,1.f,0.f}}};
const color_cl Blue  = {{{0.f,0.f,1.f}}};
const color_cl White = {{{1.f,1.f,1.f}}};

/* For now it'll be super simple */
RT_ALIGN(16)
struct material_cl {

	color_cl diffuse;
	cl_float shininess;
	cl_float reflectiveness;
	cl_float refractive_index;
        cl_int   texture;

	material_cl(){
		diffuse = Black;
		shininess = 0.f;
		reflectiveness = 0.f;
		refractive_index = 0.f;
                texture = -1;
	}
}; 


struct MaterialList {

	struct material_item_cl{
		cl_uint max_id;
		material_cl mat;
	};

	std::vector<material_item_cl> mats; 

	size_t size_in_bytes(){
		return mats.size() * sizeof(material_item_cl);
	}

	const material_item_cl* arrayPointer(){
		return &(mats[0]);
	}
};


#endif /* RT_MATERIAL_HPP */
