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
	ray_clk.arg_count = 7;
	timing = false;

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
