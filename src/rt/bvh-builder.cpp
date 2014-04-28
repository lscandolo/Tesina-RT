#include <rt/bvh-builder.hpp>
#include <gpu/scan.hpp>

#define M_AXIS_BITS  10
#define M_UINT_COUNT 1 //This value needs to be (1+ (M_AXIS_BITS*3-1)/32)

#define MORTON_MAX_F 1024.f // 2^M_AXIS_BITS
#define MORTON_MAX_INV_F 0.000976562.f //1/MORTON_MAX_F

// typedef struct {

//         unsigned int code[M_UINT_COUNT];

// } morton_code_t;

// static size_t pad(size_t size, size_t pad_multiplier) {
//         size_t rem = size%pad_multiplier;
//         if (!rem)
//                 return size;
//         return size + pad_multiplier - rem;
// }

#if 0
static int test_node_2(std::vector<BVHNode>& nodes, 
                       std::vector<int>& prim_checked)
{
        for (uint32_t i = 0; i < nodes.size(); i++) {
                BVHNode& node = nodes[i];
                if (node.m_leaf) {
                        if (node.m_start_index > 12800 ||
                            node.m_end_index   > 12800 ||
                            node.m_start_index > node.m_end_index) {
                                std::cerr << "Error at " << i << "\n";
                                std::cerr << "\tstart_index: " << node.m_start_index  
                                          << "\tend_index: " << node.m_end_index 
                                          << "\n";
                                continue;
                        }
                        for (uint32_t j = node.m_start_index; j < node.m_end_index; j++)
                                prim_checked[j]++;
                }
        }
        return 0;
}

static int test_node(std::vector<BVHNode>& nodes, 
                     int id,
                     int depth,
                     std::vector<int>& prim_checked,
                     std::vector<int>& node_checked) 
{
        if (depth > 32)  {
                std::cout << "Error, trying to access depth " << depth << "\n";
                return -1;
        }

        if (id >= (int)nodes.size()) {
                std::cout << "Error, trying to access node " << id << "\n";
                return -1;
        }
                
        BVHNode& node = nodes[id];
        node_checked[id]++;
        if (node_checked[id] > 1) {
                std::cerr << "Error: checking node " << id 
                          << " " << node_checked[id] << " times\n";
                std::cerr << "Depth: " << depth << "\n";
                return -1;
        }
        if (node.m_leaf) {
                if (node.m_start_index >= prim_checked.size() || 
                    node.m_end_index >  prim_checked.size() ||
                    node.m_start_index > node.m_end_index) {
                        std::cerr << "Error at " << id << "\n";
                        std::cerr << "\tstart_index: " << node.m_start_index  
                                  << "\tend_index: " << node.m_end_index 
                                  << "\n";
                        std::cerr << "Depth: " << depth << "\n";
                        return -1;
                }
                for (uint32_t i = node.m_start_index; i < node.m_end_index; i++) {
                        prim_checked[i]++;
                }
                return 0;
        }

        int res = test_node(nodes,node.m_l_child,depth+1,prim_checked,node_checked);
        res += test_node(nodes,node.m_r_child,depth+1,prim_checked,node_checked);

        if (res)
                return -1;

        if (depth)
                return 0;

        for (size_t i = 0; i < prim_checked.size(); ++i) {
                if (prim_checked[i] != 1) {
                        std::cout << "Error: primitive " << i 
                                  << " checked " << prim_checked[i] << " times\n";
                        return -1;
                }
        }

        for (size_t i = 0; i < node_checked.size(); ++i) {
                if (node_checked[i] != 1) {
                        std::cout << "Error: node " << i 
                                  << " checked " << node_checked[i] << " times\n";
                        return -1;
                }
        }

        return 0;
}
#endif

