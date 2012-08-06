#include <rt/tracer.hpp>

Tracer::Tracer()
{
        m_initialized = false;
}

int32_t Tracer::initialize()
{

        DeviceInterface& device = *DeviceInterface::instance();
        if (!device.good())
                return -1;

        /* ---------- BVH ray tracing ------------*/
        std::vector<std::string> bvh_kernel_names;
        bvh_kernel_names.push_back("trace_single");
        bvh_kernel_names.push_back("trace_multi");

        std::vector<function_id> bvh_function_ids;
        bvh_function_ids = device.build_functions("src/kernel/trace-bvh.cl", 
                                                  bvh_kernel_names);
        if (!bvh_function_ids.size())
                return -1;

        bvh_single_tracer_id = bvh_function_ids[0];
        bvh_multi_tracer_id = bvh_function_ids[1];
        
        /* ---------- BVH shadow ray tracing ------------*/
        std::vector<std::string> bvh_shadow_kernel_names;
        bvh_shadow_kernel_names.push_back("shadow_trace_single");
        bvh_shadow_kernel_names.push_back("shadow_trace_multi");

        std::vector<function_id> bvh_shadow_function_ids;
        bvh_shadow_function_ids = device.build_functions("src/kernel/shadow-trace-bvh.cl", 
                                                         bvh_shadow_kernel_names);
        if (!bvh_shadow_function_ids.size())
                return -1;

        bvh_single_shadow_id = bvh_shadow_function_ids[0];
        bvh_multi_shadow_id  = bvh_shadow_function_ids[1];

        /* ------------------- KDTree kernels ----------------- */

        /* Single root functions */
        kdt_single_tracer_id = device.new_function();
        DeviceFunction& kdt_single_tracer = device.function(kdt_single_tracer_id);

        if (kdt_single_tracer.initialize("src/kernel/trace-kdt.cl", "trace_single"))
                return -1;

        kdt_single_tracer.set_dims(1);

        kdt_single_shadow_id = device.new_function();
        DeviceFunction& kdt_single_shadow = device.function(kdt_single_shadow_id);
        if (kdt_single_shadow.initialize("src/kernel/shadow-trace-kdt.cl", 
                                         "shadow_trace_single"))
                return -1;
        
	kdt_single_shadow.set_dims(1);
        /*----------------------------*/


        /* Multi root functions  ( NOT IMPLEMENTED )*/
/*      kdt_multi_tracer_id = device.new_function();
        DeviceFunction& kdt_multi_tracer = device.function(kdt_multi_tracer_id);

        if (kdt_multi_tracer.initialize("src/kernel/trace-kdt.cl", "???"))
                return -1;

        kdt_multi_tracer.set_dims(1);

        kdt_multi_shadow_id = device.new_function();
        DeviceFunction& kdt_multi_shadow = device.function(kdt_multi_shadow_id);
        if (multi_shadow.initialize("src/kernel/shadow-trace-kdt.cl", "???"))
                return -1;
        
	kdt_multi_shadow.set_dims(1);
*/
        /*----------------------------*/


	m_timing = false;
        m_initialized = true;
	return 0;
}


int32_t 
Tracer::trace(Scene& scene, DeviceMemory& bvh_mem, int32_t ray_count, 
	      RayBundle& rays, HitBundle& hits, bool secondary)
{
        DeviceInterface& device = *DeviceInterface::instance();
        if (!m_initialized || !device.good())
                return -1;
        
        function_id tracer_id = scene.root_count()>1?bvh_multi_tracer_id:bvh_single_tracer_id;
        DeviceFunction& tracer = device.function(tracer_id);

	if (m_timing)
		m_tracer_timer.snap_time();

        if (tracer.set_arg(0,hits.mem()))
                    return -1;

        if (tracer.set_arg(1,rays.mem()))
                    return -1;

        if (tracer.set_arg(2, scene.vertex_mem()))
                    return -1;

        if (tracer.set_arg(3, scene.index_mem()))
                    return -1;

        if (tracer.set_arg(4, bvh_mem))
                    return -1;

        if (tracer.set_arg(5, scene.bvh_roots_mem()))
                    return -1;

        cl_int root_cant = scene.root_count();
        if (tracer.set_arg(6, sizeof(cl_int), &root_cant))
                    return -1;

        size_t group_size = tracer.max_group_size();
        if (secondary)
                group_size = std::min(RT::BVH_SECONDARY_GROUP_SIZE, group_size);
        if (tracer.enqueue_single_dim(ray_count, group_size))
                return -1;
        device.enqueue_barrier();

        if (m_timing) {
                device.finish_commands();
		m_tracer_time_ms = m_tracer_timer.msec_since_snap();
        }

	return 0;
}

