#include <rt/framebuffer.hpp>
#include <rt/material.hpp>

bool 
FrameBuffer::initialize(CLInfo& clinfo, uint32_t sz[2])
		
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
FrameBuffer::copy(cl_mem& tex_mem)
{
	cl_int err;

	err = clSetKernelArg(copy_clk.kernel,1,sizeof(cl_mem),&tex_mem);
	if (error_cl(err, "clSetKernelArg 1"))
		return false;

	return !execute_cl(copy_clk);
}
