#include <algorithm>
#include <limits>

#include <rt/bvh.hpp>

BBox::BBox()
{
	float M = std::numeric_limits<float>::max();
	float m = std::numeric_limits<float>::min();
	
	hi = makeVector(m,m,m);
	lo = makeVector(M,M,M);
}

BBox::BBox(const Vertex& a, const Vertex& b, const Vertex& c)
{
	hi = makeVector(std::max(std::max(a.position[0],b.position[0]),c.position[0]),
			std::max(std::max(a.position[1],b.position[1]),c.position[1]),
			std::max(std::max(a.position[2],b.position[2]),c.position[2]));

	lo = makeVector(std::min(std::min(a.position[0],b.position[0]),c.position[0]),
			std::min(std::min(a.position[1],b.position[1]),c.position[1]),
			std::min(std::min(a.position[2],b.position[2]),c.position[2]));
}

void 
BBox::set(const Vertex& a, const Vertex& b, const Vertex& c)
{
	hi = makeVector(std::max(std::max(a.position[0],b.position[0]),c.position[0]),
			std::max(std::max(a.position[1],b.position[1]),c.position[1]),
			std::max(std::max(a.position[2],b.position[2]),c.position[2]));

	lo = makeVector(std::min(std::min(a.position[0],b.position[0]),c.position[0]),
			std::min(std::min(a.position[1],b.position[1]),c.position[1]),
			std::min(std::min(a.position[2],b.position[2]),c.position[2]));

}

void 
BBox::merge(const BBox& b)
{
	hi = max(hi, b.hi);
	lo = min(lo, b.lo);
}

uint8_t 
BBox::largestAxis() const
{
	float dx = hi[0] - lo[0];
	float dy = hi[1] - lo[1];
	float dz = hi[2] - lo[2];
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
	return (hi+lo) * 0.5f;
}

float 
BBox::surfaceArea() const
{
	vec3 diff = max(hi - lo, makeVector(0.f,0.f,0.f));
	return (diff[0] * (diff[1] + diff[2]) + diff[1] * diff[2]) * 2.f;
	
}

void 
BVHNode::sort(const std::vector<BBox>& bboxes,
	      std::vector<tri_id>& ordered_triangles,
	      std::vector<BVHNode>& nodes,
	      uint32_t node_index) 
{

	/*------------------------ Compute bbox ----------------------*/
	m_bbox = computeBBox(bboxes, ordered_triangles);

	/* ----------------- Check if it's small enough ---------------*/
	if (m_end_index - m_start_index  <= BVH::MIN_PRIMS_PER_NODE) {
		m_leaf = true;
		return;
	}


	/*------------------------ Choose split location ----------------------*/
	m_split_axis = m_bbox.largestAxis();

	/*------------------------- Reorder triangles by axis -------------------*/
	reorderTriangles(bboxes, ordered_triangles);

	/*----------------------- Choose split location -----------------------*/
	uint32_t split_location = chooseSplitLocationSAH(bboxes, ordered_triangles);

	if (split_location == m_start_index || split_location == m_end_index) {
		m_leaf = true;
		return;
	}

	/*------------------ Add left and right node -------------------*/
	m_leaf = false;

	BVHNode lnode,rnode;

	nodes.resize(nodes.size()+2);
	m_l_child = nodes.size()-2;
	m_r_child = nodes.size()-1;
	
	/*---------------------- Left node creation -------------------*/
	lnode.setBounds(m_start_index, split_location);
	lnode.sort(bboxes, ordered_triangles, nodes, m_l_child);
	lnode.m_parent = node_index;
	
	/*--------------------- Right node creation -------------------*/
	rnode.setBounds(split_location, m_end_index);
	rnode.sort(bboxes, ordered_triangles, nodes, m_r_child);
	rnode.m_parent = node_index;


	nodes[m_l_child] = lnode;
	nodes[m_r_child] = rnode;
	return;
}


BVH::BVH(const Mesh& m) : m_mesh(m) 
{
	uint32_t tris = m_mesh.triangleCount();
	m_ordered_triangles.resize(tris);
	for (uint32_t i = 0 ; i  < tris ; ++i)
		m_ordered_triangles[i] = i;
}

bool 
BVH::construct() 
{
	/*------------------------ Initialize bboxes ------------------------------*/
	std::vector<BBox> bboxes;
	bboxes.resize(m_mesh.triangleCount());
	for (uint32_t i = 0; i < bboxes.size(); ++i) {
		const Triangle& t = m_mesh.triangle(i);
		bboxes[i].set(m_mesh.vertex(t.v[0]),
			      m_mesh.vertex(t.v[1]),
			      m_mesh.vertex(t.v[2]));
	}

	/*------------------------ Initialize root and sort it ----------------------*/
	// m_nodes.reserve(2*m_mesh.triangleCount());
	m_nodes.resize(1);
	BVHNode root;
	root.setBounds(0, m_mesh.triangleCount());
	root.sort(bboxes, m_ordered_triangles, m_nodes, 0);
	m_nodes[0] = root;
	/*------------------ Split method is simple half & half now ----------------*/

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
	
	float outer_bbox_length = m_bbox.hi[axis] - m_bbox.lo[axis];
	float outer_bbox_min   = m_bbox.lo[axis]; 
	float outer_bbox_surface_area = m_bbox.surfaceArea();

	// Initialize buckets
	for (uint32_t i = m_start_index ; i < m_end_index ; ++i) {
		tri_id t = ordered_triangles[i];
		float  c = bboxes[t].centroid()[axis];
		uint32_t slot = bucket_count * ((c - outer_bbox_min) / outer_bbox_length);
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
