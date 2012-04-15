#ifndef RT_BVH_HPP
#define RT_BVH_HPP

#include <vector>
#include <stdint.h>

#include <rt/mesh.hpp>
#include <rt/math.hpp>
#include <rt/vector.hpp>
#include <rt/cl_aux.hpp>

struct BBox {

	BBox();
	BBox(const Vertex& a, const Vertex& b, const Vertex& c);
	void set(const Vertex& a, const Vertex& b, const Vertex& c);
	void merge(const BBox& b);
	void add_slack(const vec3& slack);
	uint8_t largestAxis() const;
	vec3 centroid() const;
	float surfaceArea() const;

	cl_float3 hi,lo;
};

RT_ALIGN(16)
class BVHNode {

public:

	void set_bounds(uint32_t s, uint32_t e){
		m_start_index = s;
		m_end_index = e;
	}

	void offset_bounds(uint32_t offset){
		m_start_index += offset;
		m_end_index += offset;
	}

	void sort(const std::vector<BBox>& bboxes,
                  std::vector<tri_id>& ordered_triangles,
                  std::vector<BVHNode>& nodes,
                  uint32_t parent_index,
                  uint32_t node_offset = 0,
                  uint32_t tri_offset = 0);

        void set_empty(int32_t node_offset = 0){
                m_leaf = true;
                m_start_index = 0;
                m_end_index = 0;
                m_parent = node_offset;
      }

        void set_parent(int32_t parent){
                m_parent = parent;
        }

private:

	BBox m_bbox;
	cl_uint m_l_child, m_r_child;
	cl_uint m_parent;
	cl_uint m_start_index, m_end_index;
	cl_char m_split_axis;
	cl_char m_leaf; /* Leaf is not bool because of the inability of OpenCL to 
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

	bool construct(Mesh& m_mesh, int32_t node_offset = 0, int32_t tri_offset = 0);
	bool construct_and_map(Mesh& m_mesh, std::vector<cl_int>& map, 
                               int32_t node_offset = 0, int32_t tri_offset = 0);

	BVHNode* nodeArray()
		{return &(m_nodes[0]);}

	size_t nodeArraySize()
		{return m_nodes.size();}

	const tri_id* triangle_order_array()
		{return &(m_triangle_order[0]);}

	size_t triangle_order_array_size()
		{return m_triangle_order.size();}

	static const uint32_t MIN_PRIMS_PER_NODE = 4;
	static const uint32_t SAH_BUCKETS = 32;

// private:

        uint32_t start_node;
        std::vector<BVHNode> m_nodes;
        std::vector<tri_id>  m_triangle_order;
};


struct SAHBucket {

	SAHBucket() {prims = 0;} 
	uint32_t prims;
	BBox bbox;
};

#endif /* RT_BVH_HPP */
