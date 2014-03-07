#pragma OPENCL EXTENSION cl_amd_printf:enable

#define UINT_MAX_INV 2.3283064370807974e-10f
#define UINT_MAX_F 4294967295.f

#define M_AXIS_BITS  10
#define M_MORTON_BITS 32
#define M_UINT_COUNT 1 

#define MORTON_MAX_F 1024.f // 2^M_AXIS_BITS
#define MORTON_MAX_INV_F 0.000976562.f //1/MORTON_MAX_F

#define BBOX_NOT_COMPUTED 1e35f
//#define BBOX_COMPUTED 1e34f
#define BBOX_COMPUTED 1.f

typedef struct 
{
        float3 position;
        float3 normal;
        float4 tangent;
        float3 bitangent;
        float2 texCoord;
} Vertex;

typedef struct {

        unsigned int code[M_UINT_COUNT];

} morton_code_t;

typedef unsigned int tri_id;

//typedef struct { //Temporary fix because of cl / cl size api inconsistency
//                float3 hi;
//                float3 lo;
//} BBox;

typedef struct { //Temporary fix because of cl / cl size api inconsistency
        union {
                float4 hi_4;
                float3 hi;
        };        
        union {
                float4 lo_4;
                float3 lo;
        };
} BBox;

typedef struct {

	BBox bbox;
        union {
                unsigned int l_child;
                unsigned int start_index;
        };
        union {
                unsigned int r_child;
                unsigned int end_index;
        };
	unsigned int parent;
	char split_axis;
	char leaf;

} BVHNode;


kernel void
rearrange_indices(global int*          index_buffer_in,
                  global unsigned int* triangles,
                  global int*          index_buffer_out) 
{
        size_t gid = get_global_id(0);
        unsigned int out_id = triangles[gid];

        index_buffer_out[3*gid]   = index_buffer_in[3*out_id];
        index_buffer_out[3*gid+1] = index_buffer_in[3*out_id+1];
        index_buffer_out[3*gid+2] = index_buffer_in[3*out_id+2];

}

kernel void
rearrange_map(global int*          map_buffer_in,
              global unsigned int* triangles,
              global int*          map_buffer_out) 
{
        size_t gid = get_global_id(0);
        unsigned int out_id = triangles[gid];

        map_buffer_out[gid] = map_buffer_in[out_id];
}


kernel void
rearrange_bboxes(global BBox*          bbox_buffer_in,
                 global unsigned int*  triangles,
                 global BBox*          bbox_buffer_out) 
{
        size_t gid = get_global_id(0);
        unsigned int out_id = triangles[gid];

        bbox_buffer_out[gid]   = bbox_buffer_in[out_id];

}

kernel void
rearrange_morton_codes(global morton_code_t* code_buffer_in,
                       global unsigned int*  triangles,
                       global morton_code_t* code_buffer_out) 
{
        size_t gid = get_global_id(0);
        unsigned int out_id = triangles[gid];

        code_buffer_out[gid]   = code_buffer_in[out_id];

}

/* As an aside, we initialize the triangles buffer here */
kernel void
build_primitive_bbox(global Vertex* vertex_buffer,
                     global int*    index_buffer,
                     global BBox*   bboxes,
                     global unsigned int* triangles) 
{

        int index = get_global_id(0);
        int id = 3 * index;

        global Vertex* vx0 = vertex_buffer + index_buffer[id];
        global Vertex* vx1 = vertex_buffer + index_buffer[id+1];
        global Vertex* vx2 = vertex_buffer + index_buffer[id+2];


        float3 v0 = vx0->position;
        float3 v1 = vx1->position;
        float3 v2 = vx2->position;

        BBox bbox;
        bbox.hi = bbox.lo = v0;

        bbox.hi = fmax(bbox.hi,v1);
        bbox.hi = fmax(bbox.hi,v2);

        bbox.lo = fmin(bbox.lo,v1);
        bbox.lo = fmin(bbox.lo,v2);

        bboxes[index] = bbox;
        triangles[index] = index;
}

