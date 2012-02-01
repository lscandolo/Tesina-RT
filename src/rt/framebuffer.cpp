#include <rt/framebuffer.hpp>
#include <rt/material.hpp>

bool 
FrameBuffer::initialize(CLInfo& clinfo, uint32_t sz[2],
			SceneInfo& scene_info, cl_mem& cl_hit_mem)
		
{
	cl_int err;

	size[0] = sz[0];
	size[1] = sz[1];

	if (size[0] <= 0 || size[1] <= 0)
		return false;

	/*---------------------- Create image mem ----------------------*/

	int32_t mem_size = sizeof(color_int_cl) * size[0] * size[1];

	if (create_empty_cl_mem(clinfo, 
				CL_MEM_READ_WRITE,
				mem_size,
				&img_mem))
		return false;


	/*------------------------ Set up image init kernel info ---------------------*/
	if (init_cl_kernel(&clinfo,"src/kernel/framebuffer.cl", "init", 
			   &init_clk))
		return false;

	init_clk.work_dim = 1;
	init_clk.arg_count = 1;
	init_clk.global_work_size[0] = size[0] * size[1];

	/* Arguments */

	err = clSetKernelArg(init_clk.kernel,0,sizeof(cl_mem), &img_mem);
	if (error_cl(err, "clSetKernelArg 0"))
		return false;


	/*------------------------ Set up image update kernel info ---------------------*/
	if (init_cl_kernel(&clinfo,"src/kernel/framebuffer.cl", "update", 
			   &update_clk))
		return false;

	update_clk.work_dim = 1;
	update_clk.arg_count = 12;
	update_clk.global_work_size[0] = size[0] * size[1];
	
	/* Arguments */

	err = clSetKernelArg(update_clk.kernel,0,sizeof(cl_mem), &img_mem);
	if (error_cl(err, "clSetKernelArg 0"))
		return false;

	err = clSetKernelArg(update_clk.kernel,1,sizeof(cl_mem),&cl_hit_mem);
	if (error_cl(err, "clSetKernelArg 1"))
		return false;
	
	err = clSetKernelArg(update_clk.kernel,3,sizeof(cl_mem),&scene_info.mat_list_mem());
	if (error_cl(err, "clSetKernelArg 3"))
		return false;

	err = clSetKernelArg(update_clk.kernel,4,sizeof(cl_mem),&scene_info.mat_map_mem());
	if (error_cl(err, "clSetKernelArg 4"))
		return false;

	/*------------------------ Set up image copy kernel info ---------------------*/
	if (init_cl_kernel(&clinfo,"src/kernel/framebuffer.cl", "copy", 
			   &copy_clk))
		return false;

	copy_clk.work_dim = 1;
	copy_clk.arg_count = 2;
	copy_clk.global_work_size[0] = size[0] * size[1];
	
	/* Arguments */
	err = clSetKernelArg(copy_clk.kernel,0,sizeof(cl_mem),&img_mem);
	if (error_cl(err, "clSetKernelArg 0"))
		return false;

	return true;
}

bool 
FrameBuffer::clear()
{
	return !execute_cl(init_clk);
}

bool 
FrameBuffer::update(RayBundle& rays, Cubemap& cm, int32_t work_size, cl_int arg)
{
	cl_int err;

	err = clSetKernelArg(update_clk.kernel,2,sizeof(cl_mem),&rays.mem());
	if (error_cl(err, "clSetKernelArg 2"))
		return false;

	err = clSetKernelArg(update_clk.kernel,5,sizeof(cl_mem),&cm.positive_x_mem());
	if (error_cl(err, "clSetKernelArg 5"))
		return false;

	err = clSetKernelArg(update_clk.kernel,6,sizeof(cl_mem),&cm.negative_x_mem());
	if (error_cl(err, "clSetKernelArg 6"))
		return false;

	err = clSetKernelArg(update_clk.kernel,7,sizeof(cl_mem),&cm.positive_y_mem());
	if (error_cl(err, "clSetKernelArg 7"))
		return false;

	err = clSetKernelArg(update_clk.kernel,8,sizeof(cl_mem),&cm.negative_y_mem());
	if (error_cl(err, "clSetKernelArg 8"))
		return false;

	err = clSetKernelArg(update_clk.kernel,9,sizeof(cl_mem),&cm.positive_z_mem());
	if (error_cl(err, "clSetKernelArg 9"))
		return false;

	err = clSetKernelArg(update_clk.kernel,10,sizeof(cl_mem),&cm.negative_z_mem());
	if (error_cl(err, "clSetKernelArg 10"))
		return false;

	err = clSetKernelArg(update_clk.kernel,11,sizeof(cl_int),&arg);
	if (error_cl(err, "clSetKernelArg 11"))
		return false;

	update_clk.global_work_size[0] = work_size;

	return !execute_cl(update_clk);
}

bool 
FrameBuffer::copy(cl_mem& tex_mem)
{
	cl_int err;

	err = clSetKernelArg(copy_clk.kernel,1,sizeof(cl_mem),&tex_mem);
	if (error_cl(err, "clSetKernelArg 1"))
		return false;

	return !execute_cl(copy_clk);
}
