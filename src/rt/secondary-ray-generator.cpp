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

        ray_count_id = device.new_memory();
        DeviceMemory& ray_count_mem = device.memory(ray_count_id);
        if (ray_count_mem.initialize(sizeof(cl_int), READ_WRITE_MEMORY))
                return -1;

        generator_id = device.new_function();
        DeviceFunction& generator = device.function(generator_id);
        if (generator.initialize("src/kernel/secondary-ray-generator.cl",
                                 "generate_secondary_rays"))
                return -1;

        generator.set_dims(1);

        if (generator.set_arg(5,ray_count_mem))
                return -1;

	m_timing = false;
        m_initialized = true;
	return 0;
}

int32_t 
SecondaryRayGenerator::generate(Scene& scene, RayBundle& ray_in, size_t rays_in,
				HitBundle& hits, RayBundle& ray_out, size_t* rays_out)
{
	static const cl_int zero = 0;

        if(!m_initialized)
                return -1;

	if (m_timing)
		m_timer.snap_time();

        DeviceMemory& ray_count_mem = device.memory(ray_count_id);
        DeviceFunction& generator = device.function(generator_id);

        if (ray_count_mem.write(sizeof(cl_int), &zero))
                return -1;

	/* Arguments */
        if (generator.set_arg(0, hits.mem()))
                return -1;

        if (generator.set_arg(1, ray_in.mem()))
                return -1;

        if (generator.set_arg(2, scene.material_list_mem()))
                return -1;

        if (generator.set_arg(3, scene.material_map_mem()))
                return -1;

        if (generator.set_arg(4, ray_out.mem()))
                return -1;

        if (generator.set_arg(6, sizeof(cl_int), &m_max_rays))
                return -1;

        size_t global_size[] = {rays_in, 0 , 0};
        generator.set_global_size(global_size);

        if (generator.execute())
                return -1;

        if (ray_count_mem.read(sizeof(cl_int), &m_generated_rays))
                return -1;

	*rays_out = std::min(m_generated_rays, m_max_rays);

	if (m_timing)
		m_time_ms = m_timer.msec_since_snap();

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
