#ifndef RT_MATERIAL_HPP
#define RT_MATERIAL_HPP

#include <stdint.h>
#include <vector>

#include <rt/vector.hpp>
#include <rt/cl_aux.hpp>
#include <rt/assert.hpp>


/* RGB color will be represented as floats using  [0,1]  */
/* No constructor, and no privates, so it's POD */
struct Color {

	float operator[](int32_t i) const;
	float& operator[](int32_t i);

	cl_float3 rgb;

};

/* Helpful color definitions */

const Color Black = {{{0.f,0.f,0.f}}};
const Color Red   = {{{1.f,0.f,0.f}}};
const Color Green = {{{0.f,1.f,0.f}}};
const Color Blue  = {{{0.f,0.f,1.f}}};
const Color White = {{{1.f,1.f,1.f}}};

/* For now it'll be super simple */
struct Material {

	Color diffuse;
	float shininess;
	float reflectiveness;
	float refractive_index;

	Material(){
		diffuse = Black;
		shininess = 0.f;
		reflectiveness = 0.f;
		refractive_index = 0.f;
	}
}; 


struct MaterialList {

	struct ObjectMat{
		uint32_t max_id;
		Material mat;
	};

	std::vector<ObjectMat> mats; 

	uint32_t size_in_bytes(){
		return mats.size() * sizeof(ObjectMat);
	}

	const ObjectMat* arrayPointer(){
		return &(mats[0]);
	}
};


#endif /* RT_MATERIAL_HPP */
