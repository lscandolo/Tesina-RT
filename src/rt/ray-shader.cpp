#include <rt/ray-shader.hpp>

bool 
RayShader::initialize(CLInfo& clinfo, SceneInfo& scene_info, 
		      FrameBuffer& fb, RayBundle& bundle, 
		      cl_mem& cl_hit_mem)
		
{
	cl_int err;

	if (create_empty_cl_mem(clinfo, 
				CL_MEM_READ_WRITE,
				bundle.count() * sizeof(color_cl),
				&shade_buf))
		return false;

	/*------------------------ Set up ray aggregate kernel info ---------------------*/
	if (init_cl_kernel(&clinfo,"src/kernel/ray-shader.cl", "aggregate", 
			   &aggregate_clk))
		return false;

	aggregate_clk.work_dim = 1;
	aggregate_clk.arg_count = 4;

	err = clSetKernelArg(aggregate_clk.kernel,0,sizeof(cl_mem), &shade_buf); 
	if (error_cl(err, "clSetKernelArg 0"))
		return false;

	err = clSetKernelArg(aggregate_clk.kernel,1,sizeof(cl_mem), &fb.image_mem()); 
	if (error_cl(err, "clSetKernelArg 1"))
		return false;

	/*------------------------ Set up ray shading kernel info ---------------------*/
	if (init_cl_kernel(&clinfo,"src/kernel/ray-shader.cl", "shade", 
			   &shade_clk))
		return false;

	shade_clk.work_dim = 1;
	shade_clk.arg_count = 12;
	
	/* Arguments */

	err = clSetKernelArg(shade_clk.kernel,0,sizeof(cl_mem), &shade_buf); //!!
	// err = clSetKernelArg(shade_clk.kernel,0,sizeof(cl_mem), &fb.image_mem());
	if (error_cl(err, "clSetKernelArg 0"))
		return false;

	err = clSetKernelArg(shade_clk.kernel,1,sizeof(cl_mem),&cl_hit_mem);
	if (error_cl(err, "clSetKernelArg 1"))
		return false;
	
	err = clSetKernelArg(shade_clk.kernel,3,sizeof(cl_mem),&scene_info.mat_list_mem());
	if (error_cl(err, "clSetKernelArg 3"))
		return false;

	err = clSetKernelArg(shade_clk.kernel,4,sizeof(cl_mem),&scene_info.mat_map_mem());
	if (error_cl(err, "clSetKernelArg 4"))
		return false;

	return true;
}

bool 
RayShader::shade(RayBundle& rays, Cubemap& cm, int32_t work_size, cl_int arg)
{
	cl_int err;

	err = clSetKernelArg(shade_clk.kernel,2,sizeof(cl_mem),&rays.mem());
	if (error_cl(err, "clSetKernelArg 2"))
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

	err = clSetKernelArg(shade_clk.kernel,11,sizeof(cl_int),&arg);
	if (error_cl(err, "clSetKernelArg 11"))
		return false;

	shade_clk.global_work_size[0] = work_size;

	if (execute_cl(shade_clk))
		return false;

	err = clSetKernelArg(aggregate_clk.kernel,2,sizeof(cl_mem), &rays.mem()); 
	if (error_cl(err, "clSetKernelArg 1"))
		return false;

	cl_int ray_count = work_size;
	err = clSetKernelArg(aggregate_clk.kernel,3,sizeof(cl_int), &ray_count); 
	if (error_cl(err, "clSetKernelArg 3"))
		return false;

	aggregate_clk.global_work_size[0] = 8;

	if (execute_cl(aggregate_clk))
		return false;

	return true;
}