kernel void
morton_encode(global BBox* bboxes,
              global BBox* global_bbox,
              global morton_code_t* morton_codes) 
{
        int index = get_global_id(0);
        BBox bbox = bboxes[index];
        float3 center = (bbox.hi + bbox.lo) * 0.5f;

        float3 g_lo = global_bbox->lo;
        float3 g_hi = global_bbox->hi;

        float3 inv_global_bbox_size = 1.f/(g_hi-g_lo);
        float3 slice  = (center - g_lo) * inv_global_bbox_size.xyz;
        const float3 zeros = (float3)(0.f,0.f,0.f);
        const float3 ones = (float3)(1.f,1.f,1.f);
        slice = fmax(zeros,fmin(ones,slice));
        ///// Slice is the value between 0 and 1 in the global bbox where this tri stands

        slice *= MORTON_MAX_F;

        /*Scramble codes*/
        
        //// Buffer output per each axis
        unsigned int morton_in[3];
        morton_in[0] = slice.x;
        morton_in[1] = slice.y;
        morton_in[2] = slice.z;

        //// Set morton code to 0
        morton_code_t morton;
        for (int i = 0; i < M_UINT_COUNT; ++i){
                morton.code[i] = 0;
        }

        for (int i = 0; i < M_AXIS_BITS*3; ++i) {
                int src       = 2-(i%3);
                int src_bit   = i/3;

                int dest      = M_UINT_COUNT-1-(i/(sizeof(uint)*8));
                int dest_bit  = i%(sizeof(uint)*8);

                morton.code[dest] |= ((morton_in[src]>>src_bit) & 1)<<dest_bit;
        }

        morton_codes[index] = morton;

}

kernel void
morton_encode_32(global BBox* bboxes,
                 global BBox* global_bbox,
                 global morton_code_t* morton_codes) 
{
        int index = get_global_id(0);
        BBox bbox = bboxes[index];
        float3 center = (bbox.hi + bbox.lo) * 0.5f;

        float3 g_lo = global_bbox->lo;
        float3 g_hi = global_bbox->hi;

        float3 inv_global_bbox_size = 1.f/(g_hi-g_lo);
        float3 slice  = (center - g_lo) * inv_global_bbox_size.xyz;
        const float3 zeros = (float3)(0.f,0.f,0.f);
        const float3 ones = (float3)(1.f,1.f,1.f);
        slice = fmax(zeros,fmin(ones,slice));
        ///// Slice is the value between 0 and 1 in the global bbox where this tri stands

        slice *= MORTON_MAX_F;

        /*Scramble codes*/
        
        //// Buffer output per each axis
        unsigned int morton_in[3];
        morton_in[0] = slice.x;
        morton_in[1] = slice.y;
        morton_in[2] = slice.z;

        //// Set morton code to 0
        morton_code_t morton;
        for (int i = 0; i < M_UINT_COUNT; ++i){
                morton.code[i] = 0;
        }

        for (int i = 0; i < M_AXIS_BITS*3 + 1; ++i) {
                int src       = 2-(i%3);
                int src_bit   = i/3;

                int dest      = M_UINT_COUNT-1-(i/(sizeof(uint)*8));
                int dest_bit  = i%(sizeof(uint)*8);

                if (dest_bit < 32)
                        morton.code[dest] |= ((morton_in[src]>>src_bit) & 1)<<dest_bit;
        }

        morton_codes[index] = morton;
}

// kernel void
// morton_encode(global BBox* bboxes,
//               global BBox* global_bbox,
//               global morton_code_t* morton_codes) 
// {
//         int index = get_global_id(0);
//         BBox bbox = bboxes[index];
//         float3 center = (bbox.hi + bbox.lo) * 0.5f;

//         float3 g_lo = global_bbox->lo;
//         float3 g_hi = global_bbox->hi;

//         float3 inv_global_bbox_size = 1.f/(g_hi-g_lo);
//         float3 slice  = (center - g_lo) * inv_global_bbox_size.xyz;
//         const float3 zeros = (float3)(0.f,0.f,0.f);
//         const float3 ones = (float3)(1.f,1.f,1.f);
//         slice = fmax(zeros,fmin(ones,slice));

//         slice *= MORTON_MAX_F;

//         /*Scramble codes*/
        
//         unsigned int morton_in[3];
//         morton_in[0] = slice.x;
//         morton_in[1] = slice.y;
//         morton_in[2] = slice.z;

//         morton_code_t morton;
//         for (int i = 0; i < M_UINT_COUNT; ++i){
//                 morton.code[i] = 0;
//         }

