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


        std::vector<std::string> kernel_names;
        kernel_names.push_back("generate_primary_rays");
        kernel_names.push_back("generate_primary_rays_zcurve");

        std::vector<function_id> function_ids;
        function_ids = device.build_functions("src/kernel/primary-ray-generator.cl", 
                                              kernel_names);

        if (!function_ids.size())
                return -1;

        generator_id        = function_ids[0];
        generator_zcurve_id = function_ids[1];

	m_timing = false;
	m_spp = 1;

        pixel_samples_id = device.new_memory();
        DeviceMemory& pixel_samples_mem = device.memory(pixel_samples_id);
        pixel_sample_cl single_pixel_sample = {0.f,0.f,1.f};
        if (pixel_samples_mem.initialize(m_spp * sizeof(pixel_sample_cl), 
                                         &single_pixel_sample, 
                                         READ_ONLY_MEMORY))
                return -1;

        m_initialized = true;
        m_use_zcurve  = false;
        m_quad_size   = 32;
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

        bool use_zcurve = false;
        /// Check if sizes are equal and a power of 2
        if (m_use_zcurve && size[0] == size[1] && (size[0] & (size[0]-1)) == 0) {
                use_zcurve = true;  
                std::cout << "Using zcurve!\n";
        }

        function_id gen_id = use_zcurve ? generator_zcurve_id : generator_id;

        DeviceFunction& generator = device.function(gen_id);

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

        if (generator.set_arg(7,sizeof(cl_int), &m_quad_size))
                return -1;

        if (generator.set_arg(8,sizeof(cl_int), &m_spp))
                return -1;

        if (generator.set_arg(9, device.memory(pixel_samples_id)))
                return -1;

        size_t group_size = generator.max_group_size();

	/*------------------- Execute kernel to create rays ------------*/
        int32_t ret = generator.enqueue_single_dim(ray_count, group_size, offset);
        device.enqueue_barrier();

	if (m_timing) {
            device.finish_commands();
            m_time_ms = m_timer.msec_since_snap();
        }


        // char* samples_buffer = new char[bundle.mem().size()];
        // sample_cl* samples = (sample_cl*)samples_buffer;

        // bundle.mem().read(bundle.mem().size(), samples_buffer);

        // std::vector<bool> visited(size[0] * size[1], false);

        // for (uint32_t i = 0; i < ray_count; i++) {
        //         int pixel = samples[i].pixel;
        //         if ((size_t)pixel > size[0] * size[1] || pixel < 0) {
        //                 std::cout << "Sample " << i << " (" 
        //                           << pixel << ") is out of range!" 
        //                           << std::endl;
        //                 continue;
        //         }

        //         if (visited[pixel])
        //                 std::cout << "Sample " << i << " (" 
        //                           << samples[i].pixel << ") visited multiple times!" 
        //                           << std::endl;
        //         else
        //                 visited[pixel] = true;
        // }

        // for (int i = 0; i < 256; i++) {
        //         std::cout << "Sample " << i << " (" 
        //                   << samples[i].pixel << ")" << std::endl;
        // }

        // delete [] samples_buffer;
        
        
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

void 
PrimaryRayGenerator::update_configuration(const RendererConfig& conf)
{
        m_use_zcurve = conf.prim_ray_use_zcurve;
        m_quad_size  = conf.prim_ray_quad_size;
}