int32_t 
BVHBuilder::initialize()
{
        DeviceInterface& device = *DeviceInterface::instance();

        if (!device.good())
                return -1;

        triangles_mem_id = device.new_memory();

        outputCounter_mem_id = device.new_memory();
        processedCounter_mem_id = device.new_memory();

        DeviceMemory& OC = device.memory(outputCounter_mem_id);
        DeviceMemory& PC = device.memory(processedCounter_mem_id);

        OC.initialize(64 * sizeof(cl_uint));
        PC.initialize(64 * sizeof(cl_uint));

        std::vector<std::string> builder_names;
        builder_names.push_back("build_primitive_bbox");
        builder_names.push_back("morton_encode_fixed");
        builder_names.push_back("morton_encode");
        builder_names.push_back("rearrange_indices");
        builder_names.push_back("rearrange_map");
        builder_names.push_back("rearrange_morton_codes");
        builder_names.push_back("build_node_bbox");
        builder_names.push_back("build_leaf_bbox");
        builder_names.push_back("max_local_bbox");
        builder_names.push_back("process_task");
        
        std::vector<function_id> builder_ids = 
                device.build_functions("src/kernel/bvh-builder.cl", builder_names);
        if (!builder_ids.size())
                return -1;

        int id = 0;
        primitive_bbox_builder_id = builder_ids[id++];
        morton_encoder_fixed_id   = builder_ids[id++];
        morton_encoder_id         = builder_ids[id++];
        index_rearranger_id       = builder_ids[id++];
        map_rearranger_id         = builder_ids[id++];
        morton_rearranger_id      = builder_ids[id++];
        node_bbox_builder_id      = builder_ids[id++];
        leaf_bbox_builder_id      = builder_ids[id++];
        max_local_bbox_id         = builder_ids[id++];
        process_task_id           = builder_ids[id++];

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

        node_count = 0;
        bvh_depth = 32;
        bvh_min_leaf_size = 1;
        bvh_created = false;

	m_timing = true;
        m_logging = false;
        m_log = NULL;
        return 0;
}


void 
BVHBuilder::update_configuration(const RendererConfig& conf)
{
        bvh_depth = std::min(std::max(conf.bvh_depth, 1), 64);
        bvh_min_leaf_size = std::min(std::max(conf.bvh_min_leaf_size, 1), 256);
}

