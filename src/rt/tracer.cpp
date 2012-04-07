#include <iostream> //!!
#include <rt/tracer.hpp>

Tracer::Tracer()
{
        m_initialized = false;
}

int32_t Tracer::initialize(const CLInfo& clinfo)
{

        if (device.initialize(clinfo))
                return -1;

        tracer_id = device.new_function();
        DeviceFunction& tracer = device.function(tracer_id);

        if (tracer.initialize("src/kernel/trace.cl", "trace"))
                return -1;

        tracer.set_dims(1);

        shadow_id = device.new_function();
        DeviceFunction& shadow = device.function(shadow_id);
        if (shadow.initialize("src/kernel/shadow-trace.cl", "shadow_trace"))
                return -1;
        
	shadow.set_dims(1);

	m_timing = false;
        m_initialized = true;
	return 0;
}


int32_t 
Tracer::trace(Scene& scene, DeviceMemory& bvh_mem, int32_t ray_count, 
	      RayBundle& rays, HitBundle& hits, bool secondary)
{
        if (!m_initialized || !device.good())
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

        if (tracer.set_arg(3, scene.triangle_mem()))
                    return -1;

        if (tracer.set_arg(4, bvh_mem))
                    return -1;

        if (tracer.set_arg(5, scene.bvh_roots_mem()))
                    return -1;

        cl_int root_cant = scene.object_count();
        if (tracer.set_arg(6, sizeof(cl_int), &root_cant))
                    return -1;

        size_t global_size[]   = {0, 0, 0};
        size_t global_offset[] = {0, 0, 0};
        size_t local_size[]    = {0, 0, 0};

        size_t leftover_global_size[]   = {0, 0, 0};
        size_t leftover_global_offset[] = {0, 0, 0};
        size_t leftover_local_size[]    = {0, 0, 0};

	const size_t  sec_size = 64;
	size_t leftover = ray_count%sec_size;

	bool do_leftover = false;

	if (secondary) {
		if (ray_count >= sec_size) {
                        global_size[0] = ray_count - leftover;
                        global_offset[0] = 0;
                        local_size[0]  = sec_size;
			if (leftover) {
                                leftover_global_size[0] = leftover;
                                leftover_global_offset[0] = ray_count - leftover;
                                leftover_local_size[0] = 0;
								do_leftover = true;
			}
		} else {
                        global_size[0] = ray_count;
                        global_offset[0] = 0;
                        local_size[0] = 0;
		}
	} else {
                global_size[0] = ray_count;
                global_offset[0] = 0;
                local_size[0] = 0;
	}

        tracer.set_global_size(global_size);
        tracer.set_global_offset(global_offset);
        tracer.set_local_size(local_size);

        if (tracer.execute())
                return -1;

        if (do_leftover) {
                tracer.set_global_size(leftover_global_size);
                tracer.set_global_offset(leftover_global_offset);
                tracer.set_local_size(leftover_local_size);
                if (tracer.execute())
                        return -1;
        }                

	if (m_timing)
		m_tracer_time_ms = m_tracer_timer.msec_since_snap();

	return 0;
}

int32_t 
Tracer::trace(Scene& scene, int32_t ray_count, 
	      RayBundle& rays, HitBundle& hits, bool secondary)
{
        if (!m_initialized || !device.good())
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

        if (tracer.set_arg(3, scene.triangle_mem()))
                    return -1;

        if (tracer.set_arg(4, scene.bvh_nodes_mem()))
                    return -1;

        if (tracer.set_arg(5, scene.bvh_roots_mem()))
                    return -1;

        cl_int root_cant = scene.object_count();
        if (tracer.set_arg(6, sizeof(cl_int), &root_cant))
                    return -1;

        size_t global_size[]   = {0, 0, 0};
        size_t global_offset[] = {0, 0, 0};
        size_t local_size[]    = {0, 0, 0};

        size_t leftover_global_size[]   = {0, 0, 0};
        size_t leftover_global_offset[] = {0, 0, 0};
        size_t leftover_local_size[]    = {0, 0, 0};

		const size_t  sec_size = 64;
        size_t leftover = ray_count%sec_size;

		bool do_leftover = false;

		if (secondary) {
			if (ray_count >= sec_size) {
				global_size[0] = ray_count - leftover;
				global_offset[0] = 0;
				local_size[0]  = sec_size;
				if (leftover) {
					leftover_global_size[0] = leftover;
					leftover_global_offset[0] = ray_count - leftover;
					leftover_local_size[0] = 0;
					do_leftover = true;
				}
			} else {
				global_size[0] = ray_count;
				global_offset[0] = 0;
				local_size[0] = 0;
			}
		} else {
			global_size[0] = ray_count;
			global_offset[0] = 0;
			local_size[0] = 0;
		}
		
		
		tracer.set_global_size(global_size);
		tracer.set_global_offset(global_offset);
		tracer.set_local_size(local_size);

        if (tracer.execute())
                return -1;

        if (do_leftover) {
                tracer.set_global_size(leftover_global_size);
                tracer.set_global_offset(leftover_global_offset);
                tracer.set_local_size(leftover_local_size);
                if (tracer.execute())
                        return -1;
        }                

	if (m_timing)
		m_tracer_time_ms = m_tracer_timer.msec_since_snap();

	return 0;
}

