#include <rt/secondary-ray-generator.hpp>
#include <gpu/scan.hpp>

SecondaryRayGenerator::SecondaryRayGenerator()
{
        m_initialized = false;
        m_disc = true;
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

        kernel_names.push_back("mark_secondary_rays_disc");
        kernel_names.push_back("generate_secondary_rays_disc");

        kernel_names.push_back("gen_sec_ray");

        std::vector<function_id> function_ids;
        function_ids = device.build_functions("src/kernel/secondary-ray-generator.cl", 
                                              kernel_names);
        if (!function_ids.size())
                return -1;

        marker_id = function_ids[0];
        generator_id = function_ids[1];
        marker_disc_id = function_ids[2];
        generator_disc_id = function_ids[3];
        gen_sec_ray_id = function_ids[4];

        const size_t initial_size = 512*2*1024; //Up to ~ 1e6 rays 

        count_id = device.new_memory();
        DeviceMemory& count_mem = device.memory(count_id);
        if (count_mem.initialize(initial_size * sizeof(cl_int), READ_WRITE_MEMORY))
                return -1;

        counters_id = device.new_memory();
        DeviceMemory& counters_mem = device.memory(counters_id);
        if (counters_mem.initialize(2 * sizeof(cl_int), READ_WRITE_MEMORY))
                return -1;

	m_timing = false;
        m_initialized = true;
	return 0;
}

int32_t SecondaryRayGenerator::generate(Scene& scene, 
                                        RayBundle& ray_in, size_t rays_in,
                                        HitBundle& hits, 
                                        RayBundle& ray_out, size_t* rays_out)
{
        CLInfo* clinfo = clinfo->instance();

        //////////////// No disc
        if (!m_disc) {
                //////////////// tasks
                if (m_tasks) {

                        if (gen_tasks(scene, ray_in, rays_in, hits, ray_out,  rays_out)) {
                                std::cerr << "Failed to create secondary rays." 
                                          << "\n";
                                return -1;
                        }
                //////////////// scan
                } else { 

                        if (gen_scan(scene, ray_in, rays_in, hits, ray_out,  rays_out)) {
                                std::cerr << "Failed to create secondary rays." 
                                          << "\n";
                                return -1;
                        }

                }

        //////////////// Disc
        } else {

                //////////////// tasks
                if (m_tasks) {

                        if (gen_tasks_disc(scene,ray_in,rays_in,hits,ray_out,rays_out)) {
                                std::cerr << "Failed to create secondary rays." 
                                          << "\n";
                                return -1;
                        }
                //////////////// scan
                } else {
                        if (gen_scan_disc(scene,ray_in,rays_in,hits,ray_out,rays_out)) {
                                std::cerr << "Failed to create secondary rays." 
                                          << "\n";
                                return -1;
                        }
                }

                ///// For disc generation only:
                //// If rays_out is too small, then it's very likely that 
                //// reflect and refract rays from the same base ray will be 
                //// handled by different warps at the same time. If that
                //// seems like it will be the case, then compute it so that
                //// they will be bundled together for (hopefully) the same 
                //// work group to analize.

                size_t max_count = 
                        10 * clinfo->max_work_group_size * clinfo->max_compute_units;

                if (*rays_out > max_count || 
                    *rays_out == 0)
                    // *rays_out < clinfo->max_work_group_size)
                        return 0;

                double partial_time = m_time_ms;

                //////////////// tasks
                if (m_tasks) {

                        if (gen_tasks(scene, ray_in, rays_in, hits, ray_out,  rays_out)) {
                                std::cerr << "Failed to create secondary rays." 
                                          << "\n";
                                return -1;
                        }
                //////////////// scan
                } else { 

                        if (gen_scan(scene, ray_in, rays_in, hits, ray_out,  rays_out)) {
                                std::cerr << "Failed to create secondary rays." 
                                          << "\n";
                                return -1;
                        }

                }
                m_time_ms += partial_time;
        }

        return 0;
}


