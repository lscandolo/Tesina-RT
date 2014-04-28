#define M_UINT_COUNT 2 

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

typedef struct { //Temporary fix because of cl c / api size inconsistency
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
morton_encode_fixed(global BBox* bboxes,
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

        slice *= 1024.f;

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

        for (int i = 0; i < 30; ++i) {
                int src       = 2-(i%3);
                int src_bit   = i/3;

                int dest      = M_UINT_COUNT-1-(i/(sizeof(uint)*8));
                int dest_bit  = i%(sizeof(uint)*8);

                morton.code[dest] |= ((morton_in[src]>>src_bit) & 1)<<dest_bit;
        }

        morton_codes[index] = morton;

}

kernel void
morton_encode(global BBox* bboxes,
              global BBox* global_bbox,
              global morton_code_t* morton_codes,
              int depth) 
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
        ///// Each axis will get divided in half depth/3 times, so each slice will
        /////  be multiplied by pow(2, depth/3) to get the bits corresponding to its 
        /////  belonging in each axis divide. depth/3 is ceil

        slice *= pow(2, ceil(depth/3.0));

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


        ///// We'll take from the morton codes, we need to get to bit
        ///// ceil(depth/3.0) on all 3 axis, so we start with the 
        ///// offset depth%3

        // We assume depth < 32 * 3

        // The offset is to ignore the least significant bits of some axes if
        //  the depth is not divisible by 3 
        //  e.g. (if depth is 10, we want bits 3x 3y 3z 2x 2y 2z 1x 1y 1z 0x)
        int offset = depth%3 ? 3 - (depth%3) : 0;

        int axis = (offset+1)%3;// This is just to get a consistent first division axis (x)

        for (int i = 0; i < depth; ++i) {


                // We want the last 3 to be depth/3
                int src_bit = (i + offset ) / 3; 

                /* int src_bit   = i / 3; */

                int dest_bit  = i % (8 * sizeof(unsigned int));
                int dest_idx  = i / (8 * sizeof(unsigned int));

                morton.code[dest_idx] |= ((morton_in[axis]>>src_bit) & 1)<<dest_bit;

                axis = (axis+1)%3;
        }

        morton_codes[index] = morton;
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

        if (node->leaf)
                return;

        BBox bbox = lchild(node).bbox;
        merge_bbox(&bbox, rchild(node).bbox);
        node->bbox = bbox;
        //////////////////

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

void prefix_sum_double(local   int* bit,
                       local   int* sum)
{
        size_t lid = get_local_id(0);
        size_t lsz = get_local_size(0);

        int offset = 1;

        sum[lid] = bit[lid];
        sum[lid+lsz] = bit[lid+lsz];

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

void prefix_sum(local   int* bit,
                local   int* sum)
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

int find_middle(global  BVHNode* node, 
                global morton_code_t* codes,
                int max_level,
                int min_leaf_size)
{
#define CODEBIT(x) ((codes[(x)].code[bit/32]) & (1 << (bit%32)))

        unsigned int first_code = node->start_index;
        unsigned int last_code  = node->end_index-1;

        // node->leaf encodes the level we need to check
        int level = node->leaf;

        // bit for the level
        int bit = max_level - 1 - level;

        // If node is small, just end it
        if (last_code - first_code - 1 <= min_leaf_size) {
                node->leaf = 1;
                return 0;
        }

        // if level is such that mask is 0, we output is as leaf
        if (bit < 0) {
                node->leaf = 1;
                return 0;
                /* node->leaf = 30; */
                /* return (first_code+last_code)/2; */
        }
      
        // Advance levels until we find one with different start and end
        while (CODEBIT(first_code) != 0 || 
               CODEBIT(last_code)  == 0) {
                level++;
                bit = max_level - 1 - level;
                if (bit < 0) {
                        node->leaf = 1;
                        return 0;
                        /* node->leaf = 30; */
                        /* return (first_code+last_code)/2; */
                }
        }

        // save level in node->leaf
        node->leaf = level;

        // Do binomial search for the middle
        while (first_code+1 < last_code) {

                unsigned int middle = (first_code + last_code) / 2;
                if (CODEBIT(middle) == 0)  {
                        /* if (CODEBIT(middle+1) != 0) */
                        /*         return middle+1; */
                        first_code = middle;
                } else {
                        /* if (CODEBIT(middle-1) == 0) */
                        /*         return middle; */
                        last_code = middle;
                }

                /* if (first_code + 1 == last_code) */
                /*         return last_code; */
        }

        return last_code;

#undef CODEBIT

}

void set_children(global BVHNode* nodes,
                  int parentIndex,
                  int childrenIndex,
                  int middle) 
{
        global BVHNode* parent = nodes + parentIndex;
        global BVHNode* lchild = nodes + childrenIndex;
        global BVHNode* rchild = lchild + 1;

        int child_level = parent->leaf+1; // Next level we check
        lchild->leaf = child_level; // Next level we check
        rchild->leaf = child_level;
        /* parent->split_axis = (parent->leaf)%3; */
        parent->leaf = 0;

        lchild->start_index = parent->start_index;
        lchild->end_index   = middle;

        rchild->start_index = middle;
        rchild->end_index   = parent->end_index;

        parent->l_child = childrenIndex;
        parent->r_child = childrenIndex + 1;

        lchild->parent = parentIndex;
        rchild->parent = parentIndex;
}

kernel void
process_task(global  int* OC,        // Output counter
             global  int* PC,        // Processed counter
             global  BVHNode* nodes, // BVH Nodes
             global morton_code_t* codes,    // morton codes
             local int*  LC,        // Local output counter
             local int*  PS,        // local prefix sum
             int   max_level, // Maximum depth of the bvh
             int   min_leaf_size, // Cutoff size for leaf nodes
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

        global  BVHNode* inputNodes = nodes + inputOffset;
        global  BVHNode* outputNodes = nodes + outputOffset;

        local int firstNode;

        if (wi == 0) {
                firstNode = atomic_add(PC + level, wgsize);
        }
        barrier(CLK_LOCAL_MEM_FENCE | CLK_GLOBAL_MEM_FENCE);

        while (firstNode < available_tasks) {
                LC[wi] = 0;
                int middle = 0;

                if (firstNode + wi < available_tasks)  {
                        middle = find_middle(inputNodes + firstNode + wi,
                                             codes, 
                                             max_level,
                                             min_leaf_size);
                }

                if (middle)
                        LC[wi] = 2;

                barrier(CLK_LOCAL_MEM_FENCE);

                ///////// PREFIX SUM!
                prefix_sum(LC, PS);
                barrier(CLK_LOCAL_MEM_FENCE);

                local int localOffset;
                if (wi == 0) {
                        int outputTasks = PS[wgsize];
                        localOffset  = atomic_add(OC + level, outputTasks);
                }
                barrier(CLK_LOCAL_MEM_FENCE);

                if (middle) {

                        set_children(nodes,
                                     inputOffset + firstNode + wi,
                                     outputOffset + localOffset + PS[wi],
                                     middle);
                                     
                }
                /* barrier(CLK_LOCAL_MEM_FENCE | CLK_GLOBAL_MEM_FENCE); */
                
                if (wi == 0) {
                        firstNode = atomic_add(PC + level, wgsize);
                }
                barrier(CLK_LOCAL_MEM_FENCE);
        }
}
