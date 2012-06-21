#include <rt/secondary-ray-generator.hpp>

SecondaryRayGenerator::SecondaryRayGenerator()
{
        m_initialized = false;
}

int32_t 
SecondaryRayGenerator::initialize(const CLInfo& clinfo)
{
        if (device.initialize(clinfo))
                return -1;

	m_max_rays = 0;

        marker_id = device.new_function();
        DeviceFunction& marker = device.function(marker_id);
        if (marker.initialize("src/kernel/secondary-ray-generator.cl", 
                              "mark_secondary_rays"))
                return -1;

        local_prefix_sum_id = device.new_function();
        DeviceFunction& local_prefix_sum = device.function(local_prefix_sum_id);
        if (local_prefix_sum.initialize("src/kernel/secondary-ray-generator.cl", 
                                        "local_prefix_sum"))
                return -1;

        totals_prefix_sum_id = device.new_function();
        DeviceFunction& totals_prefix_sum = device.function(totals_prefix_sum_id);
        if (totals_prefix_sum.initialize("src/kernel/secondary-ray-generator.cl",
                           "totals_prefix_sum"))
                return -1;

        global_prefix_sum_id = device.new_function();
        DeviceFunction& global_prefix_sum = device.function(global_prefix_sum_id);
        if (global_prefix_sum.initialize("src/kernel/secondary-ray-generator.cl",
                                         "global_prefix_sum"))
                return -1;

        generator_id = device.new_function();
        DeviceFunction& generator = device.function(generator_id);
        if (generator.initialize("src/kernel/secondary-ray-generator.cl",
                                 "generate_secondary_rays"))
                return -1;

        const int group_size = device.max_group_size(0);
        const size_t initial_size = 512*2*1024; //Up to ~ 1e6 rays 

        count_in_id = device.new_memory();
        DeviceMemory& count_in_mem = device.memory(count_in_id);
        if (count_in_mem.initialize(initial_size * sizeof(cl_int), READ_WRITE_MEMORY))
                return -1;

        count_out_id = device.new_memory();
        DeviceMemory& count_out_mem = device.memory(count_out_id);
        if (count_out_mem.initialize(initial_size * sizeof(cl_int), READ_WRITE_MEMORY))
                return -1;

        totals_id = device.new_memory();
        DeviceMemory& totals_mem = device.memory(totals_id);
        if (totals_mem.initialize(sizeof(cl_int)*initial_size/(2*group_size)))
            return -1;

	m_timing = false;
        m_initialized = true;
	return 0;
}

