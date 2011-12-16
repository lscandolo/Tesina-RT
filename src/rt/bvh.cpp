#include <algorithm>

#include <rt/bvh.hpp>


#include <iostream> //!!


BBox::BBox()
{}

BBox::BBox(const Vertex& a, const Vertex& b, const Vertex& c)
{
	hi = makeVector(std::max(std::max(a.position[0],b.position[0]),c.position[0]),
			std::max(std::max(a.position[1],b.position[1]),c.position[1]),
			std::max(std::max(a.position[2],b.position[2]),c.position[2]));

	lo = makeVector(std::min(std::min(a.position[0],b.position[0]),c.position[0]),
			std::min(std::min(a.position[1],b.position[1]),c.position[1]),
			std::min(std::min(a.position[2],b.position[2]),c.position[2]));

	centroid = (hi+lo) * 0.5f;
}

void BBox::set(const Vertex& a, const Vertex& b, const Vertex& c)
{
	hi = makeVector(std::max(std::max(a.position[0],b.position[0]),c.position[0]),
			std::max(std::max(a.position[1],b.position[1]),c.position[1]),
			std::max(std::max(a.position[2],b.position[2]),c.position[2]));

	lo = makeVector(std::min(std::min(a.position[0],b.position[0]),c.position[0]),
			std::min(std::min(a.position[1],b.position[1]),c.position[1]),
			std::min(std::min(a.position[2],b.position[2]),c.position[2]));

	centroid = (hi+lo) * 0.5f;
}

void BBox::merge(const BBox& b)
{
	hi = max(hi, b.hi);
	lo = min(lo, b.lo);
	centroid = (hi+lo) * 0.5f;
}

uint8_t BBox::largestAxis() const
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

void BVHNode::sort(const std::vector<BBox>& bboxes,
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
	uint32_t split_location = chooseSplitLocation(bboxes, ordered_triangles);

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

bool BVH::construct() 
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

BBox BVHNode::computeBBox(const std::vector<BBox>& bboxes, 
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
		// std::cerr << "Comparing " << id1 << " & " << id2 << std::endl;
		return (bboxes[id1].centroid)[ax] < (bboxes[id2].centroid)[ax];
	};

	uint8_t ax;
	const std::vector<BBox>& bboxes;
};

void BVHNode::reorderTriangles(const std::vector<BBox>& bboxes,
			       std::vector<tri_id>& ordered_triangles) const
{
	triangleOrder tri_order(m_split_axis, bboxes);

	std::sort(ordered_triangles.begin() + m_start_index, 
		  ordered_triangles.begin() + m_end_index,
		  tri_order);
}

// For now this is a simple choose the middle one!
uint32_t BVHNode::chooseSplitLocation(const std::vector<BBox>& bboxes, 
				      const std::vector<tri_id>& ordered_triangles) const
{
	float center_val = m_bbox.centroid[m_split_axis];
	uint32_t i = m_start_index;

	for (; i < m_end_index; ++i){
		tri_id t_id = ordered_triangles[i];
		if (bboxes[t_id].centroid[m_split_axis] >= center_val)
			break;
	}
	return i;
}

