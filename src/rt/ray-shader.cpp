#include <rt/ray-shader.hpp>

bool 
RayShader::initialize(CLInfo& clinfo)

		
{
	/*------------------------ Set up ray shading kernel info ---------------------*/
	if (init_cl_kernel(&clinfo,"src/kernel/ray-shader.cl", "shade", 
			   &shade_clk))
		return false;

	shade_clk.work_dim = 1;
	shade_clk.arg_count = 12;
	timing = false;
	
	return true;
}

bool 
RayShader::shade(RayBundle& rays, HitBundle& hb, SceneInfo& scene_info, 
		 Cubemap& cm, FrameBuffer& fb, int32_t size)
{
	cl_int err;

	if (timing)
		timer.snap_time();

	err = clSetKernelArg(shade_clk.kernel,0,sizeof(cl_mem), &fb.image_mem());
	if (error_cl(err, "clSetKernelArg 0"))
		return false;

	err = clSetKernelArg(shade_clk.kernel,1,sizeof(cl_mem),&hb.mem());
	if (error_cl(err, "clSetKernelArg 1"))
		return false;

	err = clSetKernelArg(shade_clk.kernel,2,sizeof(cl_mem),&rays.mem());
	if (error_cl(err, "clSetKernelArg 2"))
		return false;

	err = clSetKernelArg(shade_clk.kernel,3,sizeof(cl_mem),&scene_info.mat_list_mem());
	if (error_cl(err, "clSetKernelArg 3"))
		return false;

	err = clSetKernelArg(shade_clk.kernel,4,sizeof(cl_mem),&scene_info.mat_map_mem());
	if (error_cl(err, "clSetKernelArg 4"))
		return false;

	err = clSetKernelArg(shade_clk.kernel,5,sizeof(cl_mem),&cm.positive_x_mem());
	if (error_cl(err, "clSetKernelArg 5"))
		return false;

	err = clSetKernelArg(shade_clk.kernel,6,sizeof(cl_mem),&cm.negative_x_mem());
	if (error_cl(err, "clSetKernelArg 6"))
		return false;

	err = clSetKernelArg(shade_clk.kernel,7,sizeof(cl_mem),&cm.positive_y_mem());
	if (error_cl(err, "clSetKernelArg 7"))
		return false;

	err = clSetKernelArg(shade_clk.kernel,8,sizeof(cl_mem),&cm.negative_y_mem());
	if (error_cl(err, "clSetKernelArg 8"))
		return false;

	err = clSetKernelArg(shade_clk.kernel,9,sizeof(cl_mem),&cm.positive_z_mem());
	if (error_cl(err, "clSetKernelArg 9"))
		return false;

	err = clSetKernelArg(shade_clk.kernel,10,sizeof(cl_mem),&cm.negative_z_mem());
	if (error_cl(err, "clSetKernelArg 10"))
		return false;

	err = clSetKernelArg(shade_clk.kernel,11,sizeof(cl_mem),&scene_info.light_mem());
	if (error_cl(err, "clSetKernelArg 11"))
		return false;

	shade_clk.global_work_size[0] = size;

	if (execute_cl(shade_clk))
		return false;

	if (timing)
		time_ms = timer.msec_since_snap();

	return true;
}

void
RayShader::enable_timing(bool b)
{
	timing = b;
}

double 
RayShader::get_exec_time()
{
	return time_ms;
}