//         for (int i = 0; i < M_AXIS_BITS*3; ++i) {
//                 int src       = 2-(i%3);
//                 int src_bit   = i/3;

//                 int dest      = M_UINT_COUNT-1-(i/(sizeof(uint)*8));
//                 int dest_bit  = i%(sizeof(uint)*8);

//                 morton.code[dest] |= ((morton_in[src]>>src_bit) & 1)<<dest_bit;
//         }

//         morton_codes[index] = morton;

// }

kernel void
build_splits(global morton_code_t*   morton_codes,
             global unsigned int*    splits,
             global unsigned int*    triangles,
             local  morton_code_t*   local_codes)
{
        size_t gid = get_global_id(0);
        size_t lid = get_local_id(0);
        size_t lsz = get_local_size(0);

        local_codes[lid] = morton_codes[triangles[gid]];
        if (lid == 0)
                local_codes[lsz] = morton_codes[triangles[gid+lsz]];

        barrier(CLK_LOCAL_MEM_FENCE);
        splits[gid] = local_codes[lid].code[0] ^ local_codes[lid+1].code[0];
}

kernel void
init_treelet_maps(global int*    node_map,
                  global int*    segment_map)
{
        size_t gid = get_global_id(0);

        if (gid == 0) {
                node_map[0]     = 0;
                segment_map[0]  = 0;
        } else {
                node_map[gid]     = -1;
                segment_map[gid]  = 0;
        }

}

kernel void
init_segment_map(global int*    node_map,
                 global int*    segment_map)
{
        size_t gid = get_global_id(0);
        
        /* We set the previous element because we will do a scan afterwards */
        segment_map[gid] = node_map[gid+1] >= 0 ? 1 : 0;
}

kernel void
init_segment_heads(global int*    segment_map,
                   global int*    segment_heads,
                   unsigned int   segment_count,
                   unsigned int   triangle_count)
{
        size_t gid = get_global_id(0);
        
        if (gid == 0) {
                segment_heads[0] = 0;
                segment_heads[segment_count] = triangle_count;
        } else {
                if (segment_map[gid] != segment_map[gid-1])
                        segment_heads[segment_map[gid]] = gid;
        }
}

#define TREELET_LEVELS 3
#define TREELET_SIZE 7 //(1<<TREELET_LEVELS)-1

/* #define T_ROOT 0 */
/* #define T_L    1 */
/* #define T_R    2 */
/* #define T_LL   3 */
/* #define T_LR   4 */
/* #define T_RL   5 */
/* #define T_RR   6 */

#define T_ROOT 1
#define T_L    2
#define T_R    3
#define T_LL   4
#define T_LR   5
#define T_RL   6
#define T_RR   7

#define T_ALL_0 -1
#define T_ALL_1 -2
#define T_NULL  -3


kernel void
init_treelet(global int*    treelets)
{
        size_t gid = get_global_id(0);
        treelets[gid] = T_NULL;
        //treelets[gid] = T_ALL_0;
}

kernel void
build_treelet(global int*            node_map,
              global int*            segment_map,
              global int*            segment_heads,
              unsigned int           bitmask,
              unsigned int           triangle_count,
              unsigned int           pass,
              global unsigned int*   triangles,
              global morton_code_t*  morton_codes,
              global morton_code_t*  splits,
              global int*            treelets)
{
        const size_t gid = get_global_id(0);
        const size_t lid = get_local_id(0);
        const size_t lsz = get_local_size(0);
        morton_code_t split = splits[gid];

        int current_node = T_ROOT;
        int segment_id = segment_map[gid];
        int node_start = segment_heads[segment_id];
        int node_end   = segment_heads[segment_id + 1];

        treelets += segment_id * TREELET_SIZE;
        global int* treelet  = treelets-1;

#define parent(N) ((N)>>1)
#define l_child(N) ((N)<<1)
#define r_child(N) (((N)<<1)+1)

        ///////// Descend on the treelet to the correct node
        for (int lev = 0; lev < pass; ++lev) {
                //if (treelet[current_node] == T_NULL) 
                //        break; 

                if (treelet[current_node] == T_ALL_0){
                        current_node = l_child(current_node);
                } else if (treelet[current_node] == T_ALL_1){
                        current_node = r_child(current_node);
                } else if (gid < treelet[current_node]) {
                        node_end = treelet[current_node];
                        current_node = l_child(current_node);
                } else { /* gid >= treelet[current_node] */
                        node_start = treelet[current_node];
                        current_node = r_child(current_node);
                }
        }

#undef parent
#undef l_child
#undef r_child

        ///////// If bit changes is in this node, note it
        bool change = split.code[0] & bitmask;
        if (change && gid < node_end-1) {
                treelet[current_node] = gid+1;
        }
        ///////// If the first and last codes in this nodes are the same, there
        ///////// is no change, so mark it
        if  (gid == node_start) {
                unsigned int first_code = morton_codes[triangles[node_start]].code[0];
                unsigned int last_code = morton_codes[triangles[node_end-1]].code[0];
                if (! ((first_code^last_code) & bitmask)) {
                        if (first_code & bitmask)
                                treelet[current_node] = T_ALL_1;
                        else
                                treelet[current_node] = T_ALL_0;
                }
        }
}

