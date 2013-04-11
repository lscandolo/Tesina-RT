#pragma once
#ifndef RT_KDTREE_HPP
#define RT_KDTREE_HPP

#include <vector>
#include <stdint.h>

#include <rt/mesh.hpp>
#include <rt/math.hpp>
#include <rt/vector.hpp>
#include <rt/bbox.hpp>
#include <rt/cl_aux.hpp>

#define trav_cost .125f
#define isec_cost 1.f

class KDTNode {

public:

        cl_bool  m_leaf;
        cl_char  m_split_axis;
        cl_float m_split_coord;
        union {
                cl_uint  m_tris_start;
                cl_uint  m_l_child;
        };
        union {
                cl_uint  m_tris_end;
                cl_uint  m_r_child;
        };

        int32_t process(const BBox bbox, 
                        const std::vector<BBox>& bboxes,
                        std::vector<KDTNode>& nodes, 
                        const std::vector<cl_uint>& tris,
                        std::vector<cl_uint>& leaf_tris);

private:
        void create_leaf(const std::vector<cl_uint>& tris,
                         std::vector<cl_uint>& leaf_tris);
        float SAHCost(const BBox& bbox, int32_t split_axis, 
                      float coord, size_t nl, size_t nr);
        
};

class KDTree {

public:


	int32_t construct(Mesh& mesh, BBox* scene_bbox,
                          int32_t node_offset = 0, int32_t tri_offset = 0);
    void destroy();
    ~KDTree();


    static const size_t MAX_PRIMS_PER_NODE = 4;


    KDTNode* node_array()
        {return &(m_nodes[0]);}
    size_t node_array_size()
        {return m_nodes.size();}
    
    cl_uint* leaf_tris_array()
        {return &(m_leaf_tris[0]);}
    size_t   leaf_tris_array_size()
        {return m_leaf_tris.size();}

    std::vector<KDTNode> m_nodes;
    // std::vector<cl_uint> m_tris;
    std::vector<cl_uint> m_leaf_tris;
};

struct SplitPlane {

        enum {
                start,
                end
        } type;
        
        float coord;
        size_t tri;

        bool operator<(const SplitPlane& sp) const {
                if (coord == sp.coord) {
                        if (tri == sp.tri) {
                                return (int)type < (int)sp.type;
                        }
                        return tri < sp.tri;
                }
                return coord < sp.coord;            
        }

};


#endif /* RT_KDTREE_HPP */

