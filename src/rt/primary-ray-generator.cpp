#include <stdint.h>

#include <cl-gl/opencl-init.hpp>
#include <rt/primary-ray-generator.hpp>


bool
PrimaryRayGenerator::initialize(CLInfo& clinfo)
{

	if (init_cl_kernel(&clinfo,"src/kernel/primary-ray-generator.cl", 
			   "generate_primary_rays", &ray_clk))
		return false;

	ray_clk.work_dim = 1;
	ray_clk.arg_count = 9;
	timing = false;
	
	spp = 1;
	sample_cl single_sample = {0.f,0.f,1.f};
	if (create_filled_cl_mem(clinfo, 
				 CL_MEM_READ_ONLY,
				 spp * sizeof(sample_cl),
				 &single_sample,
				 &samples_mem))
		return false;
	

	return true;
	
}


bool 
PrimaryRayGenerator::set_rays(const Camera& cam, RayBundle& bundle, uint32_t size[2],
			      const int32_t ray_count, const int32_t offset)
{

	cl_int err;

	if (timing)
		timer.snap_time();

	/*-------------- Set cam parameters as arguments ------------------*/
	/*-- My OpenCL implementation cannot handle using float3 as arguments!--*/
	cl_float4 cam_pos = vec3_to_float4(cam.pos);
	cl_float4 cam_dir = vec3_to_float4(cam.dir);
	cl_float4 cam_right = vec3_to_float4(cam.right);
	cl_float4 cam_up = vec3_to_float4(cam.up);

	err = clSetKernelArg(ray_clk.kernel, 1, 
			     sizeof(cl_float4), &cam_pos);
	if (error_cl(err, "clSetKernelArg 1"))
		return false;

	err = clSetKernelArg(ray_clk.kernel, 2, 
			     sizeof(cl_float4), &cam_dir);
	if (error_cl(err, "clSetKernelArg 2"))
		return false;

	err = clSetKernelArg(ray_clk.kernel, 3, 
			     sizeof(cl_float4), &cam_right);
	if (error_cl(err, "clSetKernelArg 3"))
		return false;

	err = clSetKernelArg(ray_clk.kernel, 4, 
			     sizeof(cl_float4), &cam_up);
	if (error_cl(err, "clSetKernelArg 4"))
		return false;

	cl_int width = size[0];
	err = clSetKernelArg(ray_clk.kernel, 5, 
			     sizeof(cl_int), &width);
	if (error_cl(err, "clSetKernelArg 5"))
		return false;

	cl_int height = size[1];
	err = clSetKernelArg(ray_clk.kernel, 6, 
			     sizeof(cl_int), &height);
	if (error_cl(err, "clSetKernelArg 6"))
		return false;

	err = clSetKernelArg(ray_clk.kernel, 7, 
			     sizeof(cl_int), &spp);
	if (error_cl(err, "clSetKernelArg 7"))
		return false;

	err = clSetKernelArg(ray_clk.kernel, 8, 
			     sizeof(cl_mem), &samples_mem);
	if (error_cl(err, "clSetKernelArg 8"))
		return false;


	ray_clk.global_work_size[0] = ray_count;
	ray_clk.global_work_offset[0] = offset;

	/*------------------ Set ray mem object as argument ------------*/

	err = clSetKernelArg(ray_clk.kernel, 0,
			     sizeof(cl_mem),&bundle.mem());

	if (error_cl(err, "clSetKernelArg 4"))
		return false;

	/*------------------- Execute kernel to create rays ------------*/
	if (execute_cl(ray_clk))
		return false;

	if (timing)
		time_ms = timer.msec_since_snap();

	return true;
}

bool 
PrimaryRayGenerator::set_spp(int _spp, sample_cl const* samples)
{
	cl_int err;
	if (_spp < 1)
		return false;

	spp = _spp;

	err = clReleaseMemObject(samples_mem);
	if (error_cl(err, "clReleaseMemObject"))
		return false;

	if (create_filled_cl_mem(*(ray_clk.clinfo), 
				 CL_MEM_READ_ONLY,
				 spp * sizeof(sample_cl),
				 samples,
				 &samples_mem))
		return false;
		
	return true;
}

int
PrimaryRayGenerator::get_spp() const
{
	return spp;
}

cl_mem&
PrimaryRayGenerator::get_samples() 
{
	return samples_mem;
}

const cl_mem&
PrimaryRayGenerator::get_samples() const
{
	return samples_mem;
}


void 
PrimaryRayGenerator::enable_timing(bool b)
{
	timing = b;
}

double 
PrimaryRayGenerator::get_exec_time()
{
	return time_ms;
}