void kernel count_nodes(global int* treelets,
                        global unsigned int* node_offsets)
{
        size_t gid = get_global_id(0);
        
        /////// Go through the treelet and
        /////// save the amount of nodes to produce (2*splits)
        treelets += gid*TREELET_SIZE;
        int node_count = 0;
        for (int i = 0; i < TREELET_SIZE; ++i) {
                node_count += treelets[i] >= 0? 2 : 0;
        }
        node_offsets[gid] = node_count;
}

void link_children(global BVHNode* nodes, 
                   unsigned int    parent_id,
                   unsigned int    lchild_id,
                   unsigned int    rchild_id)
{
        global BVHNode* parent = nodes+parent_id;
        global BVHNode* lchild = nodes+lchild_id;
        global BVHNode* rchild = nodes+rchild_id;

        parent->l_child = lchild_id;
        parent->r_child = rchild_id;
        parent->leaf = false;
        lchild->parent = parent_id;
        rchild->parent = parent_id;

        lchild->split_axis = (parent->split_axis + 1)%3;
        rchild->split_axis = lchild->split_axis;

}

unsigned int process_node_split_2(global BVHNode* nodes,
                                  unsigned int    parent_id,
                                  unsigned int    children_offset,
                                  global   int*   treelets,
                                  int             treelet_entry,
                                  int             first_primitive,
                                  int             end_primitive,
                                  global int*     node_map,
                                  int last_pass)
{
        int split = treelets[treelet_entry];

        if (!last_pass) {

                if (split >= 0) {
                        unsigned int lnode_id = children_offset;
                        unsigned int rnode_id = lnode_id + 1;
                        link_children(nodes, parent_id, lnode_id, rnode_id);

                        //save node start location 
                        node_map[first_primitive] = lnode_id;
                        node_map[split] =   rnode_id;

                        return 2;
                } else if (split == T_ALL_0) {
                        //save node start location
                        node_map[first_primitive] = parent_id;
                        nodes[parent_id].leaf = true;
                        nodes[parent_id].start_index = first_primitive;
                        nodes[parent_id].end_index = end_primitive;
                        return 0;
                } else { //split == T_ALL_1
                        //save node start location
                        node_map[first_primitive] = parent_id;
                        nodes[parent_id].leaf = true;
                        nodes[parent_id].start_index = first_primitive;
                        nodes[parent_id].end_index = end_primitive;
                        return 0;
                }

        } else {
                size_t gid = get_global_id(0);
                unsigned int start_index = first_primitive;
                unsigned int end_index = end_primitive;

                if (split >= 0) {
                        unsigned int lnode_id = children_offset;
                        unsigned int rnode_id = lnode_id + 1;
                        link_children(nodes, parent_id, lnode_id, rnode_id);

                        nodes[lnode_id].leaf = true;
                        nodes[lnode_id].start_index = start_index;
                        nodes[lnode_id].end_index = split;

                        nodes[rnode_id].leaf = true;
                        nodes[rnode_id].start_index = split;
                        nodes[rnode_id].end_index = end_index;

                        return 2;
                } else if (split == T_ALL_0) {
                        nodes[parent_id].leaf = true;
                        nodes[parent_id].start_index = start_index;
                        nodes[parent_id].end_index = end_index;
                        return 0;
                } else { //split == T_ALL_1
                        nodes[parent_id].leaf = true;
                        nodes[parent_id].start_index = start_index;
                        nodes[parent_id].end_index = end_index;
                        return 0;
                }

        }
}

