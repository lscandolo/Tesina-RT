#include <rt/bvh-builder.hpp>
#include <gpu/scan.hpp>

#define M_AXIS_BITS  10
#define M_UINT_COUNT 1 //This value needs to be (1+ (M_AXIS_BITS*3-1)/32)

#define MORTON_MAX_F 1024.f // 2^M_AXIS_BITS
#define MORTON_MAX_INV_F 0.000976562.f //1/MORTON_MAX_F

typedef struct {

        unsigned int code[M_UINT_COUNT];

} morton_code_t;

static size_t pad(size_t size, size_t pad_multiplier) {
        size_t rem = size%pad_multiplier;
        if (!rem)
                return size;
        return size + pad_multiplier - rem;
}

static int test_node_2(std::vector<BVHNode>& nodes, 
                int id,
                int depth,
                std::vector<int>& prim_checked, 
                std::vector<int>& node_checked) 
{
        for (int i = 0; i < nodes.size(); i++) {
                BVHNode& node = nodes[i];
                if (node.m_leaf) {
                        if (node.m_start_index > 12800 ||
                            node.m_end_index   > 12800 ||
                            node.m_start_index > node.m_end_index) {
                                std::cerr << "Error at " << i << std::endl;
                                std::cerr << "\tstart_index: " << node.m_start_index  
                                          << "\tend_index: " << node.m_end_index 
                                          << std::endl;
                                continue;
                        }
                        for (int j = node.m_start_index; j < node.m_end_index; j++)
                                prim_checked[j]++;
                }
        }
        return 0;
}

static int test_node (std::vector<BVHNode>& nodes, 
               int id,
               int depth,
               std::vector<int>& prim_checked,
               std::vector<int>& node_checked) 
{
        BVHNode& node = nodes[id];
        node_checked[id]++;
        if (node.m_leaf) {
                if (node.m_start_index > 12800 || node.m_end_index > 12800 ||
                    node.m_start_index > node.m_end_index) {
                        std::cerr << "Error at " << id << std::endl;
                        std::cerr << "\tstart_index: " << node.m_start_index  
                                  << "\tend_index: " << node.m_end_index 
                                  << std::endl;
                }
                for (int i = node.m_start_index; i < node.m_end_index; i++)
                        prim_checked[i]++;
                return depth + 1;
        }

        return std::max(test_node(nodes,node.m_l_child,depth+1,prim_checked,node_checked),
                        test_node(nodes,node.m_r_child,depth+1,prim_checked,node_checked));
}

int32_t 
BVHBuilder::initialize(CLInfo& clinfo)
{
        DeviceInterface& device = *DeviceInterface::instance();

        if (!device.good())
                return -1;

        triangles_mem_id = device.new_memory();

        std::vector<std::string> builder_names;
        builder_names.push_back("build_primitive_bbox");
        builder_names.push_back("morton_encode");
        builder_names.push_back("rearrange_indices");
        builder_names.push_back("rearrange_map");
        builder_names.push_back("build_splits");
        builder_names.push_back("init_treelet_maps");
        builder_names.push_back("init_segment_map");
        builder_names.push_back("init_segment_heads");
        builder_names.push_back("init_treelet");
        builder_names.push_back("build_treelet");
        builder_names.push_back("build_nodes");
        builder_names.push_back("count_nodes");
        builder_names.push_back("build_node_bbox");
        builder_names.push_back("build_leaf_bbox");
        
        std::vector<function_id> builder_ids = 
                device.build_functions("src/kernel/bvh-builder.cl", builder_names);
        if (!builder_ids.size())
                return -1;

        int id = 0;
        primitive_bbox_builder_id = builder_ids[id++];
        morton_encoder_id         = builder_ids[id++];
        index_rearranger_id       = builder_ids[id++];
        map_rearranger_id         = builder_ids[id++];
        splits_build_id           = builder_ids[id++];
        treelet_maps_init_id      = builder_ids[id++];
        segment_map_init_id       = builder_ids[id++];
        segment_heads_init_id     = builder_ids[id++];
        treelet_init_id           = builder_ids[id++];
        treelet_build_id          = builder_ids[id++];
        node_build_id             = builder_ids[id++];
        node_counter_id           = builder_ids[id++];
        node_bbox_builder_id      = builder_ids[id++];
        leaf_bbox_builder_id      = builder_ids[id++];

        std::vector<std::string> sorter_names;
        sorter_names.push_back("morton_sort_g2");
        sorter_names.push_back("morton_sort_g4");
        sorter_names.push_back("morton_sort_g8");
        sorter_names.push_back("morton_sort_g16");


        std::vector<function_id> sorter_ids = 
                device.build_functions("src/kernel/bvh-builder-sort.cl", sorter_names);
        if (!sorter_ids.size())
                return -1;
        morton_sorter_2_id  = sorter_ids[0];
        morton_sorter_4_id  = sorter_ids[1];
        morton_sorter_8_id  = sorter_ids[2];
        morton_sorter_16_id = sorter_ids[3];

        /*--------------------------------------------------------------------------*/

	m_timing = true;
	return 0;
}