int32_t
BVHBuilder::build_lbvh(Scene& scene, size_t cq_i)
{
        
        /*  New Pseudocode: 
         *
         * 1. Compute BBox for each primitive
         *
         * 2. Create morton code encoding of barycenters
         *
         * 3. Sort the primitives by their morton code
         *
         * 4. Compute BVH nodes using work queues
         * 
         * 5. Compute node BBoxes
         * 
         * 6. Voila
         *
         */
        
        if (m_timing)
                m_timer.snap_time();
        
        DeviceInterface& device = *DeviceInterface::instance();
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
        if (primitive_bbox_builder.enqueue_simple(triangle_count, cq_i)) {
                std::cout << "Failed at primitive_bbox_builder " << std::endl; 
                return -1;
        }
        device.enqueue_barrier(cq_i);

        if (m_logging && m_log != NULL) { 
                device.finish_commands(cq_i);
                partial_time_ms = partial_timer.msec_since_snap();
                (*m_log) << "-----BVH construction times: " << "\n";
                (*m_log) << "\tBBox builder time: " << partial_time_ms << " ms\n";
        }
        
        //////////////////////////////////////////////////////////////////////////
        ///////////// 2. Create morton code encoding of barycenters //////////////
        //////////////////////////////////////////////////////////////////////////
        partial_timer.snap_time();
        memory_id morton_mem_id = device.new_memory();
        DeviceMemory& morton_mem = device.memory(morton_mem_id);

        size_t morton_size = sizeof(cl_uint2) * triangle_count * 2;
        if (!morton_mem.valid())
                if (morton_mem.initialize(morton_size))
                        return -1;

        if (morton_mem.size() < morton_size)
                if (morton_mem.resize(morton_size))
                        return -1;
                    
        /////////////// Computing global bbox
        DeviceFunction& max_local_bbox = device.function(max_local_bbox_id);
        size_t group_size  = max_local_bbox.max_group_size();
        size_t bbox_count = triangle_count;
        size_t out_bbox_count = ceil(bbox_count/(2.*group_size));
        std::vector<memory_id> aux_bbox_mems;
        memory_id bbox_in_mem_id  = bboxes_mem_id;
        memory_id bbox_out_mem_id;
        size_t real_size = ceil(bbox_count/2.);
        size_t global_size = ceil(real_size/(double)group_size) * group_size;
        while (true) {
                bbox_out_mem_id  = device.new_memory();
                DeviceMemory& bbox_in_mem = device.memory(bbox_in_mem_id);
                DeviceMemory& bbox_out_mem = device.memory(bbox_out_mem_id);
                if (bbox_out_mem.initialize(out_bbox_count * sizeof(BBox))) {
                        return -1;
                }

                if (max_local_bbox.set_arg(0,bbox_in_mem) ||
                    max_local_bbox.set_arg(1,2 * group_size * sizeof(BBox), NULL) ||
                    max_local_bbox.set_arg(2, sizeof(cl_uint), &bbox_count) ||
                    max_local_bbox.set_arg(3,bbox_out_mem))
                        return -1;

                if (max_local_bbox.enqueue_single_dim(global_size,group_size, 0, cq_i)) {
                        std::cout << "Failed at max_local_bbox " << std::endl; 
                        return -1;
                }
                device.enqueue_barrier(cq_i);
                    
                bbox_count = out_bbox_count;
                out_bbox_count = ceil(bbox_count/(2.*group_size));
                aux_bbox_mems.push_back(bbox_out_mem_id);
                bbox_in_mem_id = bbox_out_mem_id;

                if (global_size <= group_size)
                        break;

                real_size = ceil(bbox_count/2.);
                global_size = ceil(real_size/(double)group_size) * group_size;
        }


        for (size_t i = 0; i < aux_bbox_mems.size()-1; ++i) {
                device.delete_memory(aux_bbox_mems[i]);
        }

        memory_id last_bbox_out_id = aux_bbox_mems[aux_bbox_mems.size()-1];
        DeviceMemory& global_bbox_mem = device.memory(last_bbox_out_id);

        DeviceFunction& morton_encoder = device.function(morton_encoder_id);
        if (morton_encoder.set_arg(0,bboxes_mem) ||
            morton_encoder.set_arg(1,global_bbox_mem) ||
            morton_encoder.set_arg(2,morton_mem) ||
            morton_encoder.set_arg(3,sizeof(cl_int), &bvh_depth))
                return -1;
        if (morton_encoder.enqueue_simple(triangle_count, cq_i)) {
                std::cout << "Failed at morton encoder " << std::endl; 
                return -1;
        }
        device.enqueue_barrier(cq_i);
        device.delete_memory(last_bbox_out_id);
        if (m_logging && m_log != NULL) {
                device.finish_commands(cq_i);
                partial_time_ms = partial_timer.msec_since_snap();
                (*m_log) << "\tMorton encoder time: " << partial_time_ms << " ms\n";
        }

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
                                
                                if (morton_sorter_16.enqueue_simple(ms16_work_items, cq_i)){
                                        std::cout << "Failed at morton sorter_16" 
                                                  << std::endl; 
                                        exit(1);
                                }
                                device.enqueue_barrier(cq_i);

                        } else if (inc > 2) {
                                inc >>=2;
                                morton_sorter_8.set_arg(2,sizeof(int),&inc);
                                morton_sorter_8.set_arg(3,sizeof(int),&dir);
                                morton_sorter_8.set_arg(4,sizeof(int),&inv);
                                morton_sorter_8.set_arg(5,sizeof(int),&last_step);
                                morton_sorter_8.set_arg(6,sizeof(int),&morton_sort_size);
                                
                                if (morton_sorter_8.enqueue_simple(ms8_work_items, cq_i)) {
                                        std::cout << "Failed at morton sorter_8" 
                                                  << std::endl; 
                                        exit(1);
                                }
                                device.enqueue_barrier(cq_i);

                        } else if (inc > 1) {
                                inc>>=1;
                                morton_sorter_4.set_arg(2,sizeof(int),&inc);
                                morton_sorter_4.set_arg(3,sizeof(int),&dir);
                                morton_sorter_4.set_arg(4,sizeof(int),&inv);
                                morton_sorter_4.set_arg(5,sizeof(int),&last_step);
                                morton_sorter_4.set_arg(6,sizeof(int),&morton_sort_size);
                                
                                if (morton_sorter_4.enqueue_simple(ms4_work_items, cq_i)) {
                                        std::cout << "Failed at morton sorter_4" 
                                                  << std::endl; 
                                        exit(1);
                                }
                                device.enqueue_barrier(cq_i);
                        } else {
                                morton_sorter_2.set_arg(2,sizeof(int),&inc);
                                morton_sorter_2.set_arg(3,sizeof(int),&dir);
                                morton_sorter_2.set_arg(4,sizeof(int),&inv);
                                morton_sorter_2.set_arg(5,sizeof(int),&last_step);
                                morton_sorter_2.set_arg(6,sizeof(int),&morton_sort_size);
                                
                                if (morton_sorter_2.enqueue_simple(ms2_work_items, cq_i)) {
                                        std::cout << "Failed at morton sorter_2" 
                                                  << std::endl; 
                                        exit(1);
                                }
                                device.enqueue_barrier(cq_i);
                        }
                }
        }
        // device.finish_commands(cq_i);
        if (m_logging && m_log != NULL) {
                device.finish_commands(cq_i);
                partial_time_ms = partial_timer.msec_since_snap();
                (*m_log) << "\tMorton sorter: " << partial_time_ms << " ms" << "\n";
        }

        ///////////////////////////////////////////////////////////////////////////////
        ////////////////////// 3.2 Rearrange indices and map //////////////////////////
        ///////////////////////////////////////////////////////////////////////////////
        //////////////////////////////////
        DeviceFunction& index_rearranger = device.function(index_rearranger_id);
        DeviceFunction& map_rearranger   = device.function(map_rearranger_id);
        DeviceFunction& morton_rearranger   = device.function(morton_rearranger_id);

        ////// Rearrange indices
        memory_id aux_id = device.new_memory();
        DeviceMemory& aux_mem = device.memory(aux_id);
        size_t aux_mem_size = scene.index_mem().size();
        aux_mem_size = std::max(aux_mem_size, bboxes_mem.size());
        aux_mem_size = std::max(aux_mem_size, scene.material_map_mem().size());
        aux_mem_size = std::max(aux_mem_size, morton_mem.size());

        if (aux_mem.initialize(aux_mem_size))
                return -1;

        if (scene.index_mem().copy_all_to(aux_mem, cq_i)) {
                std::cout << "Error copying " << "\n";
                return -1;
        }
        if (index_rearranger.set_arg(0,aux_mem) || 
            index_rearranger.set_arg(1,triangles_mem) ||
            index_rearranger.set_arg(2,scene.index_mem())) {
                return -1;
        }
        if (index_rearranger.enqueue_simple(triangle_count, cq_i)) {
                std::cout << "Failed at index rearranger" << std::endl; 
                return -1;
        }
        device.enqueue_barrier(cq_i);

        ////// Rearrange material map
        if (scene.material_map_mem().copy_all_to(aux_mem, cq_i)) {
                std::cout << "Error copying " << "\n";
                return -1;
        }

        if (map_rearranger.set_arg(0,aux_mem) || 
            map_rearranger.set_arg(1,triangles_mem) ||
            map_rearranger.set_arg(2,scene.material_map_mem())) {
                return -1;
        }
        if (map_rearranger.enqueue_simple(triangle_count, cq_i)) {
                std::cout << "Failed at map rearranger" << std::endl; 
                return -1;
        }
        device.enqueue_barrier(cq_i);

        if (morton_mem.copy_all_to(aux_mem, cq_i)) {
                std::cout << "Error copying " << "\n";
                return -1;
        }
        if (morton_rearranger.set_arg(0,aux_mem) || 
            morton_rearranger.set_arg(1,triangles_mem) ||
            morton_rearranger.set_arg(2,morton_mem)) {
                return -1;
        }
        if (morton_rearranger.enqueue_simple(triangle_count, cq_i)) {
                std::cout << "Failed at morton rearranger" << std::endl; 
                return -1;
        }
        device.enqueue_barrier(cq_i);
        device.delete_memory(aux_id);

        ///////////////////////////////////////////////////////////////////////////////
        ////////////////////// 4. Create bvh nodes ////////////////////////////////////
        ///////////////////////////////////////////////////////////////////////////////
        rt_time_t node_time;

        partial_timer.snap_time();

        DeviceMemory& nodes_mem = scene.bvh_nodes_mem();

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
        root_node.m_split_axis = 0; //First split axis is x
        root_node.m_start_index = 0;
        root_node.m_end_index = triangle_count;
        root_node.m_leaf = 0; // Because we use leaf to encode the level at which we check
        if (nodes_mem.write(sizeof(BVHNode), &root_node, cq_i)) {
                return -1;
        }
        device.enqueue_barrier(cq_i);

        DeviceFunction& process_tasks = device.function(process_task_id);

        for (int i = 0; i < 64; ++i) {
                processedCounter[i] = 0;
                outputCounter[i] = 0;
        }

        DeviceMemory& outputCounter_mem = device.memory(outputCounter_mem_id);
        outputCounter_mem.write(64 * sizeof(cl_uint), outputCounter, cq_i);

        DeviceMemory& processedCounter_mem = device.memory(processedCounter_mem_id);
        processedCounter_mem.write(64 * sizeof(cl_uint), processedCounter, cq_i);

        size_t gsize = process_tasks.max_group_size();
        node_time.snap_time();

        if (process_tasks.set_arg(0, outputCounter_mem) ||
            process_tasks.set_arg(1, processedCounter_mem) ||
            process_tasks.set_arg(2, nodes_mem) ||
            process_tasks.set_arg(3, morton_mem) ||
            process_tasks.set_arg(4, sizeof(cl_int) * (gsize * 2 + 1), NULL) ||
            process_tasks.set_arg(5, sizeof(cl_int) * (gsize * 2 + 1), NULL) ||
            process_tasks.set_arg(6, sizeof(cl_uint), &bvh_depth) ||
            process_tasks.set_arg(7, sizeof(cl_uint), &bvh_min_leaf_size)) {
                std::cout << "Error setting arguments for process_tasks\n";
                return -1;
        }

        // std::cout << "bvh mem size: " << nodes_mem.size() << "\n";
        // std::cout << "OC size: " << OC.size() << "\n";
        // std::cout << "PC size: " << PC.size() << "\n";
        // std::cout << "morton_mem size: " << morton_mem.size() << "\n";

        int compute_units = CLInfo::instance()->max_compute_units;

        for (cl_uint i = 0; i < bvh_depth; ++i) {

                if (process_tasks.set_arg(8, sizeof(int), &i)) {
                        std::cout << "Error setting arguments for process_tasks\n";
                        return -1;
                }

                size_t max_size = powf(2, i);
                size_t global_size = gsize;
                while (global_size < max_size && global_size < gsize * compute_units - 1)
                        global_size += gsize;

                if (process_tasks.enqueue_single_dim(global_size,gsize, 0, cq_i)) {
                        std::cout << "Error executing process_tasks\n";
                        return -1;
                }
                device.enqueue_barrier(cq_i);
        }

        outputCounter_mem.read(sizeof(int) * bvh_depth, outputCounter, 0, cq_i);
        node_count = 1; // We count the root!

        // PC.read(sizeof(int) * bvh_depth, pc, 0, cq_i);

        processedCounter[0] = 0;
        for (cl_uint i = 0; i < bvh_depth; ++i) {
                // std::cout << "OC[" << i << "]: " << oc[i] << "\t";
                processedCounter[i] = node_count;
                node_count += outputCounter[i];
                // std::cout << "PC[" << i << "]: " << pc[i] << "\n";
        }
 
        // std::cout << "Nodes: " << node_count << "\n";
        
        // std::vector<BVHNode> nodes(node_count);
        // nodes_mem.read(sizeof(BVHNode) * node_count, &(nodes[0]), 0, cq_i);

        // std::vector<int> prim_check(triangle_count,0);
        // std::vector<int> node_check(node_count,0);

        // if (test_node(nodes, 0, 0, prim_check, node_check))
        //         std::cerr << "TEST NODES FAILED\n";
        // else
        //         std::cerr << "TEST NODES SUCCEDED!!\n";

        // for (int i = 0; i < 32; ++i) {
        //         std::cout << "Node " << i << ": " 
        //                   << (nodes[i].m_leaf? "(leaf)   \t" : "(branch)\t")
        //                   << nodes[i].m_l_child << " - " 
        //                   << nodes[i].m_r_child << "\n";
        // }        

        // unsigned int max_node_tris = 0;
        // for (int i = 0; i < node_count; ++i) {
        //         if (!nodes[i].m_leaf)
        //                 continue;
        //         max_node_tris = std::max(max_node_tris, 
        //                                  nodes[i].m_end_index - nodes[i].m_start_index);

        // }
        // std::cout << "MAX TRIS: " << max_node_tris;

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

        if (leaf_bbox_builder.enqueue_simple(node_count, cq_i)) {
                std::cout << "Failed at leaf bbox builder" << std::endl; 
                return -1;
        }
        device.enqueue_barrier(cq_i);

        if (node_bbox_builder.set_arg(0,nodes_mem)) {
                std::cout << "Failed at node bbox builder iteration 0" << std::endl; 
                return -1;
        }
        device.enqueue_barrier(cq_i);

        for (int32_t i = bvh_depth-1; i >= 0; --i) {
                size_t start_node = i ? processedCounter[i-1] : 0;
                size_t count = processedCounter[i] - start_node;
                if (!count)
                        continue;

                if (node_bbox_builder.enqueue_single_dim(count, 0,start_node, cq_i)) {
                        std::cout << "Failed at node bbox builder iteration " 
                                  << i << std::endl; 
                        return -1;
                }
                device.enqueue_barrier(cq_i);
        }

        // // One last time for the root
        // if (node_bbox_builder.enqueue_single_dim(1, 0, 0, cq_i)) {
        //         return -1;
        // }
        // device.enqueue_barrier(cq_i);

        // if (m_logging && m_log != NULL) {
        //         device.finish_commands(cq_i);
        //         partial_time_ms = partial_timer.msec_since_snap();
        //         (*m_log) << "\tNode bbox compute time: " 
        //                   << partial_time_ms << " ms\n" ;
        // }
        //////////////////////////////////

        if (device.delete_memory(bboxes_mem_id)) {
                std::cerr << "Error deleting bboxes memory\n";
                return -1;
        }

        if (device.delete_memory(morton_mem_id)) {
                std::cerr << "Error deleting morton memory\n";
                return -1;
        }
        ///////////////////////////////

        // std::cout << "Nodes created: " << node_count << "\n";
        // exit(0);
        if (m_timing) {
                device.finish_commands(cq_i);
                m_time_ms = m_timer.msec_since_snap();
        }

        scene.m_accelerator_type = LBVH_ACCELERATOR;
        scene.m_aggregate_bvh_built = true;
        scene.m_aggregate_bvh_transfered = true;

        scene.bvh_node_count = node_count;
        for (int i = 0; i < 64; ++i) {
                scene.bvh_level_node_count[i] = outputCounter[i];
        }

        bvh_created = true;

        return 0;
}
        