unsigned int process_node_split_1(global BVHNode* nodes,
                                  unsigned int    parent_id,
                                  unsigned int    children_offset,
                                  global   int*   treelets,
                                  int             treelet_entry,
                                  int             first_primitive,
                                  int             end_primitive,
                                  global int*     node_map,
                                  int last_pass)
{
        int split = treelets[treelet_entry];
        int lentry = treelet_entry*2+1;
        int rentry = treelet_entry*2+2;

        if (split >= 0) {
                unsigned int lnode_id = children_offset;
                unsigned int rnode_id = lnode_id + 1;
                link_children(nodes, parent_id, lnode_id, rnode_id);

                unsigned int new_nodes = 2;
                new_nodes += process_node_split_2(nodes,lnode_id,
                                                  children_offset+new_nodes,
                                                  treelets,lentry,
                                                  first_primitive,
                                                  split,
                                                  node_map,
                                                  last_pass);

                new_nodes += process_node_split_2(nodes,rnode_id,
                                                  children_offset+new_nodes,
                                                  treelets,rentry,
                                                  split,
                                                  end_primitive,
                                                  node_map,
                                                  last_pass);
                return new_nodes;
        } else if (split == T_ALL_0) {
                unsigned int new_nodes = 0;
                new_nodes += process_node_split_2(nodes,parent_id,
                                                  children_offset,
                                                  treelets,lentry,
                                                  first_primitive,
                                                  end_primitive,
                                                  node_map,
                                                  last_pass);
                return new_nodes;
        } else { //split == T_ALL_1
                unsigned int new_nodes = 0;
                new_nodes += process_node_split_2(nodes,parent_id,
                                                  children_offset,
                                                  treelets,rentry,
                                                  first_primitive,
                                                  end_primitive,
                                                  node_map,
                                                  last_pass);
                return new_nodes;
        }
}

unsigned int process_node_split_0(global BVHNode* nodes,
                                  unsigned int    parent_id,
                                  unsigned int    children_offset,
                                  global   int*   treelets,
                                  unsigned int    first_primitive,
                                  unsigned int    end_primitive,
                                  global int*     node_map,
                                  int last_pass)
{ 
        int split = treelets[0];

        if (split >= 0) {
                unsigned int lnode_id = children_offset;
                unsigned int rnode_id = lnode_id + 1;
                link_children(nodes, parent_id, lnode_id, rnode_id);

                unsigned int new_nodes = 2;
                new_nodes += process_node_split_1(nodes,lnode_id,
                                                  children_offset+new_nodes,
                                                  treelets,1,
                                                  first_primitive,
                                                  split,
                                                  node_map,
                                                  last_pass);

                new_nodes += process_node_split_1(nodes,rnode_id,
                                                  children_offset+new_nodes,
                                                  treelets,2,
                                                  split,
                                                  end_primitive,
                                                  node_map,
                                                  last_pass);
                return new_nodes;
        } else if (split == T_ALL_0) {
                unsigned int new_nodes = 0;
                new_nodes += process_node_split_1(nodes,parent_id,
                                                  children_offset,
                                                  treelets,1,
                                                  first_primitive,
                                                  end_primitive,
                                                  node_map,
                                                  last_pass);
                return new_nodes;
        } else { //split == T_ALL_1
                unsigned int new_nodes = 0;
                new_nodes += process_node_split_1(nodes,parent_id,
                                                  children_offset,
                                                  treelets,2,
                                                  first_primitive,
                                                  end_primitive,
                                                  node_map,
                                                  last_pass);
                return new_nodes;
        }
}

kernel void
build_nodes(global int*            node_map,
            global int*            segment_heads,
            unsigned int           triangle_count,
            global int*            treelets,
            unsigned int           global_node_offset,
            global unsigned int*   node_offsets,
            global BVHNode*        nodes,
            int                    last_pass)
{

        size_t gid = get_global_id(0);

        treelets+= TREELET_SIZE*gid;

        size_t parent_id = node_map[segment_heads[gid]];

        unsigned int start_offset = global_node_offset + node_offsets[gid];

        int first_primitive = segment_heads[gid];
        int end_primitive = segment_heads[gid+1];

        ///// Create new nodes and save in new node map
        int new_nodes = process_node_split_0(nodes,
                                             parent_id,
                                             start_offset,
                                             treelets,
                                             first_primitive,
                                             end_primitive,
                                             node_map,
                                             last_pass);

}

