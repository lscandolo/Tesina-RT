#include <iostream> //!!
#include <algorithm>
#include <limits>

#include <rt/bvh.hpp>

BBox::BBox()
{
	float M = std::numeric_limits<float>::max();
	float m = std::numeric_limits<float>::min();
	
	hi = makeFloat3(m,m,m);
	lo = makeFloat3(M,M,M);
}

BBox::BBox(const Vertex& a, const Vertex& b, const Vertex& c)
{
	hi = max(a.position, max(b.position,c.position));
	lo = min(a.position, min(b.position,c.position));
}

void 
BBox::set(const Vertex& a, const Vertex& b, const Vertex& c)
{
	hi = max(a.position, max(b.position,c.position));
	lo = min(a.position, min(b.position,c.position));
}

void 
BBox::merge(const BBox& b)
{
	hi = max(hi, b.hi);
	lo = min(lo, b.lo);
}

void 
BBox::add_slack(const vec3& sl)
{
	hi.s[0] += sl[0];
	hi.s[1] += sl[1];
	hi.s[2] += sl[2];

	lo.s[0] -= sl[0];
	lo.s[1] -= sl[1];
	lo.s[2] -= sl[2];
}

uint8_t 
BBox::largestAxis() const
{
	float dx = hi.s[0] - lo.s[0];
	float dy = hi.s[1] - lo.s[1];
	float dz = hi.s[2] - lo.s[2];
	// if (dx >= dy && dx >= dz)
        //         return dy > dz ? 2 : 1;
	// if (dy >= dx && dy >= dz)
        //         return dx > dz ? 2 : 0;
	// if (dz >= dx && dz >= dy)
        //         return dx > dy ? 1 : 0;
	if (dx > dy && dx > dz)
		return 0;
	else if (dy > dz)
		return 1;
	else
		return 2;
}

vec3 
BBox::centroid() const
{
	vec3 hi_vec = float3_to_vec3(hi);
	vec3 lo_vec = float3_to_vec3(lo);
	return (hi_vec+lo_vec) * 0.5f;
	// return vec3_to_float3((hi_vec+lo_vec) * 0.5f);
}

float 
BBox::surfaceArea() const
{
	vec3 hi_vec = float3_to_vec3(hi);
	vec3 lo_vec = float3_to_vec3(lo);
	vec3 diff = max(hi_vec - lo_vec, makeVector(0.f,0.f,0.f));
	return (diff[0] * (diff[1] + diff[2]) + diff[1] * diff[2]) * 2.f;
	
}

void 
BVHNode::sort(const std::vector<BBox>& bboxes,
	      std::vector<tri_id>& ordered_triangles,
	      std::vector<BVHNode>& nodes,
	      uint32_t node_index,
              uint32_t node_offset,
              uint32_t tri_offset) 
{

	/*------------------------ Compute bbox ----------------------*/
	m_bbox = computeBBox(bboxes, ordered_triangles);

	/* ----------------- Check if it's small enough ---------------*/
	if (m_end_index - m_start_index  <= BVH::MIN_PRIMS_PER_NODE) {
                offset_bounds(tri_offset);
		m_leaf = true;
		return;
	}
	/*------------------------ Choose split axis  ----------------------*/
	m_split_axis = m_bbox.largestAxis();
        uint32_t split_location;
        for (int8_t tries = 0; tries < 2; ++tries) {

                /*------------------------- Reorder triangles by axis ---------------*/
                reorderTriangles(bboxes, ordered_triangles);

                /*----------------------- Choose split location ---------------------*/
                split_location = chooseSplitLocationSAH(bboxes, ordered_triangles);

                if (split_location == m_start_index || split_location == m_end_index)
                        m_split_axis = (m_split_axis + 1)%3;
                else
                        break;
        }

	if (split_location == m_start_index || split_location == m_end_index) {
                offset_bounds(tri_offset);
		m_leaf = true;
		return;
	}


	/*------------------ Add left and right node -------------------*/
	m_leaf = false;

	BVHNode lnode,rnode;

	nodes.resize(nodes.size()+2);
        uint32_t l_child_index = uint32_t(nodes.size()-2);
        uint32_t r_child_index = uint32_t(nodes.size()-1);

        /* Offset is added in case the nodes are inserted in a big array of nodes 
           starting at node_offset */
	m_l_child = l_child_index + node_offset;
	m_r_child = r_child_index + node_offset;
	
	/*---------------------- Left node creation -------------------*/
	lnode.set_bounds(m_start_index, split_location);
	lnode.sort(bboxes, ordered_triangles, nodes, m_l_child, node_offset, tri_offset);
	lnode.m_parent = node_index;
	
	/*--------------------- Right node creation -------------------*/
	rnode.set_bounds(split_location, m_end_index);
	rnode.sort(bboxes, ordered_triangles, nodes, m_r_child, node_offset, tri_offset);
	rnode.m_parent = node_index;

        /*----------------- Save created nodex -------------------------*/
        offset_bounds(tri_offset);
	nodes[l_child_index] = lnode;
	nodes[r_child_index] = rnode;
	return;
}

