#ifndef RT_BVH_HPP
#define RT_BVH_HPP

#include <vector>
#include <stdint.h>

#include <rt/mesh.hpp>
#include <rt/math.hpp>
#include <rt/vector.hpp>
#include <rt/cl_aux.hpp>

typedef uint32_t index_t;

struct BBox {

	BBox();
	BBox(const Vertex& a, const Vertex& b, const Vertex& c);
	void set(const Vertex& a, const Vertex& b, const Vertex& c);
	void merge(const BBox& b);
	uint8_t largestAxis() const;
	vec3 centroid() const;
	float surfaceArea() const;

	cl_float3 hi,lo;
};

class BVHNode {

public:

	void setBounds(uint32_t s, uint32_t e){
		m_start_index = s;
		m_end_index = e;
		m_parent = 0;
	}

	void sort(const std::vector<BBox>& bboxes,
		  std::vector<tri_id>& ordered_triangles,
		  std::vector<BVHNode>& nodes,
		  uint32_t node_index);

private:

	BBox m_bbox;
	uint32_t m_l_child, m_r_child;
	uint32_t m_parent;
	uint32_t m_start_index, m_end_index;
	int8_t m_split_axis;
	int8_t m_leaf; /* Leaf is not bool because of the inability of OpenCL to 
			   handle byte aligned structs (at least my work implementation)*/

	BBox     computeBBox(const std::vector<BBox>& bboxes, 
			     const std::vector<tri_id>& ordered_triangles) const; 
	void     reorderTriangles(const std::vector<BBox>& bboxes,
				  std::vector<tri_id>& ordered_triangles) const;
	uint32_t chooseSplitLocation(const std::vector<BBox>& bboxes, 
				     const std::vector<tri_id>& ordered_triangles)const;
	uint32_t chooseSplitLocationSAH(const std::vector<BBox>& bboxes, 
					const std::vector<tri_id>& ordered_triangles)const;

};

class BVH {

public:

	bool construct(Mesh& m_mesh);
	bool construct_and_map(Mesh& m_mesh, std::vector<cl_int>& map);

	const BVHNode* nodeArray()
		{return &(m_nodes[0]);}

	uint32_t nodeArraySize()
		{return m_nodes.size();}

	const tri_id* orderedTrianglesArray()
		{return &(m_ordered_triangles[0]);}

	uint32_t orderedTrianglesArraySize()
		{return m_ordered_triangles.size();}

	static const uint32_t MIN_PRIMS_PER_NODE = 4;
	static const uint32_t SAH_BUCKETS = 32;

private:

	std::vector<BVHNode> m_nodes;
	std::vector<tri_id> m_ordered_triangles;
};


struct SAHBucket {

	SAHBucket() {prims = 0;} 
	uint32_t prims;
	BBox bbox;
};

#endif /* RT_BVH_HPP */