void merge_bbox(BBox* bbox, BBox bbox2) 
{
        //bbox->lo = fmin(bbox->lo, bbox2.lo);
        //bbox->hi = fmax(bbox->hi, bbox2.hi);
        bbox->lo_4 = fmin(bbox->lo_4, bbox2.lo_4);
        bbox->hi_4 = fmax(bbox->hi_4, bbox2.hi_4);
}

kernel void
build_leaf_bbox(global unsigned int* triangles,
                global BBox*   bboxes,
                global BVHNode* nodes)
{
        size_t gid = get_global_id(0);

        global BVHNode* node = nodes+gid;
        if (node->leaf) {
                unsigned int start_index = node->start_index;
                unsigned int end_index = node->end_index;
                BBox bbox = bboxes[triangles[start_index]];
                for (int i = start_index+1; i < end_index; ++i) {
                        merge_bbox(&bbox, bboxes[triangles[i]]);
                }
                ////////////
                //global BVHNode* parent = nodes + (node->parent);
                //if (parent->l_child == gid)
                //        parent->bbox.hi_4.x = BBOX_COMPUTED;
                //if (parent->r_child == gid)
                //        parent->bbox.hi_4.y = BBOX_COMPUTED;
                ////////////


                node->bbox = bbox;
        }
}

kernel void
build_node_bbox(global BVHNode* nodes)
{

        size_t gid = get_global_id(0);
        global BVHNode* node = nodes+gid;

#define lchild(n) nodes[n->l_child]
#define rchild(n) nodes[n->r_child]
#define parent(n) nodes[n->parent]

        if (node->leaf)
                return;

        BBox bbox = lchild(node).bbox;
        merge_bbox(&bbox, rchild(node).bbox);
        node->bbox = bbox;
        //////////////////

        //if (node->leaf ||
        //    node->bbox.hi_4.x != BBOX_COMPUTED ||
        //    node->bbox.hi_4.y != BBOX_COMPUTED)
        //        return ;

        //BBox bbox = lchild(node).bbox;
        //merge_bbox(&bbox, rchild(node).bbox);
        //node->bbox = bbox;
        //mem_fence(CLK_GLOBAL_MEM_FENCE | CLK_LOCAL_MEM_FENCE);

        //if (gid == 0)
        //        return;

        //global BVHNode* parent = nodes + (node->parent);
        //if (parent->l_child == gid)
        //        parent->bbox.hi_4.x = BBOX_COMPUTED;
        //if (parent->r_child == gid)
        //        parent->bbox.hi_4.y = BBOX_COMPUTED;


#undef lchild
#undef rchild

}


/* #define NUM_BANKS 16   */
/* #define LOG_NUM_BANKS 4   */
/* /\* #define CONFLICT_FREE_OFFSET(n) (((n) >> LOG_NUM_BANKS) + ((n) >> (2 * LOG_NUM_BANKS))) *\/ */
/* #define CONFLICT_FREE_OFFSET(n) (0) */

