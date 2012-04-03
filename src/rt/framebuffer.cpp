#include <rt/framebuffer.hpp>
#include <rt/material.hpp>

bool 
FrameBuffer::initialize(CLInfo& clinfo, uint32_t sz[2])
{
        if (device.initialize(clinfo))
                return false;

	/*---------------------- Create image mem ----------------------*/
	size[0] = sz[0];
	size[1] = sz[1];
	if (size[0] <= 0 || size[1] <= 0)
		return false;

	int32_t img_mem_size = sizeof(color_int_cl) * size[0] * size[1];

        img_mem_id = device.new_memory();
        
        DeviceMemory& img_mem = device.memory(img_mem_id);
        if (img_mem.initialize(img_mem_size, READ_WRITE_MEMORY))
                return false;

	/*------------------------ Set up image init kernel info ---------------------*/
        init_id = device.new_function();
        DeviceFunction& init_function = device.function(init_id);

        if (init_function.initialize("src/kernel/framebuffer.cl", "init"))
                return false;
        
        init_function.set_dims(1);
        size_t init_work_size[] = {size[0] * size[1], 0, 0};
        init_function.set_global_size(init_work_size);

	/* Arguments */

        if (init_function.set_arg(0, img_mem))
                return false;

	/*------------------------ Set up image copy kernel info ---------------------*/
        copy_id = device.new_function();
        DeviceFunction& copy_function = device.function(copy_id);

        if (copy_function.initialize("src/kernel/framebuffer.cl", "copy"))
            return false;
            
        copy_function.set_dims(1);
        size_t copy_work_size[] = {size[0] * size[1], 0, 0};
        copy_function.set_global_size(copy_work_size);


	/* Arguments */
        if (copy_function.set_arg(0, img_mem))
                return false;

	timing = false;
	return true;
}

bool 
FrameBuffer::clear()
{
	if (timing)
		clear_timer.snap_time();

	bool ret = !device.function(init_id).execute();

	if (timing)
		clear_time_ms= clear_timer.msec_since_snap();

	return ret;
}


bool 
FrameBuffer::copy(DeviceMemory& tex_mem)
{

	if (timing)
		copy_timer.snap_time();

        
        if (device.function(copy_id).set_arg(1, tex_mem))
                return false;

	// err = clSetKernelArg(copy_clk.kernel,1,sizeof(cl_mem),&tex_mem);
	// if (error_cl(err, "clSetKernelArg 1"))
	// 	return false;

	bool ret = !device.function(copy_id).execute();
	
	if (timing)
		copy_time_ms= copy_timer.msec_since_snap();

	return ret;
}

void 
FrameBuffer::enable_timing(bool b)
{
	timing = b;
}

double
FrameBuffer::get_clear_exec_time()
{
	return clear_time_ms;
}
double
FrameBuffer::get_copy_exec_time()
{
	return copy_time_ms;
}