int32_t 
Tracer::trace(Scene& scene, int32_t ray_count, 
	      RayBundle& rays, HitBundle& hits, bool secondary)
{
        DeviceInterface& device = *DeviceInterface::instance();
        if (!m_initialized || !device.good() || !scene.ready())
                return -1;
        switch (scene.get_accelerator_type()) {
        case (KDTREE_ACCELERATOR):
                return trace_kdtree(scene, ray_count, rays, hits, secondary);
        case (BVH_ACCELERATOR):
                return trace_bvh(scene, ray_count, rays, hits, secondary);
        default:
                return -1;
        }
}

int32_t 
Tracer::trace_kdtree(Scene& scene, int32_t ray_count, 
                     RayBundle& rays, HitBundle& hits, bool secondary)
{
        function_id tracer_id;
        DeviceInterface& device = *DeviceInterface::instance();

        if (scene.root_count() == 1)
                tracer_id = kdt_single_tracer_id;
        else /* NOT IMPLEMENTED */
                return -1;

        DeviceFunction& tracer = device.function(tracer_id);

	if (m_timing)
		m_tracer_timer.snap_time();

        if (tracer.set_arg(0,hits.mem()))
                    return -1;

        if (tracer.set_arg(1,rays.mem()))
                    return -1;

        if (tracer.set_arg(2, scene.vertex_mem()))
                    return -1;

        if (tracer.set_arg(3, scene.index_mem()))
                    return -1;

        if (tracer.set_arg(4, scene.kdtree_nodes_mem()))
                    return -1;

        if (tracer.set_arg(5, scene.kdtree_leaf_tris_mem()))
                    return -1;

        BBox scene_bbox = scene.bbox();
        if (tracer.set_arg(6, sizeof(BBox), &scene_bbox))
                return -1;

        size_t group_size = tracer.max_group_size();
        if (secondary)
                group_size = std::min(RT::KDT_SECONDARY_GROUP_SIZE, group_size);

        if (tracer.enqueue_single_dim(ray_count, group_size))
                return -1;
        device.enqueue_barrier();

        if (m_timing) {
                device.finish_commands();
		m_tracer_time_ms = m_tracer_timer.msec_since_snap();
        }

	return 0;
}

int32_t 
Tracer::trace_bvh(Scene& scene, int32_t ray_count, 
                  RayBundle& rays, HitBundle& hits, bool secondary)
{
        function_id tracer_id;
        DeviceInterface& device = *DeviceInterface::instance();

        if (scene.root_count() == 1)
                tracer_id = bvh_single_tracer_id;
        else 
                tracer_id = bvh_multi_tracer_id;

        DeviceFunction& tracer = device.function(tracer_id);

	if (m_timing)
		m_tracer_timer.snap_time();

        if (tracer.set_arg(0,hits.mem()))
                    return -1;

        if (tracer.set_arg(1,rays.mem()))
                    return -1;

        if (tracer.set_arg(2, scene.vertex_mem()))
                    return -1;

        if (tracer.set_arg(3, scene.index_mem()))
                    return -1;

        if (tracer.set_arg(4, scene.bvh_nodes_mem()))
                    return -1;

        if (scene.root_count() > 1) {

                if (tracer.set_arg(5, scene.bvh_roots_mem()))
                        return -1;

                cl_int root_cant = scene.root_count();
                if (tracer.set_arg(6, sizeof(cl_int), &root_cant))
                        return -1;
        }

        size_t group_size = tracer.max_group_size();
        if (secondary)
                group_size = std::min(RT::BVH_SECONDARY_GROUP_SIZE, group_size);

        if (tracer.enqueue_single_dim(ray_count, group_size))
                return -1;
        device.enqueue_barrier();

        if (m_timing) {
                device.finish_commands();
                m_tracer_time_ms = m_tracer_timer.msec_since_snap();
        }

	return 0;
}

/*///////////////////////// Shadow ray tracing //////////////////////////////////////*/

int32_t 
Tracer::shadow_trace(Scene& scene, int32_t ray_count, 
		     RayBundle& rays, HitBundle& hits, bool secondary)
{
        DeviceInterface& device = *DeviceInterface::instance();
        if (!m_initialized || !device.good() || !scene.ready())
                return -1;

        switch (scene.get_accelerator_type()) {
        case (KDTREE_ACCELERATOR):
                return shadow_trace_kdtree(scene, ray_count, rays, hits, secondary);
        case (BVH_ACCELERATOR):
                return shadow_trace_bvh(scene, ray_count, rays, hits, secondary);
        default:
                return -1;
        }
}