bool 
BVH::construct(Mesh& mesh, int32_t node_offset, int32_t tri_offset) 
{

	/*------------------- Initialize members ----------------------------------*/
        size_t tris = mesh.triangleCount();
	m_triangle_order.resize(tris);
	for (uint32_t i = 0 ; i  < tris ; ++i)
		m_triangle_order[i] = i;

	if (tris == 0) {
		m_nodes.resize(1);
		BVHNode root;
                root.set_empty(node_offset);
		m_nodes[0] = root;
		return true;
	}

	/*------------------------ Initialize bboxes ------------------------------*/
	std::vector<BBox> bboxes;
	bboxes.resize(mesh.triangleCount());
	for (uint32_t i = 0; i < bboxes.size(); ++i) {
		const Triangle& t = mesh.triangle(i);
		bboxes[i].set(mesh.vertex(t.v[0]),
			      mesh.vertex(t.v[1]),
			      mesh.vertex(t.v[2]));
                if (mesh.slacks.size() > i)
                        bboxes[i].add_slack(mesh.slacks[i]);
	}

	/*------------------------ Initialize root and sort it ----------------------*/
	// m_nodes.reserve(2*mesh.triangleCount());
	m_nodes.resize(1);
	BVHNode root;
        root.set_parent(node_offset);
	root.set_bounds(0, uint32_t(mesh.triangleCount()));
	root.sort(bboxes, m_triangle_order, m_nodes, node_offset, 
                  node_offset, tri_offset);
	m_nodes[0] = root;


	/*------------------ Reorder triangles in mesh now ----------------*/
	mesh.reorderTriangles(m_triangle_order);

        start_node = node_offset;
	return true;
}


bool 
BVH::construct_and_map(Mesh& mesh, std::vector<cl_int>& map, 
                       int32_t node_offset, int32_t tri_offset)
{
	/*------------------- Initialize members ----------------------------------*/
	size_t tris = mesh.triangleCount();
	m_triangle_order.resize(tris);
	for (uint32_t i = 0 ; i  < tris ; ++i)
		m_triangle_order[i] = i;

	if (tris == 0) {
		m_nodes.resize(1);
		BVHNode root;
                root.set_empty(node_offset);
		m_nodes[0] = root;
		return true;
	}

	/*------------------------ Initialize bboxes ------------------------------*/
	std::vector<BBox> bboxes;
	bboxes.resize(mesh.triangleCount());
	for (uint32_t i = 0; i < bboxes.size(); ++i) {
		const Triangle& t = mesh.triangle(i);
		bboxes[i].set(mesh.vertex(t.v[0]),
			      mesh.vertex(t.v[1]),
			      mesh.vertex(t.v[2]));
                if (mesh.slacks.size() > i)
                        bboxes[i].add_slack(mesh.slacks[i]);
	}

	/*------------------------ Initialize root and sort it ----------------------*/
	m_nodes.resize(1);
	BVHNode root;
        root.set_parent(node_offset);
	root.set_bounds(0, uint32_t(mesh.triangleCount()));
	root.sort(bboxes, m_triangle_order, m_nodes, node_offset, 
                  node_offset, tri_offset);
	m_nodes[0] = root;

	/*------------------ Reorder triangles in mesh now ----------------*/
	mesh.reorderTriangles(m_triangle_order);

	/*------------------ Reorder map ----------------------------------*/
	std::vector<cl_int> new_map = map;
	for (uint32_t i = 0; i < m_triangle_order.size(); ++i) {
		map[i] = new_map[m_triangle_order[i]];
	}

        start_node = node_offset;
	return true;
}

