#define UINT_MAX_INV 2.3283064370807974e-10f
#define UINT_MAX_F 4294967295.f

#define M_AXIS_BITS  10
#define M_UINT_COUNT 1 //This value needs to be (1+ (M_AXIS_BITS*3-1)/32)

#define MORTON_MAX_F 1024.f // 2^M_AXIS_BITS
#define MORTON_MAX_INV_F 0.000976562.f //1/MORTON_MAX_F

#define BBOX_NOT_COMPUTED 1e35f

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

typedef struct { //Temporary fix because of cl / cl size api inconsistency
        union {
                float3 hi;
                float4 hi_4;
        };        
        union {
                float3 lo;
                float4 lo_4;
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
              BBox global_bbox,
              const float4 inv_global_bbox_size,
              global morton_code_t* morton_codes) 
{
        int index = get_global_id(0);
        BBox bbox = bboxes[index];

        float3 g_lo = global_bbox.lo;
        float3 g_hi = global_bbox.hi;

        float3 center = (bbox.hi + bbox.lo) * 0.5f;

        float3 slice  = (center - g_lo) * inv_global_bbox_size.xyz;
        const float3 zeros = (float3)(0.f,0.f,0.f);
        const float3 ones = (float3)(1.f,1.f,1.f);
        slice = fmax(zeros,fmin(ones,slice));

        slice *= MORTON_MAX_F;

        /*Scramble codes*/
        
        unsigned int morton_in[3];
        morton_in[0] = slice.x;
        morton_in[1] = slice.y;
        morton_in[2] = slice.z;

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

unsigned char split_bit(morton_code_t c1, morton_code_t c2) 
{
        if (M_UINT_COUNT == 1) {
                c1.code[0] = c1.code[0] ^ c2.code[0];
                for (int i = 0; i < M_AXIS_BITS*3; ++i) {
                        if (c1.code[0]>> (M_AXIS_BITS*3-i-1)) {
                                return M_AXIS_BITS*3-i;
                        }
                }
                return 0;
        } else {

                //// This is broken, i think!
                for (int u = 0; u < M_UINT_COUNT; ++u) {
                        if (c1.code[u] != c2.code[u]) {
                                c1.code[u] = c1.code[u] ^ c2.code[u];
                                int max_bit = (u==0)?
                                        (M_AXIS_BITS*3-1)%(sizeof(uint)*8):
                                        sizeof(uint)*8 - 1;
                                for (int i = 0; i <= max_bit; ++i)
                                        if (c1.code[u] >> (max_bit-i-1))
                                                return (M_UINT_COUNT-u)*
                                                        sizeof(unsigned int)*8-i;
                        }
                }
                return 0;
        }
}

kernel void
build_splits(global unsigned int*    triangles,
             global morton_code_t*   morton_codes,
             global unsigned char*   splits,
             local  morton_code_t*   local_codes)
{
        size_t gid = get_global_id(0);
        size_t lid = get_local_id(0);
        size_t lsz = get_local_size(0);

        local_codes[lid] = morton_codes[triangles[gid]];
        if (lid == 0)
                local_codes[lsz] = morton_codes[triangles[gid+lsz]];

        barrier(CLK_LOCAL_MEM_FENCE);
        splits[gid] = split_bit(local_codes[lid],local_codes[lid+1]);
        
}

kernel void
treelet_maps_init(global int*    node_map,
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
segment_map_init(global int*    node_map,
                 global int*    segment_map)
{
        size_t gid = get_global_id(0);
        
        /* We set the previous element because we will do a scan afterwards */
        segment_map[gid-1] = node_map[gid] >= 0 ? 1 : 0;
}

kernel void
segment_heads_init(global int*    segment_map,
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
treelet_init(global int*    treelets)
{
        size_t gid = get_global_id(0);
        treelets[gid] = T_NULL;
}

kernel void
treelet_build(global int*            node_map,
              global int*            segment_map,
              global int*            segment_heads,
              unsigned int           bitmask,
              unsigned int           triangle_count,
              unsigned int           pass,
              global unsigned int*   triangles,
              global morton_code_t*  morton_codes,
              local morton_code_t*   local_codes,
              global int*            treelets)
{
        const size_t gid = get_global_id(0);
        const size_t lid = get_local_id(0);
        const size_t lsz = get_local_size(0);
        local_codes[lid] = morton_codes[triangles[gid]];
        if (lid == 0)
                local_codes[lsz] = morton_codes[triangles[gid+lsz]];
        barrier(CLK_LOCAL_MEM_FENCE);

        unsigned int lcode = local_codes[lid].code[0];
        unsigned int rcode = local_codes[lid+1].code[0];

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
                /* if (treelet[current_node] == T_NULL) */
                /*         break; */

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
        bool change = (lcode^rcode) & bitmask;
        if (change && (gid+1) < node_end) {
                treelet[current_node] = gid+1;
        }
        ///////// If the first and last codes in this nodes are the same, there
        ///////// is no change, so mark it
        if  (gid == node_start) {
                unsigned int last_code = morton_codes[triangles[node_end-1]].code[0];
                if (! ((lcode^last_code) & bitmask)) {
                        if (lcode & bitmask)
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

void link_children(global BVHNode* parent,
                   unsigned int    parent_id,
                   global BVHNode* lchild,
                   unsigned int    lchild_id,
                   global BVHNode* rchild,
                   unsigned int    rchild_id,
                   int             split_id);

int process_node_split_0(global BVHNode* nodes,
                         unsigned int    parent_id,
                         unsigned int    children_offset,
                         global   int*   treelets,
                         unsigned int    first_primitive,
                         unsigned int    end_primitive,
                         global int*     node_map,
                         global int*     segment_heads,
                         bool last_pass);

kernel void
build_nodes(global int*            node_map,
            global int*            segment_map,
            global int*            segment_heads,
            unsigned int           triangle_count,
            global unsigned int*   triangles,
            global morton_code_t*  morton_codes,
            global int*            treelets,
            unsigned int           global_node_offset,
            global int*            node_offsets,
            global BVHNode*        nodes,
            int                    last_pass)
{
        size_t gid = get_global_id(0);

        treelets+= TREELET_SIZE*gid;
        
        size_t parent_id = node_map[segment_heads[gid]];

        int start_offset = global_node_offset + node_offsets[gid];

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
                                             segment_heads,
                                             last_pass);

}


int process_node_split_2(global BVHNode* nodes,
                         unsigned int    parent_id,
                         unsigned int    children_offset,
                         global   int*   treelets,
                         int             treelet_entry,
                         int             first_primitive,
                         int             end_primitive,
                         global int*     node_map,
                         global int*     segment_heads,
                         bool last_pass)
{
        if (!last_pass) {

                if (treelets[treelet_entry] >= 0) {
                        unsigned int lnode_id = children_offset;
                        unsigned int rnode_id = lnode_id + 1;
                        link_children(nodes+parent_id,parent_id,
                                      nodes+lnode_id, lnode_id,
                                      nodes+rnode_id, rnode_id,
                                      treelet_entry);

                        //save node start location 
                        node_map[first_primitive] = lnode_id;
                        node_map[treelets[treelet_entry]] =   rnode_id;

                        return 2;
                } else if (treelets[0] == T_ALL_0) {
                        //save node start location
                        node_map[first_primitive] = parent_id;
                        return 0;
                } else { //treelets[0] == T_ALL_1
                        //save node start location
                        node_map[first_primitive] = parent_id;
                        return 0;
                }

        } else {
                size_t gid = get_global_id(0);
                unsigned int start_index = first_primitive;
                unsigned int end_index = end_primitive;

                if (treelets[treelet_entry] >= 0) {
                        unsigned int lnode_id = children_offset;
                        unsigned int rnode_id = lnode_id + 1;
                        link_children(nodes+parent_id,parent_id,
                                      nodes+lnode_id, lnode_id,
                                      nodes+rnode_id, rnode_id,
                                      treelet_entry);

                        nodes[lnode_id].leaf = true;
                        nodes[lnode_id].start_index = start_index;
                        nodes[lnode_id].end_index = treelets[treelet_entry];
                        nodes[lnode_id].bbox.lo.x = BBOX_NOT_COMPUTED;

                        nodes[rnode_id].leaf = true;
                        nodes[rnode_id].start_index = treelets[treelet_entry];
                        nodes[rnode_id].end_index = end_index;
                        nodes[rnode_id].bbox.lo.x = BBOX_NOT_COMPUTED;

                        return 2;
                } else if (treelets[treelet_entry] == T_ALL_0) {
                        nodes[parent_id].leaf = true;
                        nodes[parent_id].start_index = start_index;
                        nodes[parent_id].end_index = end_index;
                        nodes[parent_id].bbox.lo.x = BBOX_NOT_COMPUTED;
                        return 0;
                } else { //treelets[0] == T_ALL_1
                        nodes[parent_id].leaf = true;
                        nodes[parent_id].start_index = start_index;
                        nodes[parent_id].end_index = end_index;
                        nodes[parent_id].bbox.lo.x = BBOX_NOT_COMPUTED;
                        return 0;
                }

        }
}

int process_node_split_1(global BVHNode* nodes,
                         unsigned int    parent_id,
                         unsigned int    children_offset,
                         global   int*   treelets,
                         int             treelet_entry,
                         int             first_primitive,
                         int             end_primitive,
                         global int*     node_map,
                         global int*     segment_heads,
                         bool last_pass)
{
        int lentry = treelet_entry*2+1;
        int rentry = treelet_entry*2+2;
        if (treelets[treelet_entry] >= 0) {
                unsigned int lnode_id = children_offset;
                unsigned int rnode_id = lnode_id + 1;
                link_children(nodes+parent_id,parent_id,
                              nodes+lnode_id, lnode_id,
                              nodes+rnode_id, rnode_id,
                              treelet_entry);

                int new_nodes = 2;
                new_nodes += process_node_split_2(nodes,lnode_id,
                                                  children_offset+new_nodes,
                                                  treelets,lentry,
                                                  first_primitive,
                                                  treelets[treelet_entry],
                                                  node_map,
                                                  segment_heads,
                                                  last_pass);

                new_nodes += process_node_split_2(nodes,rnode_id,
                                                  children_offset+new_nodes,
                                                  treelets,rentry,
                                                  treelets[treelet_entry],
                                                  end_primitive,
                                                  node_map,
                                                  segment_heads,
                                                  last_pass);
                return new_nodes;
        } else if (treelets[treelet_entry] == T_ALL_0) {
                int new_nodes = 0;
                new_nodes += process_node_split_2(nodes,parent_id,
                                                  children_offset,
                                                  treelets,lentry,
                                                  first_primitive,
                                                  end_primitive,
                                                  node_map,
                                                  segment_heads,
                                                  last_pass);
                return new_nodes;
        } else { //treelets[0] == T_ALL_1
                int new_nodes = 0;
                new_nodes += process_node_split_2(nodes,parent_id,
                                                  children_offset,
                                                  treelets,rentry,
                                                  first_primitive,
                                                  end_primitive,
                                                  node_map,
                                                  segment_heads,
                                                  last_pass);
                return new_nodes;
        }
}

int process_node_split_0(global BVHNode* nodes,
                         unsigned int    parent_id,
                         unsigned int    children_offset,
                         global   int*   treelets,
                         unsigned int    first_primitive,
                         unsigned int    end_primitive,
                         global int*     node_map,
                         global int*     segment_heads,
                         bool last_pass)
{
        if (treelets[0] >= 0) {
                unsigned int lnode_id = children_offset;
                unsigned int rnode_id = lnode_id + 1;
                link_children(nodes+parent_id,parent_id,
                              nodes+lnode_id, lnode_id,
                              nodes+rnode_id, rnode_id,
                              treelets[0]);

                int new_nodes = 2;
                new_nodes += process_node_split_1(nodes,lnode_id,
                                                  children_offset+new_nodes,
                                                  treelets,1,
                                                  first_primitive,
                                                  treelets[0],
                                                  node_map,
                                                  segment_heads,
                                                  last_pass);

                new_nodes += process_node_split_1(nodes,rnode_id,
                                                  children_offset+new_nodes,
                                                  treelets,2,
                                                  treelets[0],
                                                  end_primitive,
                                                  node_map,
                                                  segment_heads,
                                                  last_pass);
                return new_nodes;
        } else if (treelets[0] == T_ALL_0) {
                int new_nodes = 0;
                new_nodes += process_node_split_1(nodes,parent_id,
                                                  children_offset,
                                                  treelets,1,
                                                  first_primitive,
                                                  end_primitive,
                                                  node_map,
                                                  segment_heads,
                                                  last_pass);
                return new_nodes;
        } else { //treelets[0] == T_ALL_1
                int new_nodes = 0;
                new_nodes += process_node_split_1(nodes,parent_id,
                                                  children_offset,
                                                  treelets,2,
                                                  first_primitive,
                                                  end_primitive,
                                                  node_map,
                                                  segment_heads,
                                                  last_pass);
                return new_nodes;
        }
}


void link_children(global BVHNode* parent_node,
                   unsigned int    parent_id,
                   global BVHNode* lnode,
                   unsigned int    lchild_id,
                   global BVHNode* rnode,
                   unsigned int    rchild_id,
                   int             split_id)
{
        parent_node->l_child = lchild_id;
        parent_node->r_child = rchild_id;
        parent_node->leaf = false;
        lnode->parent = parent_id;
        rnode->parent = parent_id;

        /*Split axis will be x->y->z->x... throughout the tree because 
          of the morton encoding */
        lnode->split_axis = (parent_node->split_axis + 1)%3;
        rnode->split_axis = lnode->split_axis;

        /* Set marker to know if the bbox has been computed yet */
        parent_node->bbox.lo.x = BBOX_NOT_COMPUTED;
}

void merge_bbox(BBox* bbox, BBox bbox2) 
{
        bbox->lo = fmin(bbox->lo, bbox2.lo);
        bbox->hi = fmax(bbox->hi, bbox2.hi);
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
                                break;
                }
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

        if (node->leaf ||
            lchild(node).bbox.lo.x == BBOX_NOT_COMPUTED ||
            rchild(node).bbox.lo.x == BBOX_NOT_COMPUTED) 
                return ;

        /* if (!node->leaf &&  */
        /*     lchild(node).bbox.lo.x != BBOX_NOT_COMPUTED &&  */
        /*     rchild(node).bbox.lo.x != BBOX_NOT_COMPUTED) { */
                BBox bbox = lchild(node).bbox;
                merge_bbox(&bbox, rchild(node).bbox);

                node->bbox.hi = bbox.hi;
                mem_fence(CLK_GLOBAL_MEM_FENCE);
                node->bbox.lo = bbox.lo;
                /* This is to make sure the flag to compute
                 *  the parent bbox is not set until every other value is set
                 */
        /* } */

#undef lchild
#undef rchild

}

