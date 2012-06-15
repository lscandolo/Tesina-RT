#include <iostream> //!!
#include <algorithm>
#include <limits>

#include <rt/kdtree.hpp>

#define  PRINT(x)  std::cout << (#x) << " = " << (x) << std::endl

int max_leaf = 0;

int32_t 
KDTree::construct(Mesh& mesh, BBox* scene_bbox, int32_t node_offset, int32_t tri_offset) 
{

	/*------------------------ Initialize bboxes ------------------------------*/
	std::vector<BBox> bboxes;
        BBox root_bbox;
	bboxes.resize(mesh.triangleCount());
        std::vector<cl_uint> tris(mesh.triangleCount());
	for (uint32_t i = 0; i < bboxes.size(); ++i) {
		const Triangle& t = mesh.triangle(i);
		bboxes[i].set(mesh.vertex(t.v[0]),
			      mesh.vertex(t.v[1]),
			      mesh.vertex(t.v[2]));
                if (mesh.slacks.size() > i)
                        bboxes[i].add_slack(mesh.slacks[i]);
                root_bbox.merge(bboxes[i]);
                tris[i] = i;
	}

	/*------------------- Initialize members ----------------------------------*/
        // size_t tris = mesh.triangleCount();
	// if (tris == 0) { //!! Set this case later!
	// 	return true;
	// }


	/*------------------------ Initialize root and process it -----------------*/
	m_nodes.reserve(2*mesh.triangleCount());
	m_nodes.resize(1);
	KDTNode root;
        
        root.process(root_bbox, bboxes, m_nodes, tris, m_leaf_tris);
        m_nodes[0] = root;

	/*------------------ Reorder triangles in mesh now ----------------*/
	// mesh.reorderTriangles(m_triangle_order);

        PRINT(max_leaf);
        *scene_bbox = root_bbox;
	return 0;
}