kernel void 
max_local_bbox(global BBox*   in_bboxes,
               local  BBox*   aux,
               unsigned int   size,
               global BBox*   out_bboxes)
{
        size_t global_size = get_global_size(0);
        size_t local_size = get_local_size(0);
        size_t group_id   = get_group_id(0);
        size_t g_idx = get_global_id(0);
        size_t l_idx = get_local_id(0);

        int in_start_idx = group_id * local_size * 2;
        in_bboxes  += in_start_idx;

        // Have to check range
        if (group_id == get_num_groups(0) - 1) {

                BBox empty;
                empty.hi_4 = (float4)(-1e37f,-1e37f,-1e37f,-1e37f);
                empty.lo_4 = (float4)(1e37f,1e37f,1e37f,1e37f);

                size_t a = l_idx;
                size_t b = l_idx + local_size;

                if (in_start_idx + a < size)
                        aux[a] = in_bboxes[a];
                else
                        aux[a] = empty;

                if (in_start_idx + b < size)
                        aux[b] = in_bboxes[b];
                else
                        aux[b] = empty;

                for (size_t d = local_size; d > 0; d >>= 1) {
                        barrier(CLK_LOCAL_MEM_FENCE|CLK_GLOBAL_MEM_FENCE);

                        if (d > a) {
                                aux[a].lo_4 = fmin(aux[a].lo_4, aux[a+d].lo_4);
                                aux[a].hi_4 = fmax(aux[a].hi_4, aux[a+d].hi_4);
                        }
                }
                barrier(CLK_LOCAL_MEM_FENCE|CLK_GLOBAL_MEM_FENCE);
                /* barrier(CLK_LOCAL_MEM_FENCE); */

                if (l_idx == 0) {
                        out_bboxes[group_id] = aux[0];
                }
        
                // Don't have to check range
        } else {

                size_t a = l_idx;
                size_t b = l_idx + local_size;

                aux[a] = in_bboxes[a];
                aux[b] = in_bboxes[b];

                for (size_t d = local_size; d > 0; d >>= 1) {
                        barrier(CLK_LOCAL_MEM_FENCE);

                        if (d > a) {
                                aux[a].lo_4 = fmin(aux[a].lo_4, aux[a+d].lo_4);
                                aux[a].hi_4 = fmax(aux[a].hi_4, aux[a+d].hi_4);
                        }
                }
                barrier(CLK_LOCAL_MEM_FENCE);

                if (l_idx == 0) {
                        out_bboxes[group_id] = aux[0];
                }
        }
}

void prefix_sum(local  volatile int* bit,
                local  volatile int* sum)
{
        size_t lid = get_local_id(0);
        size_t lsz = get_local_size(0) / 2;

        int offset = 1;

        sum[lid] = bit[lid];

        for (int d = lsz; d > 0; d >>= 1) {
                barrier(CLK_LOCAL_MEM_FENCE);

                if (lid < d) {
                        int ai = offset * (2*lid+1)-1;
                        int bi = ai + offset;
                        sum[bi] += sum[ai];
                }
                offset <<= 1;
        }
        barrier(CLK_LOCAL_MEM_FENCE);

        if (lid == 0) {
                sum[lsz*2 - 1] = 0;
        }
        
        for (int d = 1; d < lsz<<1; d <<= 1) // traverse down tree & build scan
        {
                offset >>= 1;
                barrier(CLK_LOCAL_MEM_FENCE);
                if (lid < d) {
                        int ai = offset * (2*lid+1)-1;
                        int bi = ai + offset;
                        int t = sum[ai];
                        sum[ai] = sum[bi];
                        sum[bi] += t;
                }
        }
        barrier(CLK_LOCAL_MEM_FENCE);
        if (lid == 0) {
                sum[lsz*2] = bit[lsz*2-1] + sum[lsz*2-1];
        }
        barrier(CLK_LOCAL_MEM_FENCE);
}

int find_middle(global volatile BVHNode* node, 
                global morton_code_t* codes)
                /*, int level)  */
{
#define CODEBIT(x) ((codes[(x)].code[0]) & mask)

        
        unsigned int first_code = node->start_index;
        unsigned int last_code  = node->end_index-1;

        // node->leaf encodes the level we need to check
        int level = node->leaf;

        // bit mask for the level
        unsigned int mask = 1 << (29 - level);

        // if level is such that mask is 0, we output is as leaf
        if (!mask) {
                node->split_axis = (node->leaf-1)%3;
                node->leaf = 1;
                return 0;
        }

        // If node is small, just end it
        if (first_code + 4 >= last_code) {
                node->split_axis = level%3;
                node->leaf = 1;
                return 0;
        }
        
        // Advance levels until we found one with different start and end
        while (CODEBIT(first_code) != 0 || 
               CODEBIT(last_code)  == 0) {
                level++;
                mask >>= 1;
                if (!mask) {
                        node->split_axis = (level-1)%3;
                        node->leaf = 1;
                        return 0;
                }
        }

        // save level in node->leaf
        node->leaf = level;

        // Do binomial search for the middle
        while (first_code < last_code) {

                unsigned int middle = (first_code + last_code) / 2;
                if (CODEBIT(middle) == 0)  {
                        if (CODEBIT(middle+1) != 0)
                                return middle+1;
                        first_code = middle;
                } else {
                        if (CODEBIT(middle-1) == 0)
                                return middle;
                        last_code = middle;
                }

                if (first_code + 1 == last_code)
                        return last_code;
        }

        return last_code;

#undef CODEBIT

}


