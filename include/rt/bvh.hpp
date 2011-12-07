#ifndef RT_BVH_HPP
#define RT_BVH_HPP

#include <vector>
#include <stdint.h>

#include <rt/mesh.hpp>
#include <rt/math.hpp>
#include <rt/vector.hpp>

typedef uint32_t index_t;

class BBox {};

class BVHNode {

	BBox bbox;
	std::vector<tri_id> tris;
	
};

class BVH {

	BVH(Mesh& m);
	bool construct();

	Mesh& mesh;
	std::vector<tri_id> ordered_triangles;
	
};

#endif /* RT_BVH_HPP */