int32_t 
KDTNode::process(const BBox bbox, 
                 const std::vector<BBox>& bboxes,
                 std::vector<KDTNode>& nodes, 
                 const std::vector<cl_uint>& tris,
                 std::vector<cl_uint>& leaf_tris)
{

        // std::cout << "Processing " << tris.size() << " tris. " << std::endl;

        size_t t_count = tris.size();

        //Create leaf if not enough primitives
        if (t_count <= KDTree::MAX_PRIMS_PER_NODE) {
                create_leaf(tris,leaf_tris);
                // std::cout << "Created " << tris.size() << " tris leaf." << std::endl;
                // if (tris.size() == 0u) exit(1);
                return 0;
        }
        
        size_t split_axis;
        float min_coord;
        float max_coord;

        //Create SAH planes
        std::vector<SplitPlane> planes(2*t_count);
        int32_t retries;

        SplitPlane bp;
        for (retries = 0; retries < 3; ++retries) {
                split_axis = (bbox.largestAxis()+retries)%3;
                min_coord = bbox.lo.s[split_axis];
                max_coord = bbox.hi.s[split_axis];

                for (size_t i = 0; i < t_count; ++i) {
                        uint32_t tri = tris[i];
                        float start_coord = bboxes[tri].lo.s[split_axis];
                        float end_coord = bboxes[tri].hi.s[split_axis];
                        planes[2*i].coord = clamp(start_coord, min_coord, max_coord);
                        planes[2*i].type = SplitPlane::start;
                        planes[2*i].tri = tri;
                        
                        planes[2*i+1].coord = clamp(end_coord, min_coord, max_coord);
                        planes[2*i+1].type = SplitPlane::end;
                        planes[2*i+1].tri = tri;
                }
                
                //Sort SAH planes
                std::sort(planes.begin(), planes.end());
                
                //Compute SAH costs
                size_t nl = 0,nr = t_count;
                
                float best_cost = t_count * isec_cost;
                size_t best_plane = planes.size()+1;
                for (size_t i = 0; i < planes.size() ; ++i) {
                        SplitPlane& sp = planes[i];
                        if (sp.type == SplitPlane::end)
                                nr--;
                        
                        //Choose split_coord
                        // std::cout << "SAHCost(bbox, " << (int)split_axis  
                        //           << "," << sp.coord << "," << nl  
                        //           <<  "," << nr << ")" << std::endl;
                        if (sp.coord > min_coord && sp.coord < max_coord) {
                                float cost = SAHCost(bbox, split_axis, sp.coord, nl, nr);
                                // std::cout << "cost: " << cost << std::endl;
                                if (cost < best_cost) {
                                        best_cost = cost;
                                        best_plane = i;
                                }
                        }
                        if (sp.type == SplitPlane::start)
                                nl++;
                }
                
                bp = planes[best_plane];
                if (best_plane < planes.size())
                        break;
 
        }
        if (retries > 2) {
                create_leaf(tris,leaf_tris);
                // std::cout << "Created " << tris.size() 
                //           << " tris leaf because of lack of good split." << std::endl;
                max_leaf = max_leaf > t_count? max_leaf : t_count;
                // std::cout << "No good split found..." << std::endl;
                // bp = planes[planes.size() / 2];
                return 0;
        }

        //Construct tris for left and right subtree
        std::vector<cl_uint> l_tris,r_tris;
        l_tris.reserve(tris.size());
        r_tris.reserve(tris.size());
        for (size_t i = 0; i < tris.size(); ++i) {
                uint32_t tri = tris[i];
                const BBox& b = bboxes[tri];
                // if (b.hi.s[split_axis] <= bp.coord*1.01f)
                //         l_tris.push_back(tri);
                // if (b.lo.s[split_axis] >= bp.coord*0.99f)
                //         r_tris.push_back(tri);
                
                if (b.lo.s[split_axis] == b.hi.s[split_axis]) {
                        if (b.lo.s[split_axis] < bp.coord)
                                l_tris.push_back(tri);
                        else
                                r_tris.push_back(tri);
                } else {
                        if (b.lo.s[split_axis] < bp.coord)
                                l_tris.push_back(tri);
                        if (b.hi.s[split_axis] > bp.coord)
                                r_tris.push_back(tri);
                }
        }

        // PRINT(tris.size());
        // PRINT(r_tris.size());
        // PRINT(l_tris.size());

        size_t r_size = r_tris.size();
        size_t l_size = l_tris.size();

        // size_t excess_tris = (r_size + l_size) - t_count;
        // if (excess_tris)
        //         std::cout << "\tExcess tris: " <<  excess_tris << std::endl;

        if ((r_size == t_count && bp.coord == min_coord) || 
            (l_size == t_count && bp.coord == max_coord)) {
                std::cout << "Error: created a subnode which is a copy of current node" 
                          << std::endl;
                exit(1);
        }


        //Recursive call
        BBox l_bbox = bbox, r_bbox = bbox;
        l_bbox.hi.s[split_axis] = bp.coord;
        r_bbox.lo.s[split_axis] = bp.coord;

        // if (l_size && r_size) {
                m_l_child = nodes.size();
                m_r_child = m_l_child + 1;
                nodes.resize(nodes.size() + 2);
        // }
        // else {
        //         m_l_child = m_r_child = 0;
        //         if (l_size)
        //                 m_l_child = nodes.size();
        //         else
        //                 m_r_child = nodes.size();
        //         nodes.resize(nodes.size() + 1);
        // }
        
        // if (l_size) {
                KDTNode l_node;
                // std::cout << "Going to left node" << std::endl;
                l_node.process(l_bbox, bboxes, nodes, l_tris, leaf_tris);
                nodes[m_l_child] = l_node;
        // } 

        // if (r_size) {
                KDTNode r_node;
                // std::cout << "Going to right node" << std::endl;
                r_node.process(r_bbox, bboxes, nodes, r_tris, leaf_tris);
                nodes[m_r_child] = r_node;
        // }

        m_split_axis = split_axis;
        m_split_coord = bp.coord;
        m_leaf = false;

        return 0;
}

void
KDTNode::create_leaf(const std::vector<cl_uint>& tris,
                     std::vector<cl_uint>& leaf_tris) { 
        m_leaf = true;
        m_tris_start = leaf_tris.size();
        for (size_t i = 0; i < tris.size(); ++i){
                leaf_tris.push_back(tris[i]);
        }
        m_tris_end = leaf_tris.size();
        return;
}

float 
KDTNode::SAHCost(const BBox& bbox, int32_t split_axis, float coord, size_t nl, size_t nr) {

        float bbox_surface = bbox.surfaceArea();
        float bbox_surface_inv = 1.f/bbox_surface;

        float left_rem = bbox.hi.s[split_axis] - coord;
        float right_rem = coord - bbox.lo.s[split_axis];

        float other_axis_len1 = bbox.hi.s[(split_axis+1) % 3] - 
                                bbox.lo.s[(split_axis+1) % 3];
        float other_axis_len2 = bbox.hi.s[(split_axis+2) % 3] - 
                                bbox.lo.s[(split_axis+2) % 3];

        float left_surface = bbox_surface - 
                (2 * left_rem * (other_axis_len1 + other_axis_len2));
        float right_surface = bbox_surface - 
                (2 * right_rem * (other_axis_len1 + other_axis_len2));

        float pl = left_surface  * bbox_surface_inv;
        float pr = right_surface * bbox_surface_inv;

        float empty_bonus = (nl == 0 || nr == 0) ? .717f : 1.f;

        float cost = trav_cost + empty_bonus * isec_cost * (pl * nl + pr * nr);

        return cost;
}

