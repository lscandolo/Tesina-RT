#include <rt/secondary-ray-generator.hpp>

bool 
SecondaryRayGenerator::initialize(CLInfo& clinfo)
{
	cl_int err;
	max_rays = 0;

	if (create_empty_cl_mem(clinfo,CL_MEM_READ_WRITE,sizeof(cl_int),&ray_count_mem))
		return false;

	if (init_cl_kernel(&clinfo,
			   "src/kernel/secondary-ray-generator.cl",
			   "generate_secondary_rays",
			   &generator_clk))
		return false;

	generator_clk.work_dim = 1;
	generator_clk.arg_count = 7;

	err = clSetKernelArg(generator_clk.kernel,5,sizeof(cl_mem),&ray_count_mem);
	if (error_cl(err, "clSetKernelArg 5"))
		return false;

	timing = false;

	return true;
}

bool 
SecondaryRayGenerator::generate(Scene& scene, RayBundle& ray_in, int32_t rays_in,
				HitBundle& hits, RayBundle& ray_out, int32_t* rays_out)
{
	static const cl_int zero = 0;
	// cl_int err, generated_rays;
	cl_int err;

	if (timing)
		timer.snap_time();

	err =  clEnqueueWriteBuffer(generator_clk.clinfo->command_queue,
				    ray_count_mem, 
				    true, 
				    0,
				    sizeof(cl_int),
				    &zero,
				    0,
				    NULL,
				    NULL);
	if (error_cl(err, "Ray count write"))
		return false;
	// generated_rays = 0;
	

	/* Arguments */
	err = clSetKernelArg(generator_clk.kernel,0,sizeof(cl_mem),&hits.mem());
	if (error_cl(err, "clSetKernelArg 0"))
		return false;

	err = clSetKernelArg(generator_clk.kernel,1,sizeof(cl_mem),&ray_in.mem());
	if (error_cl(err, "clSetKernelArg 1"))
		return false;
	
	err = clSetKernelArg(generator_clk.kernel,2,sizeof(cl_mem),
                             scene.material_list_mem().ptr());
	if (error_cl(err, "clSetKernelArg 2"))
		return false;

	err = clSetKernelArg(generator_clk.kernel,3,sizeof(cl_mem),
                             scene.material_map_mem().ptr());
	if (error_cl(err, "clSetKernelArg 3"))
		return false;

	err = clSetKernelArg(generator_clk.kernel,4,sizeof(cl_mem),&ray_out.mem());
	if (error_cl(err, "clSetKernelArg 4"))
		exit(1);
	
	err = clSetKernelArg(generator_clk.kernel,6,sizeof(cl_int),&max_rays);
	if (error_cl(err, "clSetKernelArg 6"))
		return false;

	generator_clk.global_work_size[0] = rays_in;

	if (execute_cl(generator_clk))
		return false;

	err =  clEnqueueReadBuffer(generator_clk.clinfo->command_queue,
				   ray_count_mem, 
				   true, 
				   0,
				   sizeof(cl_int),
				   &generated_rays,
				   0,
				   NULL,
				   NULL);
	if (error_cl(err, "Ray count read"))
		return false;

	clFinish(generator_clk.clinfo->command_queue);	
	*rays_out = std::min(generated_rays, max_rays);

	if (timing)
		time_ms = timer.msec_since_snap();

	return true;
}

void 
SecondaryRayGenerator::set_max_rays(int32_t max)
{ 
	max_rays = max;
}

void
SecondaryRayGenerator::enable_timing(bool b)
{
	timing = b;
}	

double 
SecondaryRayGenerator::get_exec_time()
{
	return time_ms;
}