int32_t 
SecondaryRayGenerator::generate(Scene& scene, RayBundle& ray_in, size_t rays_in,
				HitBundle& hits, RayBundle& ray_out, size_t* rays_out)
{

        if(!m_initialized)
                return -1;

	if (m_timing)
		m_timer.snap_time();


        int max_rays_out = ray_out.count();

        size_t global_size[3];
        const int group_size = device.max_group_size(0);
        if (!group_size)
                return -1;

        int rays_in_padded = rays_in;
        if (rays_in_padded % (2*group_size))
                rays_in_padded += (2*group_size) - (rays_in%(2*group_size));
        int prefix_sum_groups = (rays_in_padded)/(2*group_size);

        DeviceMemory& count_in_mem = device.memory(count_in_id);
        DeviceMemory& count_out_mem = device.memory(count_out_id);
        DeviceMemory& totals_mem = device.memory(totals_id);

        /* Check that we have enough space in buffers */
        if (sizeof(cl_int)*rays_in_padded > count_in_mem.size()) {
                if (count_in_mem.resize(sizeof(cl_int)*rays_in_padded) ||
                    count_out_mem.resize(sizeof(cl_int)*rays_in_padded) ||
                    totals_mem.resize(sizeof(cl_int)*prefix_sum_groups))
                        return -1;
        }


        /////////////// Mark secondary rays to be created in aux mem //////////////
        DeviceFunction& marker = device.function(marker_id);
	/* Arguments */
        if (marker.set_arg(0, hits.mem()) || 
            marker.set_arg(1, ray_in.mem()) ||
            marker.set_arg(2, scene.material_list_mem()) || 
            marker.set_arg(3, scene.material_map_mem()) || 
            marker.set_arg(4, count_in_mem)) {
                return -1;
        }

        marker.set_dims(1);
        global_size[0] = rays_in;
        marker.set_global_size(global_size);
        if (marker.execute()) {
                return -1;
        }
        /*//////////////////////////////////////////////////////////////////*/

        /////////////// Compute prefix sum of groups //////////////
        DeviceFunction& local_prefix_sum = device.function(local_prefix_sum_id);
        size_t local_size[] = {group_size, 0, 0};
        int local_mem_size = group_size * 2 + 1 ;

        local_prefix_sum.set_dims(1);
        global_size[0] = prefix_sum_groups * group_size;
        local_prefix_sum.set_global_size(global_size);
        local_prefix_sum.set_local_size(local_size);
        if (local_prefix_sum.set_arg(0, count_in_mem) ||
            local_prefix_sum.set_arg(1, count_out_mem)||
            local_prefix_sum.set_arg(2, sizeof(cl_int)* local_mem_size, NULL) ||
            local_prefix_sum.set_arg(3, totals_mem)) {
                return -1;
        }

        if (local_prefix_sum.execute()) {
                return -1;
        }
        /*//////////////////////////////////////////////////////////////////*/

        //TODO: do this the efficient way
        ////////////// Compute prefix sums of group totals //////////////
        DeviceFunction& totals_prefix_sum = device.function(totals_prefix_sum_id);
        global_size[0] = 1;
        totals_prefix_sum.set_dims(1);
        totals_prefix_sum.set_global_size(global_size);
        int totals_prefix_sum_size = prefix_sum_groups;
        if (totals_prefix_sum.set_arg(0, totals_mem) || 
            totals_prefix_sum.set_arg(1, sizeof(int), &totals_prefix_sum_size)) {
                return -1;
        }

        if (totals_prefix_sum.execute()) {
                return -1;
        }
        /*//////////////////////////////////////////////////////////////////*/

        //////////////// Compute global prefix sums //////////////
        // DeviceFunction& global_prefix_sum = device.function(global_prefix_sum_id);
        // global_prefix_sum.set_dims(1);
        // global_size[0] = padded_global_size;
        // global_prefix_sum.set_global_size(global_size);
        // if (global_prefix_sum.set_arg(0, count_in_mem) || 
        //     global_prefix_sum.set_arg(1, count_out_mem) ||
        //     global_prefix_sum.set_arg(2, totals_mem)){
        //         return -1;
        // }
        // if (global_prefix_sum.execute()) {
        //         return -1;
        // }
        /*//////////////////////////////////////////////////////////////////*/
        
        /*////////////// Read rays generated from last prefix sum /////////*/
        int last_total = 0;
        if (totals_prefix_sum_size > 1) {
                if (totals_mem.read(sizeof(cl_int),&last_total,
                                    sizeof(cl_int)*(totals_prefix_sum_size-2))) {
                        return -1;
                }
        }
        int last_local_sum;
        if (count_out_mem.read(sizeof(cl_int),&last_local_sum,
                               sizeof(cl_int)*(rays_in-1))) {
                return -1;
        }
        *rays_out = last_total + last_local_sum;
        /*//////////////////////////////////////////////////////////////////*/

        ////////// Use prefix sums generate rays in the right location //////////////
        if (*rays_out) {
                DeviceFunction& generator = device.function(generator_id);
                // global_size[0] = 2*prefix_sum_groups * group_size;
                global_size[0] = rays_in_padded;
                local_size[0]  = group_size;
                generator.set_dims(1);
                generator.set_global_size(global_size);
                generator.set_local_size(local_size);
                if (generator.set_arg(0, hits.mem()) || 
                    generator.set_arg(1, ray_in.mem()) ||
                    generator.set_arg(2, scene.material_list_mem()) || 
                    generator.set_arg(3, scene.material_map_mem()) || 
                    generator.set_arg(4, ray_out.mem()) || 
                    generator.set_arg(5, count_out_mem) ||
                    generator.set_arg(6, totals_mem)    ||
                    generator.set_arg(7, sizeof(cl_int),&max_rays_out)) {
                        return -1;
                }
                if (generator.execute()) {
                        return -1;
                }
        }
        /*//////////////////////////////////////////////////////////////////*/

	if (m_timing)
		m_time_ms = m_timer.msec_since_snap();

        // std::cout << "\nrays out : " << *rays_out << std::endl;
        // std::cout << "\ntime : " << m_timer.msec_since_snap() << std::endl;
        // exit(1);

	return 0;
}


void 
SecondaryRayGenerator::set_max_rays(size_t max)
{ 
	m_max_rays = max;
}

void
SecondaryRayGenerator::timing(bool b)
{
	m_timing = b;
}	

double 
SecondaryRayGenerator::get_exec_time()
{
	return m_time_ms;
}