int32_t BVHBuilder::refit_lbvh(Scene& scene, size_t cq_i)
{

        if (m_timing)
                m_timer.snap_time();
        
        DeviceInterface& device = *DeviceInterface::instance();
        double    partial_time_ms;
        rt_time_t partial_timer;
        size_t triangle_count = scene.triangle_count();

        //////////////////////////////////////////////////////////////////////////
        //////////   Compute BBox for each primitive //////////////////////////
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
        if (primitive_bbox_builder.enqueue_simple(triangle_count, cq_i)) {
                std::cout << "Failed at primitive_bbox_builder " << std::endl; 
                return -1;
        }
        device.enqueue_barrier(cq_i);

        if (m_logging && m_log != NULL) { 
                device.finish_commands(cq_i);
                partial_time_ms = partial_timer.msec_since_snap();
                (*m_log) << "-----BVH refit times: " << "\n";
                (*m_log) << "\tBBox builder time: " << partial_time_ms << " ms\n";
        }

        //////////////////////////////////////////////////////////////////////////
        //////////   Compute BBox for nodes //////////////////////////
        //////////////////////////////////////////////////////////////////////////

        DeviceFunction& node_bbox_builder = device.function(node_bbox_builder_id);
        DeviceFunction& leaf_bbox_builder = device.function(leaf_bbox_builder_id);

        DeviceMemory& nodes_mem = scene.bvh_nodes_mem();

        if (leaf_bbox_builder.set_arg(0,triangles_mem) ||
            leaf_bbox_builder.set_arg(1,bboxes_mem) ||
            leaf_bbox_builder.set_arg(2,nodes_mem)) {
                return -1;
        }

        if (leaf_bbox_builder.enqueue_simple(node_count, cq_i)) {
                std::cout << "Failed at leaf bbox builder" << std::endl; 
                return -1;
        }
        device.enqueue_barrier(cq_i);

        if (node_bbox_builder.set_arg(0,nodes_mem)) {
                std::cout << "Failed at node bbox builder iteration 0" << std::endl; 
                return -1;
        }
        device.enqueue_barrier(cq_i);

        for (int32_t i = bvh_depth-1; i >= 0; --i) {
                size_t start_node = i ? processedCounter[i-1] : 0;
                size_t count = processedCounter[i] - start_node;
                if (!count)
                        continue;

                if (node_bbox_builder.enqueue_single_dim(count, 0,start_node, cq_i)) {
                        std::cout << "Failed at node bbox builder iteration " 
                                  << i << std::endl; 
                        return -1;
                }
                device.enqueue_barrier(cq_i);
        }

        if (device.delete_memory(bboxes_mem_id)) {
                std::cerr << "Error deleting bboxes memory\n";
                return -1;
        }

        if (m_timing) {
                device.finish_commands(cq_i);
                m_time_ms = m_timer.msec_since_snap();
        }

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


void BVHBuilder::set_log(Log* log)
{
        m_log = log;
}
 
void BVHBuilder::logging(bool l)
{
        m_logging = l;
}

