#include <rt/secondary-ray-generator.hpp>
#include <gpu/scan.hpp>

SecondaryRayGenerator::SecondaryRayGenerator()
{
        m_initialized = false;
}

int32_t 
SecondaryRayGenerator::initialize()
{
	m_max_rays = 0;

        DeviceInterface& device = *DeviceInterface::instance();
        if (!device.good())
                return -1;

        std::vector<std::string> kernel_names;
        kernel_names.push_back("mark_secondary_rays");
        kernel_names.push_back("generate_secondary_rays");

        std::vector<function_id> function_ids;
        function_ids = device.build_functions("src/kernel/secondary-ray-generator.cl", 
                                              kernel_names);
        if (!function_ids.size())
                return -1;

        marker_id = function_ids[0];
        generator_id = function_ids[1];

        const size_t initial_size = 512*2*1024; //Up to ~ 1e6 rays 

        count_id = device.new_memory();
        DeviceMemory& count_mem = device.memory(count_id);
        if (count_mem.initialize(initial_size * sizeof(cl_int), READ_WRITE_MEMORY))
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


        DeviceInterface& device = *DeviceInterface::instance();
        DeviceFunction& marker = device.function(marker_id);
        DeviceFunction& generator = device.function(generator_id);
        int max_rays_out = ray_out.count();

        size_t group_size = std::min(marker.max_group_size(), generator.max_group_size());

        if (!group_size)
                return -1;

        DeviceMemory& count_mem = device.memory(count_id);
        if (sizeof(cl_int)*(rays_in+1) > count_mem.size()) {
                if (count_mem.resize(sizeof(cl_int)*(rays_in+1)))
                        return -1;
        }

        /////////////// Mark secondary rays to be created in aux mem //////////////
        /* Arguments */
        if (marker.set_arg(0, hits.mem()) || 
            marker.set_arg(1, ray_in.mem()) ||
            marker.set_arg(2, scene.material_list_mem()) || 
            marker.set_arg(3, scene.material_map_mem()) || 
            marker.set_arg(4, count_mem)) {
                return -1;
        }

        if (marker.enqueue_single_dim(rays_in, group_size)) {
                return -1;
        }
        device.enqueue_barrier();
        /*//////////////////////////////////////////////////////////////////*/

        ////////////// Compute scan (prefix sum) on count_mem ///////////////////
        if (gpu_scan_uint(device, count_id, rays_in+2, count_id)){
                return -1;
        }

        uint32_t new_ray_count;
        if (count_mem.read(sizeof(cl_int),&new_ray_count,
                           sizeof(cl_int)*(rays_in))) {
                return -1;
        }
        *rays_out = new_ray_count;
        // /*//////////////////////////////////////////////////////////////////*/

        ////////// Use prefix sums generate rays in the right location //////////////
        if (*rays_out) {
                if (generator.set_arg(0, hits.mem()) || 
                    generator.set_arg(1, ray_in.mem()) ||
                    generator.set_arg(2, scene.material_list_mem()) || 
                    generator.set_arg(3, scene.material_map_mem()) || 
                    generator.set_arg(4, ray_out.mem()) || 
                    generator.set_arg(5, count_mem) ||
                    generator.set_arg(6, sizeof(cl_int),&max_rays_out)) {
                        return -1;
                }
                if (generator.enqueue_single_dim(rays_in,group_size)) {
                        return -1;
                }
                device.enqueue_barrier();
        }
        /*//////////////////////////////////////////////////////////////////*/

	if (m_timing) {
                device.finish_commands();
		m_time_ms = m_timer.msec_since_snap();
        }

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