int32_t 
SecondaryRayGenerator::gen_scan_disc(Scene& scene, RayBundle& ray_in, size_t rays_in,
                                     HitBundle& hits, RayBundle& ray_out, size_t* rays_out)
{

        if(!m_initialized)
                return -1;

	if (m_timing)
		m_timer.snap_time();


        DeviceInterface& device = *DeviceInterface::instance();
        DeviceFunction& marker = device.function(marker_disc_id);
        DeviceFunction& generator = device.function(generator_disc_id);
        cl_int max_rays_out = ray_out.count();

        size_t group_size = std::min(marker.max_group_size(), generator.max_group_size());

        if (!group_size)
                return -1;

        DeviceMemory& count_mem = device.memory(count_id);
        if (sizeof(cl_int)*(2*rays_in+1) > count_mem.size()) {
                // *2 because we have one int per reflect ray and one int per refract ray
                if (count_mem.resize(sizeof(cl_int)*(2*rays_in+1))) 
                        return -1;
        }

        cl_int ray_in_count = rays_in;

        /////////////// Mark secondary rays to be created in aux mem //////////////
        /* Arguments */
        if (marker.set_arg(0, hits.mem()) || 
            marker.set_arg(1, ray_in.mem()) ||
            marker.set_arg(2, scene.material_list_mem()) || 
            marker.set_arg(3, scene.material_map_mem()) || 
            marker.set_arg(4, count_mem) ||
            marker.set_arg(5, sizeof(cl_int), &ray_in_count)) {
                return -1;
        }

        if (marker.enqueue_single_dim(rays_in, group_size)) {
                return -1;
        }
        device.enqueue_barrier();

        /*//////////////////////////////////////////////////////////////////*/

        ////////////// Compute scan (prefix sum) on count_mem ///////////////////
        if (gpu_scan_uint(device, count_id, 2*rays_in, count_id)){
                return -1;
        }

        device.enqueue_barrier();

        uint32_t new_ray_count;
        if (count_mem.read(sizeof(cl_int),&new_ray_count,
                           sizeof(cl_int)*(2*rays_in))) {
                return -1;
        }

        if (new_ray_count > 2* rays_in)
                return -1;

        *rays_out = new_ray_count;
        // /*//////////////////////////////////////////////////////////////////*/

        // device.finish_commands();//!!
        // exit(0);//!!

        ////////// Use prefix sums generate rays in the right location //////////////
        if (*rays_out) {
                if (generator.set_arg(0, hits.mem()) || 
                    generator.set_arg(1, ray_in.mem()) ||
                    generator.set_arg(2, scene.material_list_mem()) || 
                    generator.set_arg(3, scene.material_map_mem()) || 
                    generator.set_arg(4, ray_out.mem()) || 
                    generator.set_arg(5, count_mem) ||
                    generator.set_arg(6, sizeof(cl_int),&ray_in_count) ||
                    generator.set_arg(7, sizeof(cl_int),&max_rays_out)) {
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


int32_t 
SecondaryRayGenerator::gen_scan(Scene& scene, RayBundle& ray_in, size_t rays_in,
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
        if (gpu_scan_uint(device, count_id, rays_in, count_id)){
                return -1;
        }


        uint32_t new_ray_count;
        if (count_mem.read(sizeof(cl_int),&new_ray_count,
                           sizeof(cl_int)*(rays_in))) {
                return -1;
        }

        // std::cout << "Rays out after read: " << new_ray_count << std::endl;

        if (new_ray_count > 2* rays_in) {
                std::cerr << "Error: secondary ray generator reported wrong number of "
                          << "new rays.\n";
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

        // std::cout << "Rays out after generator: " << *rays_out << std::endl;

	if (m_timing) {
                device.finish_commands();
		m_time_ms = m_timer.msec_since_snap();
        }

	return 0;
}

int32_t 
SecondaryRayGenerator::gen_tasks(Scene& scene, RayBundle& ray_in, size_t rays_in,
                                 HitBundle& hits, RayBundle& ray_out, size_t* rays_out)
{

        if(!m_initialized)
                return -1;

	if (m_timing)
		m_timer.snap_time();


        DeviceInterface& device = *DeviceInterface::instance();
        DeviceFunction& gen_sec = device.function(gen_sec_ray_id);
        int max_rays_out = ray_out.count();

        size_t group_size = gen_sec.max_group_size();
        if (!group_size)
                return -1;

        DeviceMemory& counters_mem = device.memory(counters_id);
        cl_int counters[2] = {0, 0};
        if (counters_mem.write(sizeof(cl_int) * 2, &counters)) {
                return -1;
        }

        /////////////// Mark secondary rays to be created in aux mem //////////////
        /* Arguments */
        if (gen_sec.set_arg(0, hits.mem()) || 
            gen_sec.set_arg(1, scene.material_list_mem()) || 
            gen_sec.set_arg(2, scene.material_map_mem()) || 
            gen_sec.set_arg(3, ray_in.mem()) ||
            gen_sec.set_arg(4, ray_out.mem()) ||
            gen_sec.set_arg(5, counters_mem) ||
            gen_sec.set_arg(6, sizeof(cl_int) * group_size, NULL) ||
            gen_sec.set_arg(7, sizeof(cl_int) * group_size * 2, NULL) ||
            gen_sec.set_arg(8, sizeof(cl_int), &rays_in) ||
            gen_sec.set_arg(9, sizeof(cl_int), &max_rays_out)) {
                return -1;
        }

        if (gen_sec.enqueue_single_dim(group_size * 4, group_size)) {
                return -1;
        }
        device.enqueue_barrier();
        /*//////////////////////////////////////////////////////////////////*/

        if (counters_mem.read(sizeof(cl_int)*2,&counters)) {
                return -1;
        }
        *rays_out = counters[1];
        // /*//////////////////////////////////////////////////////////////////*/

        // std::cout << "rays in: " << rays_in << std::endl;
        // std::cout << "processed samples: " << counters[0] << std::endl;
        // std::cout << "output samples: " << counters[1] << std::endl;

	if (m_timing) {
                device.finish_commands();
		m_time_ms = m_timer.msec_since_snap();
        }

	return 0;
}

int32_t 
SecondaryRayGenerator::gen_tasks_disc(Scene& scene, 
                                      RayBundle& ray_in, size_t rays_in,
                                      HitBundle& hits, 
                                      RayBundle& ray_out, size_t* rays_out)
{
        static bool warned = false;
        if (!warned) {
                std::cerr << "Warning: using atomics and discriminating "
                          << "reflection/refraction not implemented yet."
                          << "Will use atomics only.\n";
                warned = true;
        }                        
                return gen_tasks(scene, ray_in, rays_in, hits, ray_out, rays_out);
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

void 
SecondaryRayGenerator::update_configuration(const RendererConfig& conf)
{
	m_tasks = conf.sec_ray_use_atomics;
	m_disc = conf.sec_ray_use_disc;
}


