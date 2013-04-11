#include <stdint.h>

#include <cl-gl/opencl-init.hpp>
#include <rt/primary-ray-generator.hpp>


int32_t
PrimaryRayGenerator::initialize()
{

        if (m_initialized)
                return -1;

        DeviceInterface& device = *DeviceInterface::instance();
        if (!device.good())
                return -1;

        generator_id = device.new_function();
        DeviceFunction& generator = device.function(generator_id);

        if (generator.initialize("src/kernel/primary-ray-generator.cl", 
                                   "generate_primary_rays"))
                return -1;

        generator.set_dims(1);
	m_timing = false;
	m_spp = 1;

        pixel_samples_id = device.new_memory();
        DeviceMemory& pixel_samples_mem = device.memory(pixel_samples_id);
        pixel_sample_cl single_pixel_sample = {0.f,0.f,1.f};
        if (pixel_samples_mem.initialize(m_spp * sizeof(pixel_sample_cl), &single_pixel_sample, 
                                   READ_ONLY_MEMORY))
                return -1;

        m_initialized = true;
	return 0;
	
}


int32_t 
PrimaryRayGenerator::generate(const Camera& cam, RayBundle& bundle, size_t size[2],
			      const size_t ray_count, const size_t offset)
{

        if (!m_initialized)
                return -1;

	if (m_timing)
		m_timer.snap_time();

        DeviceInterface& device = *DeviceInterface::instance();
        DeviceFunction& generator = device.function(generator_id);

	/*-------------- Set cam parameters as arguments ------------------*/
	/*-- My OpenCL implementation cannot handle using float3 as arguments!--*/
	cl_float4 cam_pos = vec3_to_float4(cam.pos);
	cl_float4 cam_dir = vec3_to_float4(cam.dir);
	cl_float4 cam_right = vec3_to_float4(cam.right);
	cl_float4 cam_up = vec3_to_float4(cam.up);

        if (generator.set_arg(0, bundle.mem()))
                return -1;

        if (generator.set_arg(1,sizeof(cl_float4), &cam_pos))
                return -1;

        if (generator.set_arg(2,sizeof(cl_float4), &cam_dir))
                return -1;

        if (generator.set_arg(3,sizeof(cl_float4), &cam_right))
                return -1;

        if (generator.set_arg(4,sizeof(cl_float4), &cam_up))
                return -1;

	cl_int width = size[0];
        if (generator.set_arg(5,sizeof(cl_int), &width))
                return -1;

	cl_int height = size[1];
        if (generator.set_arg(6,sizeof(cl_int), &height))
                return -1;

        if (generator.set_arg(7,sizeof(cl_int), &m_spp))
                return -1;

        if (generator.set_arg(8, device.memory(pixel_samples_id)))
                return -1;

        size_t group_size = generator.max_group_size();

	/*------------------- Execute kernel to create rays ------------*/
        int32_t ret = generator.enqueue_single_dim(ray_count, group_size, offset);
        device.enqueue_barrier();

	if (m_timing) {
            device.finish_commands();
            m_time_ms = m_timer.msec_since_snap();
        }

	return ret;
}

int32_t 
PrimaryRayGenerator::set_spp(size_t spp, pixel_sample_cl const* pixel_samples)
{
        DeviceInterface& device = *DeviceInterface::instance();

        if (!m_initialized || !device.good() || spp < 1)
                return -1;

	m_spp = spp;

        DeviceMemory& pixel_samples_mem = device.memory(pixel_samples_id);

        if (pixel_samples_mem.resize(spp * sizeof(pixel_sample_cl)))
                return -1;
        if (pixel_samples_mem.write(spp * sizeof(pixel_sample_cl), pixel_samples))
                return -1;

	return 0;
}

size_t
PrimaryRayGenerator::get_spp() const
{
	return m_spp;
}

DeviceMemory&
PrimaryRayGenerator::get_pixel_samples() 
{
        DeviceInterface& device = *DeviceInterface::instance();
	return device.memory(pixel_samples_id);
}

void 
PrimaryRayGenerator::timing(bool b)
{
	m_timing = b;
}

double 
PrimaryRayGenerator::get_exec_time()
{
	return m_time_ms;
}