BBox 
BVHNode::computeBBox(const std::vector<BBox>& bboxes, 
		     const std::vector<tri_id>& ordered_triangles) const
{
	uint32_t bbox_id = ordered_triangles[m_start_index];
	BBox b = bboxes[bbox_id];

	for (uint32_t i = m_start_index; i < m_end_index; ++i) {
		bbox_id = ordered_triangles[i];
		b.merge(bboxes[bbox_id]);
	}
	return b;
}

struct triangleOrder{
	triangleOrder(uint8_t order_axis, const std::vector<BBox>& _bboxes) : 
		bboxes(_bboxes){ax = order_axis;};
	bool operator()(tri_id id1, tri_id id2){
		return (bboxes[id1].centroid())[ax] < (bboxes[id2].centroid())[ax];
	};

	uint8_t ax;
	const std::vector<BBox>& bboxes;
};

void 
BVHNode::reorderTriangles(const std::vector<BBox>& bboxes,
			  std::vector<tri_id>& ordered_triangles) const
{
	triangleOrder tri_order(m_split_axis, bboxes);

	std::sort(ordered_triangles.begin() + m_start_index, 
		  ordered_triangles.begin() + m_end_index,
		  tri_order);
}

uint32_t 
BVHNode::chooseSplitLocation(const std::vector<BBox>& bboxes, 
			     const std::vector<tri_id>& ordered_triangles) const
{
	float center_val = m_bbox.centroid()[m_split_axis];
	uint32_t i = m_start_index;
	for (; i < m_end_index; ++i){
		tri_id t_id = ordered_triangles[i];
		if (bboxes[t_id].centroid()[m_split_axis] >= center_val)
			break;
	}
	return i;
}

uint32_t 
BVHNode::chooseSplitLocationSAH(const std::vector<BBox>& bboxes, 
				const std::vector<tri_id>& ordered_triangles) const
{
	const uint8_t& axis = m_split_axis;
	const uint32_t bucket_count = BVH::SAH_BUCKETS;

	// Allocate buckets
	SAHBucket buckets[bucket_count];
	
	vec3 hi_vec = float3_to_vec3(m_bbox.hi);
	vec3 lo_vec = float3_to_vec3(m_bbox.lo);
	float outer_bbox_length = hi_vec[axis] - lo_vec[axis];
	float outer_bbox_min   = lo_vec[axis]; 
	float outer_bbox_surface_area = m_bbox.surfaceArea();

	// Initialize buckets
	for (uint32_t i = m_start_index ; i < m_end_index ; ++i) {
		tri_id t = ordered_triangles[i];
		float  c = bboxes[t].centroid()[axis];
		uint32_t slot = uint32_t(bucket_count * ((c - outer_bbox_min) / outer_bbox_length));
		slot = std::min(slot, bucket_count-1);
		buckets[slot].prims++;
		buckets[slot].bbox.merge(bboxes[t]);
	}

	// Compute costs for splitting at buckets
	float cost[bucket_count-1];
	for (uint32_t i = 0; i < bucket_count-1; ++i) {
		BBox bl = buckets[0].bbox;
		BBox br = buckets[bucket_count-1].bbox;
		uint32_t countl = buckets[0].prims;
		uint32_t countr = buckets[bucket_count-1].prims;

		for (uint32_t j = 1; j <= i; ++j) {
			bl.merge(buckets[j].bbox);
			countl += buckets[j].prims;
		}
		for (uint32_t j = i+1; j < bucket_count; ++j) {
			br.merge(buckets[j].bbox);
			countr += buckets[j].prims;
		}
		cost[i] = .125f + 
			(countl * bl.surfaceArea() + countr*br.surfaceArea()) / 
			outer_bbox_surface_area;
	}
	
	
	// Find best bucket for split according to SAH
	float best_cost = cost[0];
	uint32_t best_split = 0;
	for (uint32_t i = 1; i < bucket_count-1; ++i) { 

		if (cost[i] < best_cost) {
			best_cost = cost[i];
			best_split = i;
		}
	}

	// Find split location
	float split_value = outer_bbox_min;
	split_value += (best_split+1) * (outer_bbox_length / ((float)bucket_count));
	uint32_t i = m_start_index;
	for (; i < m_end_index; ++i){
		tri_id t = ordered_triangles[i];
		if (bboxes[t].centroid()[m_split_axis] >= split_value)
			break;
	}

	return i;

}