int32_t
BVHBuilder::build_bvh(Scene& scene)
{
        
        /*  New Pseudocode: 
         *
         * 1. Compute BBox for each primitive
         *
         * 2. Create morton code encoding of barycenters
         *
         * 3. Sort the primitives by their morton code
         *
         * 4. Compute treelets and ouput nodes
         * 
         * 5. Compute node BBoxes
         * 
         * 6. Voila
         *
         */
        
        if (m_timing)
                m_timer.snap_time();
        
        DeviceInterface& device = *DeviceInterface::instance();
        bool      log_partial_time = false;
        double    partial_time_ms;
        rt_time_t partial_timer;
        size_t triangle_count = scene.triangle_count();

        //////////////////////////////////////////////////////////////////////////
        //////////   1. Compute BBox for each primitive //////////////////////////
        //////////////////////////////////////////////////////////////////////////
        partial_timer.snap_time();
        memory_id bboxes_mem_id = device.new_memory();
        DeviceMemory& bboxes_mem = device.memory(bboxes_mem_id);
        size_t bboxes_size = sizeof(BBox)*triangle_count;
        if (!bboxes_mem.valid())
                if (bboxes_mem.initialize(bboxes_size))
                        return -1;

        if (bboxes_mem.size() < bboxes_size)
                if (bboxes_mem.resize(bboxes_size))
                        return -1;

        DeviceMemory& triangles_mem = device.memory(triangles_mem_id);
        size_t triangles_size = sizeof(unsigned int)*triangle_count;
        if (!triangles_mem.valid())
                if (triangles_mem.initialize(triangles_size))
                        return -1;

        if (triangles_mem.size() < triangles_size)
                if (triangles_mem.resize(triangles_size))
                        return -1;
                    
        DeviceFunction& primitive_bbox_builder = device.function(primitive_bbox_builder_id);
        if (primitive_bbox_builder.set_arg(0,scene.vertex_mem()) ||
            primitive_bbox_builder.set_arg(1,scene.index_mem()) ||
            primitive_bbox_builder.set_arg(2,bboxes_mem) ||
            primitive_bbox_builder.set_arg(3,triangles_mem))
                return -1;
        if (primitive_bbox_builder.enqueue_single_dim(triangle_count))
                return -1;
        device.enqueue_barrier();
        partial_time_ms = partial_timer.msec_since_snap();
        if (log_partial_time)
                std::cout << "BBox builder time: " << partial_time_ms << " ms\n";
        
        //////////////////////////////////////////////////////////////////////////
        ///////////// 2. Create morton code encoding of barycenters //////////////
        //////////////////////////////////////////////////////////////////////////
        partial_timer.snap_time();
        memory_id morton_mem_id = device.new_memory();
        DeviceMemory& morton_mem = device.memory(morton_mem_id);
        size_t morton_size = sizeof(morton_code_t)*triangle_count;
        if (!morton_mem.valid())
                if (morton_mem.initialize(morton_size))
                        return -1;

        if (morton_mem.size() < morton_size)
                if (morton_mem.resize(morton_size))
                        return -1;
                    
        std::vector<BBox> bboxes(triangle_count);
        bboxes_mem.read(sizeof(BBox) * bboxes.size(), &(bboxes[0]));
        BBox& global_bbox = scene.bbox(); /*!! This should be computed in gpu */
        global_bbox = bboxes[0];
        for (size_t i = 1; i < bboxes.size(); ++ i) {
                global_bbox.merge(bboxes[i]);
        }

        cl_float4 inv_global_bbox_size;
        inv_global_bbox_size.s[0] = 1.f/(global_bbox.hi.s[0] - global_bbox.lo.s[0]);
        inv_global_bbox_size.s[1] = 1.f/(global_bbox.hi.s[1] - global_bbox.lo.s[1]);
        inv_global_bbox_size.s[2] = 1.f/(global_bbox.hi.s[2] - global_bbox.lo.s[2]);
        DeviceFunction& morton_encoder = device.function(morton_encoder_id);
        if (morton_encoder.set_arg(0,bboxes_mem) ||
            morton_encoder.set_arg(1,sizeof(BBox),   &global_bbox) ||
            morton_encoder.set_arg(2,sizeof(cl_float4), &inv_global_bbox_size) ||
            morton_encoder.set_arg(3,morton_mem))
                return -1;
        if (morton_encoder.enqueue_single_dim(triangle_count))
                return -1;
        device.enqueue_barrier();
        partial_time_ms = partial_timer.msec_since_snap();
        if (log_partial_time)
                std::cout << "Morton encoder time: " << partial_time_ms << " ms\n";

        //////////////////////////////////////////////////////////////////////////
        ///////////// 3. Sort the primitives by their morton code ////////////////
        //////////////////////////////////////////////////////////////////////////
        partial_timer.snap_time();
        int morton_sort_size = triangle_count;
        int morton_padded_sort_size = 1;
        while (morton_padded_sort_size < morton_sort_size) /*Padded sort size is the */
                morton_padded_sort_size<<=1;               /* smallest power of 2 equal */
                                                           /* or bigger than size */

        size_t ms2_work_items = morton_sort_size/2 + (morton_sort_size%2);
        size_t ms4_work_items = morton_sort_size/4 + (morton_sort_size-morton_sort_size%4);
        size_t ms8_work_items = morton_sort_size/8 + (morton_sort_size-morton_sort_size%8);
        size_t ms16_work_items = morton_sort_size/16+(morton_sort_size-morton_sort_size%16);

        DeviceFunction& morton_sorter_2 = device.function(morton_sorter_2_id);
        DeviceFunction& morton_sorter_4 = device.function(morton_sorter_4_id);
        DeviceFunction& morton_sorter_8 = device.function(morton_sorter_8_id);
        DeviceFunction& morton_sorter_16 = device.function(morton_sorter_16_id);

        morton_sorter_2.set_arg(0,morton_mem);
        morton_sorter_2.set_arg(1,triangles_mem);

        morton_sorter_4.set_arg(0,morton_mem);
        morton_sorter_4.set_arg(1,triangles_mem);

        morton_sorter_8.set_arg(0,morton_mem);
        morton_sorter_8.set_arg(1,triangles_mem);

        morton_sorter_16.set_arg(0,morton_mem);
        morton_sorter_16.set_arg(1,triangles_mem);

        for (int len = 1; len < morton_padded_sort_size; len<<=1) {
                int dir = (len<<1);
                int inv = ((morton_sort_size-1)&dir) == 0 && dir != morton_padded_sort_size;
                int last_step = dir == morton_padded_sort_size;
                for (int inc = len; inc > 0; inc >>= 1) {

                        if (inc > 4) {
                                inc >>=3;
                                morton_sorter_16.set_arg(2,sizeof(int),&inc);
                                morton_sorter_16.set_arg(3,sizeof(int),&dir);
                                morton_sorter_16.set_arg(4,sizeof(int),&inv);
                                morton_sorter_16.set_arg(5,sizeof(int),&last_step);
                                morton_sorter_16.set_arg(6,sizeof(int),&morton_sort_size);
                                
                                if (morton_sorter_16.enqueue_single_dim(ms16_work_items))
                                        exit(1);
                                device.enqueue_barrier();

                        } else if (inc > 2) {
                                inc >>=2;
                                morton_sorter_8.set_arg(2,sizeof(int),&inc);
                                morton_sorter_8.set_arg(3,sizeof(int),&dir);
                                morton_sorter_8.set_arg(4,sizeof(int),&inv);
                                morton_sorter_8.set_arg(5,sizeof(int),&last_step);
                                morton_sorter_8.set_arg(6,sizeof(int),&morton_sort_size);
                                
                                if (morton_sorter_8.enqueue_single_dim(ms8_work_items))
                                        exit(1);
                                device.enqueue_barrier();

                        } else if (inc > 1) {
                                inc>>=1;
                                morton_sorter_4.set_arg(2,sizeof(int),&inc);
                                morton_sorter_4.set_arg(3,sizeof(int),&dir);
                                morton_sorter_4.set_arg(4,sizeof(int),&inv);
                                morton_sorter_4.set_arg(5,sizeof(int),&last_step);
                                morton_sorter_4.set_arg(6,sizeof(int),&morton_sort_size);
                                
                                if (morton_sorter_4.enqueue_single_dim(ms4_work_items))
                                        exit(1);
                                device.enqueue_barrier();
                        } else {
                                morton_sorter_2.set_arg(2,sizeof(int),&inc);
                                morton_sorter_2.set_arg(3,sizeof(int),&dir);
                                morton_sorter_2.set_arg(4,sizeof(int),&inv);
                                morton_sorter_2.set_arg(5,sizeof(int),&last_step);
                                morton_sorter_2.set_arg(6,sizeof(int),&morton_sort_size);
                                
                                if (morton_sorter_2.enqueue_single_dim(ms2_work_items))
                                        exit(1);
                                device.enqueue_barrier();
                        }
                }
        }
        // device.finish_commands();
        partial_time_ms = partial_timer.msec_since_snap();
        if (log_partial_time)
                std::cout << "Morton sorter: " << partial_time_ms << " ms" << std::endl;



        ///////////////////////////////////////////////////////////////////////////////
        ////////////////////// 3.2 Rearrange indices and map //////////////////////////
        ///////////////////////////////////////////////////////////////////////////////
        //////////////////////////////////
        DeviceFunction& index_rearranger = device.function(index_rearranger_id);
        DeviceFunction& map_rearranger   = device.function(map_rearranger_id);

        ////// Rearrange indices
        memory_id aux_id = device.new_memory();
        DeviceMemory& aux_mem = device.memory(aux_id);
        size_t aux_mem_size = std::max(scene.index_mem().size(), bboxes_mem.size());

        if (aux_mem.initialize(aux_mem_size))
                return -1;
        if (scene.index_mem().copy_to(aux_mem)) {
                std::cout << "Error copying " << std::endl;
                return -1;
        }
        if (index_rearranger.set_arg(0,aux_mem) || 
            index_rearranger.set_arg(1,triangles_mem) ||
            index_rearranger.set_arg(2,scene.index_mem())) {
                return -1;
        }
        if (index_rearranger.enqueue_single_dim(triangle_count))
                return -1;
        device.enqueue_barrier();

        ////// Rearrange material map
        if (scene.material_map_mem().copy_to(aux_mem))
                return -1;

        if (map_rearranger.set_arg(0,aux_mem) || 
            map_rearranger.set_arg(1,triangles_mem) ||
            map_rearranger.set_arg(2,scene.material_map_mem())) {
                return -1;
        }
        if (map_rearranger.enqueue_single_dim(triangle_count))
                return -1;
        device.enqueue_barrier();
        ///////////////////////////////////////////////////////////////////////////////
        ////////////////////// 4. Compute treelets and ouput nodes ////////////////////
        ///////////////////////////////////////////////////////////////////////////////
        rt_time_t node_time;
        double node_count_time = 0;
        double treelet_build_time = 0;
        double node_build_time = 0;
        double node_offset_scan_time = 0;
        double segment_offset_scan_time = 0;


        partial_timer.snap_time();

        memory_id splits_id = device.new_memory();
        DeviceMemory& splits_mem = device.memory(splits_id);

        memory_id node_map_id = device.new_memory();
        DeviceMemory& node_map_mem = device.memory(node_map_id);

        memory_id segment_map_id = device.new_memory();
        DeviceMemory& segment_map_mem = device.memory(segment_map_id);

        memory_id segment_heads_id = device.new_memory();
        DeviceMemory& segment_heads_mem = device.memory(segment_heads_id);

        memory_id treelets_id = device.new_memory();
        DeviceMemory& treelets_mem = device.memory(treelets_id);

        memory_id node_count_id = device.new_memory();
        DeviceMemory& node_count_mem = device.memory(node_count_id);

        memory_id node_offsets_id = device.new_memory();
        DeviceMemory& node_offsets_mem = device.memory(node_offsets_id);

        DeviceMemory& nodes_mem = scene.bvh_nodes_mem();

        size_t splits_size = sizeof(morton_code_t) * triangle_count;
        if (splits_mem.initialize(splits_size)) {
                // std::cout << "Error initializing node map" << std::endl;
                return -1;
        }

        size_t node_map_size = sizeof(cl_int) * triangle_count;
        if (node_map_mem.initialize(node_map_size)) {
                // std::cout << "Error initializing node map" << std::endl;
                return -1;
        }

        size_t segment_map_size = sizeof(cl_int) * triangle_count;
        if (segment_map_mem.initialize(segment_map_size)) {
                // std::cout << "Error initializing segment map" << std::endl;
                return -1;
        }

        size_t nodes_mem_size = sizeof(BVHNode) * 2 * triangle_count;
        if (!nodes_mem.valid()) {
                if (nodes_mem.initialize(nodes_mem_size)) 
                        return -1;
        }
        if (nodes_mem.size() < nodes_mem_size) {
                if (nodes_mem.resize(nodes_mem_size))
                    return -1;
        }

        //  Initialize with an empty root node
        BVHNode root_node;
        root_node.m_parent = 0;
        root_node.m_split_axis = 2; //First split axis is x
        if (nodes_mem.write(sizeof(BVHNode), &root_node)) {
                return -1;
        }

        DeviceFunction& splits_build = device.function(splits_build_id);
        DeviceFunction& treelet_maps_init = device.function(treelet_maps_init_id);
        DeviceFunction& segment_map_init = device.function(segment_map_init_id);
        DeviceFunction& segment_heads_init = device.function(segment_heads_init_id);
        DeviceFunction& treelet_init = device.function(treelet_init_id);
        DeviceFunction& treelet_build = device.function(treelet_build_id);
        DeviceFunction& node_build = device.function(node_build_id);
        DeviceFunction& node_counter = device.function(node_counter_id);

        size_t splits_build_group_size = splits_build.max_group_size();
        /* Build splits */
        if (splits_build.set_arg(0,morton_mem) ||
            splits_build.set_arg(1,splits_mem) ||
            splits_build.set_arg(2,triangles_mem) ||
            splits_build.set_arg(3,sizeof(morton_code_t)*(splits_build_group_size+1),NULL)){
                return -1;
        }

        if (splits_build.enqueue_single_dim(triangle_count-1,splits_build_group_size)) {
                return -1;
        }
        device.enqueue_barrier();

        /* Initialize segment map and node map*/
        if (treelet_maps_init.set_arg(0,node_map_mem) ||
            treelet_maps_init.set_arg(1,segment_map_mem)) {
                return -1;
        }

        if (treelet_maps_init.enqueue_single_dim(triangle_count)) {
                return -1;
        }
        device.enqueue_barrier();

        const size_t treelet_levels = 3;
        size_t treelet_size = (1 << treelet_levels) - 1;

        partial_timer.snap_time();
        uint32_t node_count = 1;
        for (uint32_t level = 0; level < 3*M_AXIS_BITS; level += treelet_levels) {
                // std::cout << std::endl << "Starting with level " << level << std::endl;
                if (level) {
                        
                        node_time.snap_time();
                        if (segment_map_init.set_arg(0, node_map_mem) ||
                            segment_map_init.set_arg(1, segment_map_mem)) {
                                return -1;
                        }
                        if (segment_map_init.enqueue_single_dim(triangle_count)) {
                                return -1;
                        }
                        device.enqueue_barrier();
                        
                        if (gpu_scan_uint(device, segment_map_id, 
                                          triangle_count, segment_map_id)){
                                return -1;
                        }

                        segment_offset_scan_time += node_time.msec_since_snap();
                        // std::cout << "Segment map initialized" << std::endl;
                } 

                /*Determine number of segments*/
                uint32_t segment_count;
                if (segment_map_mem.read(sizeof(cl_uint), &segment_count, 
                                         sizeof(cl_int)*(triangle_count-1))) {
                    return -1;
                }
                segment_count++; /* We add one because it starts at 0*/
                // std::cout << "Segment count read: " << segment_count << std::endl;

                /*Reserve memory for segment heads*/
                size_t segment_heads_size = sizeof(cl_uint)*(segment_count+1);
                if (segment_heads_mem.initialize(segment_heads_size)) {
                        return -1;
                }
                // std::cout << "Segment heads memory reserved (" 
                //           << segment_heads_size << ")" << std::endl;
                
                /*Compute segment heads*/
                uint32_t last_head = triangle_count;
                if (segment_heads_init.set_arg(0,segment_map_mem) ||
                    segment_heads_init.set_arg(1,segment_heads_mem) || 
                    segment_heads_init.set_arg(2,sizeof(cl_int), &segment_count) ||
                    segment_heads_init.set_arg(3,sizeof(cl_int), &last_head)) { 
                        return -1;
                }
                if (segment_heads_init.enqueue_single_dim(triangle_count)) {
                        return -1;
                }
                device.enqueue_barrier();
                // std::cout << "Segment heads mem initialized" << std::endl;

                /*Reserve memory for treelets*/
                size_t treelets_mem_size = sizeof(cl_uint) * treelet_size * segment_count;
                if (treelets_mem.initialize(treelets_mem_size)) {
                        return -1;
                }
                // std::cout << "Treelet memory reserved (" 
                //           << treelets_mem_size << ")"
                //           << std::endl;

                /*Reserve memory for node offsets and sums*/
                size_t node_offsets_count = segment_count+1;
                if (node_offsets_mem.initialize(sizeof(cl_uint)*(node_offsets_count))) {
                        return -1;
                }
                if (node_count_mem.initialize(sizeof(cl_uint)*(node_offsets_count))) {
                        return -1;
                }
                // std::cout << "Node offsets and count reserved" << std::endl;

                /*Initialize treelets*/
                if (treelet_init.set_arg(0,treelets_mem)) {
                        return -1;
                }
                if (treelet_init.enqueue_single_dim(segment_count * treelet_size)) {
                        return -1;
                }
                device.enqueue_barrier();
                // std::cout << "Treelets initialized" << std::endl;
                
                node_time.snap_time();
                /*Create treelets*/
                size_t treelet_build_group_size = treelet_build.max_group_size();
                for (uint32_t pass = 0; pass < treelet_levels; ++pass) {
                        uint32_t bit = 3*M_AXIS_BITS - level - pass - 1;
                        uint32_t bitmask = 1<<bit;
                        uint32_t triangle_count_arg = triangle_count;
                        size_t local_code_size=treelet_build_group_size+1;
                        local_code_size *= sizeof(cl_uint)*M_UINT_COUNT;
                        if (treelet_build.set_arg(0, node_map_mem) || 
                            treelet_build.set_arg(1, segment_map_mem) || 
                            treelet_build.set_arg(2, segment_heads_mem) || 
                            treelet_build.set_arg(3, sizeof(cl_uint), &bitmask) || 
                            treelet_build.set_arg(4, sizeof(cl_uint), &triangle_count_arg)||
                            treelet_build.set_arg(5, sizeof(cl_uint), &pass) || 
                            treelet_build.set_arg(6, triangles_mem) || 
                            treelet_build.set_arg(7, morton_mem) || 
                            treelet_build.set_arg(8, splits_mem) || 
                            treelet_build.set_arg(9, treelets_mem)) {
                                return -1;
                        }

                        if (treelet_build.enqueue_single_dim(triangle_count)) {
                                return -1;
                        }
                        device.enqueue_barrier();
                }
                treelet_build_time += node_time.msec_since_snap();

                /*------------- Count nodes to create -------------*/
                node_time.snap_time();
                if (node_counter.set_arg(0, treelets_mem) || 
                    node_counter.set_arg(1, node_count_mem)) {
                        return -1;
                }
                if (node_counter.enqueue_single_dim(segment_count)) {
                        return -1;
                }
                node_count_time += node_time.msec_since_snap();
                // std::cout << "Node counts set" << std::endl;

                node_time.snap_time();

                /* Compute local node prefix sums */
                if (gpu_scan_uint(device, node_count_id, 
                                  node_offsets_count, node_offsets_id)){
                        return -1;
                }

                node_offset_scan_time += node_time.msec_since_snap();

                uint32_t new_node_count;
                if (node_offsets_mem.read(sizeof(cl_uint), &new_node_count,
                                          sizeof(cl_uint) * segment_count)) {
                        return -1;
                }
                // std::cout << "Node offsets computed" << std::endl;
                // std::cout << "New node count: " << new_node_count << std::endl;
                

                node_time.snap_time();

                /* Create BVH nodes from treelets */
                uint32_t triangle_count_arg = triangle_count;
                int32_t last_pass = level+treelet_levels >= 3*M_AXIS_BITS;
                if (node_build.set_arg(0, node_map_mem) || 
                    node_build.set_arg(1, segment_heads_mem) || 
                    node_build.set_arg(2, sizeof(cl_uint), &triangle_count_arg)||
                    node_build.set_arg(3, treelets_mem) || 
                    node_build.set_arg(4, sizeof(cl_uint), &node_count)||
                    node_build.set_arg(5, node_offsets_mem) || 
                    node_build.set_arg(6, nodes_mem) ||
                    node_build.set_arg(7, sizeof(cl_int), &last_pass)) {
                        return -1;
                }
                if (node_build.enqueue_single_dim(segment_count)) {
                        return -1;
                }
                device.enqueue_barrier();
                // std::cout << "Nodes built" << std::endl;
                
                node_build_time += node_time.msec_since_snap();

                /* Release segment heads memory */
                if (segment_heads_mem.release() || 
                    treelets_mem.release() || 
                    node_offsets_mem.release() || 
                    node_count_mem.release()) {
                        return -1;
                }

                node_count += new_node_count;
                // std::cout <<  "Finished level " << level << std::endl;
        }
        if (segment_map_mem.release() || 
            node_map_mem.release())
                return -1;

        // device.finish_commands();
        partial_time_ms = partial_timer.msec_since_snap();
        if (log_partial_time)
                std::cout << "bvh emmission time: " << partial_time_ms << " ms" <<std::endl;

        if (log_partial_time) {
                std::cout << "segment offset scan time: " << segment_offset_scan_time 
                          << std::endl;
                std::cout << "treelet build time: " << treelet_build_time 
                          << std::endl;
                std::cout << "node count time: " << node_count_time 
                          << std::endl;
                std::cout << "node offset scan time: " << node_offset_scan_time 
                          << std::endl;
                std::cout << "node build time: " << node_build_time 
                          << std::endl;
        }

        //////////////////////////////////////////////////////////////////////////
        ////////////////////// 5. Compute node BBoxes ////////////////////////////
        //////////////////////////////////////////////////////////////////////////
        partial_timer.snap_time();

        DeviceFunction& node_bbox_builder = device.function(node_bbox_builder_id);
        DeviceFunction& leaf_bbox_builder = device.function(leaf_bbox_builder_id);

        if (leaf_bbox_builder.set_arg(0,triangles_mem) ||
            leaf_bbox_builder.set_arg(1,bboxes_mem) ||
            leaf_bbox_builder.set_arg(2,nodes_mem)) {
                return -1;
        }
        if (leaf_bbox_builder.enqueue_single_dim(node_count)) {
                return -1;
        }
        device.enqueue_barrier();

        for (int i = 0; i < 3*M_AXIS_BITS ; ++i) {
                if (node_bbox_builder.set_arg(0,nodes_mem)) {
                        return -1;
                }
                if (node_bbox_builder.enqueue_single_dim(node_count)) {
                        return -1;
                }
                device.enqueue_barrier();
        } 
        
        partial_time_ms = partial_timer.msec_since_snap();
        if (log_partial_time)
                std::cout << "Node bbox compute time: " 
                          << partial_time_ms << " ms" << std::endl;
        //////////////////////////////////

        if (device.delete_memory(node_map_id) || 
            device.delete_memory(treelets_id) || 
            device.delete_memory(aux_id) ||
            device.delete_memory(bboxes_mem_id) || 
            device.delete_memory(node_count_id) || 
            device.delete_memory(segment_map_id) || 
            device.delete_memory(node_offsets_id) || 
            device.delete_memory(segment_heads_id) || 
            device.delete_memory(morton_mem_id)) {
                std::cerr << "Error deleting memory" << std::endl;
                return -1;
        }
        ///////////////////////////////

        // std::cout << "Nodes created: " << node_count << std::endl;
        // exit(0);
        if (m_timing) {
                device.finish_commands();
                m_time_ms = m_timer.msec_since_snap();
        }

        scene.m_aggregate_bvh_built = true;
        scene.m_aggregate_bvh_transfered = true;

        return 0;
}
        
void
BVHBuilder::timing(bool b)
{
	m_timing = b;
}

double 
BVHBuilder::get_exec_time()
{
	return m_time_ms;
}


 