int32_t 
Tracer::shadow_trace(Scene& scene, int32_t ray_count, 
		     RayBundle& rays, HitBundle& hits, bool secondary)
{
        if (!m_initialized || !device.good())
                return -1;

        DeviceFunction& shadow = device.function(shadow_id);

	if (m_timing)
		m_shadow_timer.snap_time();

        if (shadow.set_arg(0, hits.mem()))
                return -1;

        if (shadow.set_arg(1, rays.mem()))
                return -1;

        if (shadow.set_arg(2, scene.vertex_mem()))
                return -1;

        if (shadow.set_arg(3, scene.triangle_mem()))
                return -1;

        if (shadow.set_arg(4, scene.bvh_nodes_mem()))
                return -1;

        if (shadow.set_arg(5, scene.lights_mem()))
                return -1;

        size_t global_size[]   = {0, 0, 0};
        size_t global_offset[] = {0, 0, 0};
        size_t local_size[]    = {0, 0, 0};

        size_t leftover_global_size[]   = {0, 0, 0};
        size_t leftover_global_offset[] = {0, 0, 0};
        size_t leftover_local_size[]    = {0, 0, 0};

	const size_t  sec_size = 64;
	size_t leftover = ray_count%sec_size;

	bool do_leftover = false;

	if (secondary) {
		if (ray_count >= sec_size) {
                        global_size[0] = ray_count - leftover;
                        global_offset[0] = 0;
                        local_size[0]  = sec_size;
			if (leftover) {
                                leftover_global_size[0] = leftover;
                                leftover_global_offset[0] = ray_count - leftover;
                                leftover_local_size[0] = 0;
								do_leftover = true;
			}
		} else {
                        global_size[0] = ray_count;
                        global_offset[0] = 0;
                        local_size[0] = 0;
		}
	} else {
                global_size[0] = ray_count;
                global_offset[0] = 0;
                local_size[0] = 0;
	}


        shadow.set_global_size(global_size);
        shadow.set_global_offset(global_offset);
        shadow.set_local_size(local_size);

        if (shadow.execute())
                return -1;

        if (do_leftover) {
                shadow.set_global_size(leftover_global_size);
                shadow.set_global_offset(leftover_global_offset);
                shadow.set_local_size(leftover_local_size);
				if (shadow.execute())
                        return -1;
        }                

	if (m_timing)
		m_shadow_time_ms = m_shadow_timer.msec_since_snap();

	return 0;

}

int32_t
Tracer::shadow_trace(Scene& scene, DeviceMemory& bvh_mem, int32_t ray_count, 
		     RayBundle& rays, HitBundle& hits, bool secondary)
{
        if (!m_initialized || !device.good())
                return -1;

        DeviceFunction& shadow = device.function(shadow_id);

	if (m_timing)
		m_shadow_timer.snap_time();

        if (shadow.set_arg(0, hits.mem()))
                return -1;

        if (shadow.set_arg(1, rays.mem()))
                return -1;

        if (shadow.set_arg(2, scene.vertex_mem()))
                return -1;

        if (shadow.set_arg(3, scene.triangle_mem()))
                return -1;

        if (shadow.set_arg(4, bvh_mem))
                return -1;

        if (shadow.set_arg(5, scene.lights_mem()))
                return -1;

        size_t global_size[]   = {0, 0, 0};
        size_t global_offset[] = {0, 0, 0};
        size_t local_size[]    = {0, 0, 0};

        size_t leftover_global_size[]   = {0, 0, 0};
        size_t leftover_global_offset[] = {0, 0, 0};
        size_t leftover_local_size[]    = {0, 0, 0};

	const size_t  sec_size = 64;
	size_t leftover = ray_count%sec_size;

	bool do_leftover = false;

	if (secondary) {
		if (ray_count >= sec_size) {
                        global_size[0] = ray_count - leftover;
                        global_offset[0] = 0;
                        local_size[0]  = sec_size;
			if (leftover) {
                                leftover_global_size[0] = leftover;
                                leftover_global_offset[0] = ray_count - leftover;
                                leftover_local_size[0] = 0;
								do_leftover = true;
			}
		} else {
                        global_size[0] = ray_count;
                        global_offset[0] = 0;
                        local_size[0] = 0;
		}
	} else {
                global_size[0] = ray_count;
                global_offset[0] = 0;
                local_size[0] = 0;
	}

        
        shadow.set_global_size(global_size);
        shadow.set_global_offset(global_offset);
        shadow.set_local_size(local_size);

        if (shadow.execute())
                return -1;

        if (do_leftover) {
                shadow.set_global_size(leftover_global_size);
                shadow.set_global_offset(leftover_global_offset);
                shadow.set_local_size(leftover_local_size);
                if (shadow.execute())
                        return -1;
        }                

	if (m_timing)
		m_shadow_time_ms = m_shadow_timer.msec_since_snap();

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
