#include <gpu/memory.hpp>

DeviceMemory::DeviceMemory() :
        m_initialized(false)
{
}

bool DeviceMemory::valid() const
{
        return m_initialized;
}

int32_t DeviceMemory::initialize(size_t size, DeviceMemoryMode mode)
{
        CLInfo* clinfo = CLInfo::instance();
        if (!clinfo->initialized() || m_initialized)
                return -1;

        cl_mem_flags flags;
        switch (mode)
        {
        case(READ_ONLY_MEMORY):
                flags = CL_MEM_READ_ONLY;
                break;
        case(WRITE_ONLY_MEMORY):
                flags = CL_MEM_WRITE_ONLY;
                break;
        case(READ_WRITE_MEMORY):
                flags = CL_MEM_READ_WRITE;
                break;
        default:
                return -1;
        }

	cl_int err;
	m_mem = clCreateBuffer(clinfo->context,
                               flags,
                               size,
                               NULL,
                               &err);
	if (error_cl(err, "clCreateBuffer"))
		return -1;

        m_mode = mode;
        m_initialized = true;
        m_size = size;
	return 0;
        
}
int32_t DeviceMemory::initialize(size_t size, const void* values, DeviceMemoryMode mode)
{
        CLInfo* clinfo = CLInfo::instance();
        if (!clinfo->initialized() || m_initialized)
                return -1;

        cl_mem_flags flags;
        switch (mode)
        {
        case(READ_ONLY_MEMORY):
                flags = CL_MEM_READ_ONLY;
                break;
        case(WRITE_ONLY_MEMORY):
                flags = CL_MEM_WRITE_ONLY;
                break;
        case(READ_WRITE_MEMORY):
                flags = CL_MEM_READ_WRITE;
                break;
        default:
                return -1;
        }

	cl_int err;
	m_mem = clCreateBuffer(clinfo->context,
                               flags | CL_MEM_COPY_HOST_PTR,
                               size,
                               const_cast<void*>(values), //This is ugly but necessary
                               // Khronos, why u not have a const variant ?!?!
                               &err);
	if (error_cl(err, "clCreateBuffer"))
		return 1;

        m_mode = mode;
        m_initialized = true;
        m_size = size;
	return 0;

}

int32_t DeviceMemory::initialize_from_gl_texture(const GLuint gl_tex)
{
        CLInfo* clinfo = CLInfo::instance();
        if (!clinfo->initialized() || m_initialized)
                return -1;

        cl_int err;

        m_mem = clCreateFromGLTexture2D(clinfo->context,
                                        CL_MEM_READ_WRITE,
                                        GL_TEXTURE_2D,0,
                                        gl_tex,&err);

	if (error_cl(err, "clCreateFromGLTexture2D"))
		return -1;

        m_mode = READ_WRITE_MEMORY;
        m_initialized = true;
        /* TODO: set appropiate size! */
        return 0;
}

size_t DeviceMemory::write(size_t nbytes, const void* values, size_t offset, 
                           size_t command_queue_i)
{
        CLInfo* clinfo = CLInfo::instance();
        if (!clinfo->initialized() || !clinfo->has_command_queue(command_queue_i))
                return -1;

        if (!valid())
                return -1;

        cl_command_queue command_queue = clinfo->get_command_queue(command_queue_i);

	cl_int err;
	err = clEnqueueWriteBuffer(command_queue,
				   m_mem,
				   true,  /* Blocking write */
				   offset,
				   nbytes,
				   const_cast<void*>(values),
				   0,        /*Not using event lists for now*/
				   NULL,
				   NULL);
	if (error_cl(err, "clEnqueueWriteBuffer"))
	    return -1;


	err = clFinish(command_queue);
	if (error_cl(err, "clFinish"))
		return -1;

	return 0;
}

size_t DeviceMemory::read(size_t nbytes, void* buffer, 
                          size_t offset, size_t command_queue_i)
{
        CLInfo* clinfo = CLInfo::instance();
        if (!clinfo->initialized() || !clinfo->has_command_queue(command_queue_i))
                return -1;

        if (!valid())
                return -1;

        cl_command_queue command_queue = clinfo->get_command_queue(command_queue_i);
	cl_int err;
	err = clEnqueueReadBuffer(command_queue,
				  m_mem,
				  true,  /* Blocking read */
				  offset, /* Offset */
				  nbytes,  /* Bytes to read */
				  buffer, /* Pointer to write to */
				  0,        /*Not using event lists for now*/
				  NULL,
				  NULL);
	if (error_cl(err, "clEnqueueReadBuffer"))
	    return -1;

	err = clFinish(command_queue);
	if (error_cl(err, "clFinish"))
		return -1;

	return 0;
}

int32_t
DeviceMemory::resize(size_t new_size)
{
        if (!new_size)
                return -1;

        if (valid() && release())
                return -1;

        CLInfo* clinfo = CLInfo::instance();
        if (!clinfo->initialized())
                return -1;

        cl_int err;
        cl_mem_flags flags;
        switch (m_mode)
        {
        case(READ_ONLY_MEMORY):
                flags = CL_MEM_READ_ONLY;
                break;
        case(WRITE_ONLY_MEMORY):
                flags = CL_MEM_WRITE_ONLY;
                break;
        case(READ_WRITE_MEMORY):
                flags = CL_MEM_READ_WRITE;
                break;
        default:
                return -1;
        }

	m_mem = clCreateBuffer(clinfo->context,
                               flags,
                               new_size,
                               NULL,
                               &err);
	if (error_cl(err, "clCreateBuffer"))
		return -1;

        m_initialized = true;
        m_size = new_size;
        return 0;
}

int32_t 
DeviceMemory::copy_to(DeviceMemory& dst, size_t bytes, size_t offset, 
                      size_t dst_offset, size_t command_queue_i)
{
        CLInfo* clinfo = CLInfo::instance();
        if (!clinfo->initialized() || !clinfo->has_command_queue(command_queue_i))
                return -1;

        cl_command_queue command_queue = clinfo->get_command_queue(command_queue_i);

        if (!valid() || !dst.valid())
                return -1;

        if (bytes == 0)
                bytes = size();

        if (bytes + offset   > size() ||
            bytes + dst_offset > dst.size())
                return -1;

        cl_int err;
        err = clEnqueueCopyBuffer(command_queue,
                                  m_mem,
                                  *dst.ptr(),
                                  offset,
                                  dst_offset,
                                  bytes,
                                  NULL,NULL,NULL);

        if (error_cl(err, "clEnqueueCopyBuffer"))
                return -1;

        err = clFinish(command_queue);
        if (error_cl(err, "clFinish"))
                return -1;

        return 0;

}

int32_t 
DeviceMemory::copy_all_to(DeviceMemory& dst, size_t command_queue_i)
{
        return copy_to(dst, 0, 0, 0, command_queue_i);
}

int32_t
DeviceMemory::release()
{
        if (!valid())
                return -1;

        cl_int err;
        err = clReleaseMemObject(m_mem);	
        if (error_cl(err, "clReleaseMemObject"))
	    return -1;
        m_initialized = false;
        return 0;
}

size_t
DeviceMemory::size() const
{
        if (!valid())
                return 0;
        return cl_mem_size(m_mem);
}

cl_mem* 
DeviceMemory::ptr()
{
        if (!valid())
                return NULL;
        return &m_mem;
}
