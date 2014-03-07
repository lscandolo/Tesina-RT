#pragma once
#ifndef RT_BBOX_HPP
#define RT_BBOX_HPP

#include <stdint.h>
#include <limits>

#include <rt/mesh.hpp>
#include <rt/math.hpp>
#include <rt/vector.hpp>
#include <rt/cl_aux.hpp>


// RT_ALIGN(16)
struct BBox {

	BBox();
	BBox(const Vertex& a, const Vertex& b, const Vertex& c);
	void set(const Vertex& a, const Vertex& b, const Vertex& c);
	void merge(const BBox& b);
	void add_slack(const vec3& slack);
	uint8_t largestAxis() const;
	vec3 centroid() const;
	float surfaceArea() const;
    void reset();

        union {
                cl_float4 hi_4;
                cl_float3 hi;
        };        
        union {
                cl_float4 lo_4;
                cl_float3 lo;
        };
};


#endif /* RT_BBOX_HPP */