kernel void 
process_task(global volatile int* OC,        // Output counter
             global volatile int* PC,        // Processed counter
             global volatile BVHNode* nodes, // BVH Nodes
             global morton_code_t* codes,    // morton codes
             local volatile int*  LC,        // Local output counter
             local volatile int*  PS,        // local prefix sum
             int   level)                    // Current level
{

        int wgsize = (int)get_local_size(0);
        int wi     = (int)get_local_id(0);

        size_t inputOffset;
        size_t outputOffset;

        
        int    available_tasks;

        if (level == 0) {
                available_tasks = 1;
                inputOffset = 0;
        } else if (level == 1) {
                available_tasks = OC[0];
                inputOffset = 1;
        } else {
                available_tasks = OC[level-1];
                inputOffset = 1;
                for (int i = 0; i < level-1; ++i) {
                        inputOffset += OC[i];
                }
        }

        outputOffset = inputOffset + available_tasks;

        global volatile BVHNode* inputNodes = nodes + inputOffset;
        global volatile BVHNode* outputNodes = nodes + outputOffset;

        local volatile int firstNode;

        barrier(CLK_LOCAL_MEM_FENCE | CLK_GLOBAL_MEM_FENCE);

        if (wi == 0) {
                firstNode = atomic_add(PC + level, wgsize);
        }
        barrier(CLK_LOCAL_MEM_FENCE | CLK_GLOBAL_MEM_FENCE);

        while (firstNode < available_tasks) {
                LC[wi] = 0;
                int middle = 0;

                if (firstNode + wi < available_tasks)  {
                        middle = find_middle(inputNodes + firstNode + wi, 
                                             codes);
                }

                barrier(CLK_LOCAL_MEM_FENCE | CLK_GLOBAL_MEM_FENCE);

                if (middle)
                        LC[wi] = 2;

                barrier(CLK_LOCAL_MEM_FENCE | CLK_GLOBAL_MEM_FENCE);

                ///////// PREFIX SUM!
                prefix_sum(LC, PS);
                barrier(CLK_LOCAL_MEM_FENCE | CLK_GLOBAL_MEM_FENCE);

                local volatile int localOffset;
                if (wi == 0) {
                        int outputTasks = PS[wgsize];
                        localOffset  = atomic_add(OC + level, outputTasks);
                }
                barrier(CLK_LOCAL_MEM_FENCE | CLK_GLOBAL_MEM_FENCE);

                if (middle) {
                        /* printf("inputNodes: %d\n", inputOffset); */
                        /* printf("outputNodes: %d\n", outputOffset); */
                        /* printf("localOffset: %d\n", localOffset); */
                        /* printf("PS[wi]: %d\n", PS[wi]); */

                        global volatile BVHNode* parent = inputNodes + firstNode + wi;
                        global volatile BVHNode* lchild = outputNodes+localOffset+PS[wi];
                        global volatile BVHNode* rchild = lchild+1;

                        lchild->leaf = parent->leaf+1; // Next level we check
                        rchild->leaf = parent->leaf+1;
                        parent->leaf = 0;

                        lchild->start_index = parent->start_index;
                        lchild->end_index   = middle;

                        rchild->start_index = middle;
                        rchild->end_index   = parent->end_index;

                        parent->l_child = outputOffset + localOffset + PS[wi];
                        parent->r_child =  parent->l_child + 1;

                        lchild->parent = inputOffset + firstNode + wi;
                        rchild->parent = lchild->parent;

                        lchild->split_axis = (parent->split_axis+1)%3;
                        rchild->split_axis = lchild->split_axis;
                }
                barrier(CLK_LOCAL_MEM_FENCE | CLK_GLOBAL_MEM_FENCE);
                
                if (wi == 0) {
                        firstNode = atomic_add(PC + level, wgsize);
                }
                barrier(CLK_LOCAL_MEM_FENCE | CLK_GLOBAL_MEM_FENCE);
        }
}
