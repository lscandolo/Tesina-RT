#include <rt/framebuffer.hpp>
#include <rt/material.hpp>

int32_t 
FrameBuffer::initialize(CLInfo& clinfo, size_t sz[2])
{
        if (device.initialize(clinfo))
                return -1;

	/*---------------------- Create image mem ----------------------*/
	size[0] = sz[0];
	size[1] = sz[1];
	if (size[0] <= 0 || size[1] <= 0)
		return -1;

	int32_t img_mem_size = sizeof(color_int_cl) * size[0] * size[1];

        img_mem_id = device.new_memory();
        
        DeviceMemory& img_mem = device.memory(img_mem_id);
        if (img_mem.initialize(img_mem_size, READ_WRITE_MEMORY))
                return -1;

	/*------------------------ Set up image init kernel info ---------------------*/
        init_id = device.new_function();
        DeviceFunction& init_function = device.function(init_id);

        if (init_function.initialize("src/kernel/framebuffer.cl", "init"))
                return -1;
        
        init_function.set_dims(1);
        size_t init_work_size[] = {size[0] * size[1], 0, 0};
        init_function.set_global_size(init_work_size);

	/* Arguments */

        if (init_function.set_arg(0, img_mem))
                return -1;

	/*------------------------ Set up image copy kernel info ---------------------*/
        copy_id = device.new_function();
        DeviceFunction& copy_function = device.function(copy_id);

        if (copy_function.initialize("src/kernel/framebuffer.cl", "copy"))
            return -1;
            
        copy_function.set_dims(1);
        size_t copy_work_size[] = {size[0] * size[1], 0, 0};
        copy_function.set_global_size(copy_work_size);


	/* Arguments */
        if (copy_function.set_arg(0, img_mem))
                return -1;

	m_timing = false;
	return 0;
}

int32_t
FrameBuffer::clear()
{
	if (m_timing)
		m_clear_timer.snap_time();

	int32_t ret = device.function(init_id).execute();

	if (m_timing)
		m_clear_time_ms= m_clear_timer.msec_since_snap();

	return ret;
}


int32_t 
FrameBuffer::copy(DeviceMemory& tex_mem)
{

	if (m_timing)
		m_copy_timer.snap_time();

        
        if (device.function(copy_id).set_arg(1, tex_mem))
                return -1;

	int32_t ret = device.function(copy_id).execute();
	
	if (m_timing)
		m_copy_time_ms= m_copy_timer.msec_since_snap();

	return ret;
}

void 
FrameBuffer::timing(bool b)
{
	m_timing = b;
}

double
FrameBuffer::get_clear_exec_time()
{
	return m_clear_time_ms;
}
double
FrameBuffer::get_copy_exec_time()
{
	return m_copy_time_ms;
}