int32_t 
Tracer::shadow_trace_bvh(Scene& scene, int32_t ray_count, 
                         RayBundle& rays, HitBundle& hits, bool secondary)
{
        function_id shadow_id;
        DeviceInterface& device = *DeviceInterface::instance();
        if (scene.root_count() == 1)
                shadow_id = bvh_single_shadow_id;
        else
                shadow_id = bvh_multi_shadow_id;
                
        DeviceFunction& shadow = device.function(shadow_id);

	if (m_timing)
		m_shadow_timer.snap_time();

        if (shadow.set_arg(0, hits.mem()))
                return -1;

        if (shadow.set_arg(1, rays.mem()))
                return -1;

        if (shadow.set_arg(2, scene.vertex_mem()))
                return -1;

        if (shadow.set_arg(3, scene.index_mem()))
                return -1;

        if (shadow.set_arg(4, scene.bvh_nodes_mem()))
                return -1;

        if (shadow.set_arg(5, scene.lights_mem()))
                return -1;

        if (scene.root_count() > 1) {

                if (shadow.set_arg(6, scene.bvh_roots_mem()))
                        return -1;

                cl_int root_count = scene.root_count();
                if (shadow.set_arg(7, sizeof(cl_int), &root_count))
                        return -1;
        }

        size_t group_size = shadow.max_group_size();
        if (secondary)
                group_size = std::min(RT::BVH_SECONDARY_GROUP_SIZE, group_size);

        if (shadow.enqueue_single_dim(ray_count, group_size))
                return -1;
        device.enqueue_barrier();

        if (m_timing) {
                device.finish_commands();
		m_shadow_time_ms = m_shadow_timer.msec_since_snap();
        }


	return 0;

}

int32_t
Tracer::shadow_trace(Scene& scene, DeviceMemory& bvh_mem, int32_t ray_count, 
		     RayBundle& rays, HitBundle& hits, bool secondary)
{
        DeviceInterface& device = *DeviceInterface::instance();
        if (!m_initialized || !device.good())
                return -1;

        function_id shadow_id = 
                scene.root_count()>1?bvh_multi_shadow_id:bvh_single_shadow_id;
        DeviceFunction& shadow = device.function(shadow_id);

	if (m_timing)
		m_shadow_timer.snap_time();

        if (shadow.set_arg(0, hits.mem()))
                return -1;

        if (shadow.set_arg(1, rays.mem()))
                return -1;

        if (shadow.set_arg(2, scene.vertex_mem()))
                return -1;

        if (shadow.set_arg(3, scene.index_mem()))
                return -1;

        cl_int root_count = scene.root_count();
        if (shadow.set_arg(4, sizeof(cl_int), &root_count))
                return -1;

        if (shadow.set_arg(5, scene.bvh_roots_mem()))
                return -1;

        if (shadow.set_arg(6, bvh_mem))
                return -1;

        if (shadow.set_arg(7, scene.lights_mem()))
                return -1;

        size_t group_size = shadow.max_group_size();
        if (secondary)
                group_size = std::min(RT::BVH_SECONDARY_GROUP_SIZE, group_size);

        if (shadow.enqueue_single_dim(ray_count, group_size))
                return -1;
        device.enqueue_barrier();

        if (m_timing) {
                device.finish_commands();
		m_shadow_time_ms = m_shadow_timer.msec_since_snap();
        }

	return 0;
}

int32_t 
Tracer::shadow_trace_kdtree(Scene& scene, int32_t ray_count, 
                            RayBundle& rays, HitBundle& hits, bool secondary)
{
        function_id shadow_id;
        DeviceInterface& device = *DeviceInterface::instance();

        if (scene.root_count() == 1)
                shadow_id = kdt_single_shadow_id;
        else /* NOT IMPLEMENTED */
                return -1;

        DeviceFunction& shadow = device.function(shadow_id);

	if (m_timing)
		m_shadow_timer.snap_time();

        if (shadow.set_arg(0,hits.mem()))
                    return -1;

        if (shadow.set_arg(1,rays.mem()))
                    return -1;

        if (shadow.set_arg(2, scene.vertex_mem()))
                    return -1;

        if (shadow.set_arg(3, scene.index_mem()))
                    return -1;

        if (shadow.set_arg(4, scene.kdtree_nodes_mem()))
                    return -1;

        if (shadow.set_arg(5, scene.kdtree_leaf_tris_mem()))
                    return -1;

        BBox scene_bbox = scene.bbox();
        if (shadow.set_arg(6, sizeof(BBox), &scene_bbox))
                return -1;

        if (shadow.set_arg(7, scene.lights_mem()))
                return -1;

        size_t group_size = shadow.max_group_size();
        if (secondary)
                group_size = std::min(RT::KDT_SECONDARY_GROUP_SIZE, group_size);

        if (shadow.enqueue_single_dim(ray_count, group_size))
                return -1;
        device.enqueue_barrier();

        if (m_timing) {
                device.finish_commands();
		m_shadow_time_ms = m_shadow_timer.msec_since_snap();
        }

	return 0;
}

void
Tracer::timing(bool b)
{
	m_timing = b;
}

double 
Tracer::get_trace_exec_time()
{
	return m_tracer_time_ms;
}

double 
Tracer::get_shadow_exec_time()
{
	return m_shadow_time_ms;
}

