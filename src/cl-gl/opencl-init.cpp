#include <cl-gl/opencl-init.hpp>

#include <fstream>
#include <iostream>
#include <cstring>

CLInfo* CLInfo::pinstance = 0;

CLInfo::CLInfo () :
  m_initialized(false)
, m_sync(false)
{
}

CLInfo* CLInfo::instance() 
{
        if (!pinstance)
                pinstance = new CLInfo();
        return pinstance;
}

void CLInfo::release_resources()
{
        if (!m_initialized) return;
        for (size_t i = 0; i < command_queues.size(); ++i) {
                clReleaseCommandQueue(command_queues[i]);
        }
        command_queues.clear();
        clReleaseContext(context);
}


void CLInfo::set_sync(bool s) 
{
        m_sync = s;
}

bool CLInfo::sync() 
{
        return m_sync;
}

bool CLInfo::has_command_queue(size_t i)
{
        return i < command_queues.size();
}

cl_command_queue CLInfo::get_command_queue(size_t i)
{
        if (i < command_queues.size())
                return command_queues[i];

        return cl_command_queue();
}

bool CLInfo::initialized()
{
        return m_initialized;
}

cl_int CLInfo::initialize(size_t requested_command_queues)
{
        if (m_initialized || 
            requested_command_queues < 1 || 
            requested_command_queues > 1024)
                return CL_DEVICE_NOT_FOUND;

        GLInfo* glinfo = GLInfo::instance();
        if (!glinfo->initialized())
                return CL_DEVICE_NOT_FOUND;
        
	cl_int err;
	size_t bytes_returned;
	
	//retrieve the list of platforms available
	std::cout << "Retrieving the list of platforms available" << std::endl;
	err = clGetPlatformIDs(1, &platform_id, &num_of_platforms);

	if (error_cl(err, "glGetPlatformIDs"))
		return err;

	//try to get a supported gpu device
	std::cout << "Trying to get a supported gpu device" << std::endl;
	err = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_GPU, 
			     1, &device_id,
			     &num_of_devices);
	if (error_cl(err, "clGetDeviceIDs"))
		return err;

	cl_int prop_count = 0;
#ifdef _WIN32
	properties[prop_count++] = CL_CONTEXT_PLATFORM;
	properties[prop_count++] = 
		(cl_context_properties) platform_id;
	properties[prop_count++] = 
		CL_GL_CONTEXT_KHR;
	properties[prop_count++] = 
		(cl_context_properties)glinfo->renderingContext;
	properties[prop_count++] = CL_WGL_HDC_KHR;
	properties[prop_count++] = 
		(cl_context_properties)glinfo->deviceContext;
	properties[prop_count++] = 0;
#elif defined __linux__
	properties[prop_count++] = CL_CONTEXT_PLATFORM;
	properties[prop_count++] = 
		(cl_context_properties)platform_id;
	properties[prop_count++] = 
		CL_GL_CONTEXT_KHR;
	properties[prop_count++] = 
		(cl_context_properties)glinfo->renderingContext;
	properties[prop_count++] = CL_GLX_DISPLAY_KHR;
	properties[prop_count++] = 
		(cl_context_properties)glinfo->renderingDisplay;
	properties[prop_count++] = 0;
#else
#error "UNKNOWN PLATFORM"
#endif

	//properties[0] = CL_CONTEXT_PLATFORM;
	//properties[1] = (cl_context_properties) platform_id;
	//properties[2] = 0;

	//create a context with the GPU device
	std::cerr << "Creating a context with the GPU device" << std::endl;
	context = 
		clCreateContext (properties,1,&device_id,NULL,NULL,&err);
	if (error_cl(err, "clCreateContext"))
		return err;

	//create command queues using the context and device
	std::cerr << "Creating command queues using the context and device" << std::endl;
        for (size_t i = 0; i < requested_command_queues; ++i) {
                cl_command_queue cq = 
                        clCreateCommandQueue(context,
                                             device_id,
                                             0, //CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE,
                                             &err);
                if (error_cl(err, "clCreateCommandQueue"))
                        return err;
                command_queues.push_back(cq);
        }


	// Get size of global memory in the device
	err = clGetDeviceInfo (device_id,
			       CL_DEVICE_GLOBAL_MEM_SIZE, 
			       sizeof(cl_ulong),
			       &global_mem_size, 
			       &bytes_returned);
	if (error_cl(err, "clGetDeviceInfo CL_DEVICE_GLOBAL_MEM_SIZE"))
	    return err;

	// Get size of local memory in the device
	err = clGetDeviceInfo (device_id,
			       CL_DEVICE_LOCAL_MEM_SIZE, 
			       sizeof(cl_ulong),
			       &local_mem_size, 
			       &bytes_returned);
	if (error_cl(err, "clGetDeviceInfo CL_DEVICE_LOCAL_MEM_SIZE"))
	    return err;

	// Get bool stating if cl device supports images
	err = clGetDeviceInfo (device_id,
			       CL_DEVICE_IMAGE_SUPPORT, 
			       sizeof(cl_bool),
			       &image_support, 
			       &bytes_returned);
	if (error_cl(err, "clGetDeviceInfo CL_DEVICE_IMAGE_SUPPORT"))
	    return err;
	
	// Get max image2d height
	err = clGetDeviceInfo (device_id,
			       CL_DEVICE_IMAGE2D_MAX_HEIGHT, 
			       sizeof(size_t),
			       &image2d_max_height, 
			       &bytes_returned);
	if (error_cl(err, "clGetDeviceInfo CL_DEVICE_IMAGE2D_MAX_HEIGHT"))
	    return err;
	
	// Get max image2d width
	err = clGetDeviceInfo (device_id,
			       CL_DEVICE_IMAGE2D_MAX_WIDTH, 
			       sizeof(size_t),
			       &image2d_max_width, 
			       &bytes_returned);
	if (error_cl(err, "clGetDeviceInfo CL_DEVICE_IMAGE2D_MAX_WIDTH"))
	    return err;

	// Get amount of compute units in the device
	err = clGetDeviceInfo (device_id,
			       CL_DEVICE_MAX_COMPUTE_UNITS, 
			       sizeof(cl_uint),
			       &max_compute_units, 
			       &bytes_returned);
	if (error_cl(err, "clGetDeviceInfo CL_DEVICE_MAX_COMPUTE_UNITS"))
	    return err;

	// Get maximum size of global mem objects for the device
	err = clGetDeviceInfo (device_id,
			       CL_DEVICE_MAX_MEM_ALLOC_SIZE, 
			       sizeof(cl_ulong),
			       &max_mem_alloc_size, 
			       &bytes_returned);
	if (error_cl(err, "clGetDeviceInfo CL_DEVICE_MAX_MEM_ALLOC_SIZE"))
	    return err;

	// Get maximum size of a work group
	err = clGetDeviceInfo (device_id,
			       CL_DEVICE_MAX_WORK_GROUP_SIZE, 
			       sizeof(size_t),
			       &max_work_group_size, 
			       &bytes_returned);
	if (error_cl(err, "clGetDeviceInfo CL_DEVICE_MAX_WORK_GROUP_SIZE"))
	    return err;

	// Get maximum number of items that can be specified in each dimension
	err = clGetDeviceInfo (device_id,
			       CL_DEVICE_MAX_WORK_ITEM_SIZES, 
			       sizeof(max_work_item_sizes),
			       &max_work_item_sizes, 
			       &bytes_returned);
	if (error_cl(err, "clGetDeviceInfo CL_DEVICE_MAX_WORK_ITEM_SIZES"))
	    return err;

	m_initialized = true;
	return CL_SUCCESS;
}


void CLInfo::print_info()
{
        if (!m_initialized)
                return;

	cl_int err;
	cl_uint max_samplers;
	char device_vendor[128], device_name[128], device_cl_version[128], c_version[128];
	size_t bytes_returned;
	err = clGetDeviceInfo (device_id,
			       CL_DEVICE_VENDOR, sizeof(device_vendor),
			       device_vendor, &bytes_returned);
	if (error_cl(err, "clGetDeviceInfo CL_DEVICE_VENDOR"))
	    return;

	err = clGetDeviceInfo (device_id,
			       CL_DEVICE_VERSION, sizeof(device_cl_version),
			       device_cl_version, &bytes_returned);

	if (error_cl(err, "clGetDeviceInfo CL_DEVICE_VERSION"))
	    return;

	err = clGetDeviceInfo (device_id,
			       CL_DEVICE_NAME, sizeof(device_name),
			       device_name, &bytes_returned);

	if (error_cl(err, "clGetDeviceInfo CL_DEVICE_NAME"))
	    return;

	err = clGetDeviceInfo (device_id,
			       CL_DEVICE_OPENCL_C_VERSION, sizeof(c_version),
			       c_version, &bytes_returned);

	if (error_cl(err, "clGetDeviceInfo CL_DEVICE_OPENCL_C_VERSION"))
	    return;

	err = clGetDeviceInfo (device_id,
			       CL_DEVICE_MAX_SAMPLERS, sizeof(max_samplers),
			       &max_samplers, &bytes_returned);

	if (error_cl(err, "clGetDeviceInfo CL_DEVICE_MAX_SAMPLERS"))
	    return;

	std::cout << "OpenCL Info:" << std::endl;

	std::cout << "\tOpenCL device vendor: " << device_vendor << std::endl;
	std::cout << "\tOpenCL device name: " << device_name << std::endl;
	std::cout << "\tOpenCL device OpenCL version: " << device_cl_version << std::endl;
	std::cout << "\tOpenCL device OpenCL C version: " << c_version << std::endl;
	std::cout << "\tOpenCL device maximum sampler support: " 
		  << max_samplers << std::endl;
	std::cout << "\tOpenCL device global mem size: " 
		  << global_mem_size << std::endl;
	std::cout << "\tOpenCL device local mem size: " 
		  << local_mem_size << std::endl;
	std::cout << "\tOpenCL device maximum mem alloc size: " 
		  << max_mem_alloc_size << std::endl;
	std::cout << "\tOpenCL device image2d max size: " 
		  << image2d_max_width << " x "
		  << image2d_max_width << std::endl;
	std::cout << "\tOpenCL device compute units: " 
		  << max_compute_units << std::endl;
	std::cout << "\tOpenCL device max work group size: " 
		  << max_work_group_size << std::endl;
	std::cout << "\tOpenCL device max work item sizes per dimension: " 
		  << max_work_item_sizes[0] << " "
		  << max_work_item_sizes[1] << " "
		  << max_work_item_sizes[2] << std::endl;
	

	std::cout << std::endl;
}

int32_t
init_cl_kernel(const char* kernel_file, 
	       std::string kernel_name,
	       CLKernelInfo* clkernelinfo)
{
	cl_int err;

        CLInfo* clinfo = CLInfo::instance();
        if (!clinfo->initialized())
                return -1;

	clkernelinfo->clinfo = clinfo;

	//create a program from the kernel source code
	std::ifstream kernel_source_file(kernel_file);
	if (!kernel_source_file.good()){
		std::cerr << "Error: could not read kernel file." << std::endl;
		return 1;
	}

	std::streampos file_size;
	kernel_source_file.seekg (0, std::ios::beg);
	file_size = kernel_source_file.tellg();
	kernel_source_file.seekg (0, std::ios::end);
	file_size = kernel_source_file.tellg() - file_size;

	char *kernel_source = new char[(int)file_size+1];
	std::memset(kernel_source,0,file_size);

	kernel_source_file.seekg (0, std::ios::beg);
	kernel_source_file.read(kernel_source,file_size);
	kernel_source_file.close();
	kernel_source[file_size] = 0;
	
	clkernelinfo->program = 
		clCreateProgramWithSource(clinfo->context,1,
					  (const char**)&kernel_source,NULL,&err);

	delete[] kernel_source;
	if (error_cl(err, "clCreateProgramWithSource"))
		return 1;

	err = clBuildProgram(clkernelinfo->program,
		0,
		NULL,
		NULL,
		NULL,
		NULL);
	if (error_cl(err, "clBuildProgram")){
		char build_log[8196];
		size_t bytes_returned;
		err = clGetProgramBuildInfo (clkernelinfo->program,
					     clkernelinfo->clinfo->device_id,
					     CL_PROGRAM_BUILD_LOG,
					     sizeof(build_log),
					     build_log,
					     &bytes_returned);

		if (!error_cl(err, "clGetProgramBuildInfo")){
			std::cerr << "Program build error log:" << std::endl
				  << build_log << std::endl;
		}
		return 1;
	}

	//specify which kernel from the program to execute
	clkernelinfo->kernel = clCreateKernel(clkernelinfo->program,
					      kernel_name.c_str(),&err);
	if (error_cl(err, "clCreateKernel"))
		return 1;

	return 0;

}

int32_t execute_cl(const CLKernelInfo& clkernelinfo, size_t cq_i){

	cl_int err;
	
        CLInfo* clinfo = CLInfo::instance();
        if (!clinfo->initialized() || !clinfo->has_command_queue(cq_i))
                return -1;

	//enqueue the kernel command for execution

	if (clkernelinfo.work_dim < 1 || clkernelinfo.work_dim > 3) {
		std::cerr << "Kernel work dimensions not set" << std::endl;
		return 1;
	}

	if (clkernelinfo.arg_count < 0) {
		std::cerr << "Kernel argument count not set" << std::endl;
		return 1;
	}

	for (int8_t i = 0; i < clkernelinfo.work_dim; ++i) {
		if (clkernelinfo.global_work_size[i] <= 0) {
			std::cerr << "Kernel global work size not set" << std::endl;
			return 1;
		}
	}

	bool local_size_set = true;

	for (int8_t i = 0; i < clkernelinfo.work_dim; ++i)
		local_size_set = local_size_set && (clkernelinfo.local_work_size[i] > 0);

	// std::cerr << "Enqueueing the kernel command for execution" << std::endl;
	if (local_size_set)
		err = clEnqueueNDRangeKernel(clinfo->get_command_queue(cq_i),
					     clkernelinfo.kernel,
					     clkernelinfo.work_dim,
					     clkernelinfo.global_work_offset,
					     clkernelinfo.global_work_size,
					     clkernelinfo.local_work_size,
					     0,NULL,NULL);
	else
		err = clEnqueueNDRangeKernel(clinfo->get_command_queue(cq_i),
					     clkernelinfo.kernel,
					     clkernelinfo.work_dim,
					     clkernelinfo.global_work_offset,
					     clkernelinfo.global_work_size,
					     NULL,
					     0,NULL,NULL);

	if (error_cl(err, "clEnqueueNDRangeKernel"))
		return 1;

	/* finish command queue */
	err = clFinish(clinfo->get_command_queue(cq_i));
	if (error_cl(err, "clFinish"))
		return 1;

	return 0;	
}


int32_t create_empty_cl_mem(cl_mem_flags flags, 
                            uint32_t size, cl_mem* mem)
{
        CLInfo* clinfo = CLInfo::instance();
        if (!clinfo->initialized())
                return -1;

	cl_int err;
	*mem = clCreateBuffer(clinfo->context,
			     flags,
			     size,
			     NULL,
			     &err);
	if (error_cl(err, "clCreateBuffer"))
		return 1;
	return 0;
}

int32_t create_filled_cl_mem(cl_mem_flags flags, uint32_t size, 
			 const void* values, cl_mem* mem)
{
        CLInfo* clinfo = CLInfo::instance();
        if (!clinfo->initialized())
                return -1;

	cl_int err;
	*mem = clCreateBuffer(clinfo->context,
			     flags | CL_MEM_COPY_HOST_PTR,
			     size,
			     const_cast<void*>(values), //This is ugly but necessary
			      // Kronos, why u not have a const variant ?!?!
			     &err);
	if (error_cl(err, "clCreateBuffer"))
		return 1;
	return 0;

}

int32_t create_host_cl_mem(cl_mem_flags flags, uint32_t size, 
			   void* ptr, cl_mem* mem)
{
        CLInfo* clinfo = CLInfo::instance();
        if (!clinfo->initialized())
                return -1;

	cl_int err;
	*mem = clCreateBuffer(clinfo->context,
			      flags | CL_MEM_USE_HOST_PTR,
			      size,
			      ptr, //This is ugly but necessary
			      // Kronos, why u not have a const variant ?!?!
			      &err);
	if (error_cl(err, "clCreateBuffer"))
		return 1;
	return 0;

}

int32_t copy_to_cl_mem(uint32_t size, const void* values, cl_mem& mem, 
                       uint32_t offset, size_t cq_i){

        CLInfo* clinfo = CLInfo::instance();
        if (!clinfo->initialized() || !clinfo->has_command_queue(cq_i))
                return -1;

	cl_int err;
	err = clEnqueueWriteBuffer(clinfo->get_command_queue(cq_i),
				   mem,
				   true,  /* Blocking write */
				   offset,
				   size,
				   values,
				   0,        /*Not using event lists for now*/
				   NULL,
				   NULL);
	if (error_cl(err, "clEnqueueWriteBuffer"))
	    return 1;


	err = clFinish(clinfo->get_command_queue(cq_i));
	if (error_cl(err, "clFinsish"))
		return 1;

	return 0;
}

int32_t copy_from_cl_mem(uint32_t size, void* values, cl_mem& mem, 
                         uint32_t offset, size_t cq_i)
{

        CLInfo* clinfo = CLInfo::instance();
        if (!clinfo->initialized() || !clinfo->has_command_queue(cq_i))
                return -1;

	cl_int err;
	err = clEnqueueReadBuffer(clinfo->get_command_queue(cq_i),
				  mem,
				  true,  /* Blocking write */
				  offset, /* Offset */
				  size,  /* Bytes to read */
				  values, /* Pointer to write to */
				  0,        /*Not using event lists for now*/
				  NULL,
				  NULL);
	if (error_cl(err, "clEnqueueReadBuffer"))
	    return 1;

	err = clFinish(clinfo->get_command_queue(cq_i));
	if (error_cl(err, "clFinsish"))
		return 1;

	return 0;
}


void print_cl_mem_info(const cl_mem& mem){

        CLInfo* clinfo = CLInfo::instance();
        if (!clinfo->initialized())
                return;

	size_t mem_size, bytes_written;
	cl_mem_object_type mem_type;
	cl_int err;

	err = clGetMemObjectInfo(mem,
				 CL_MEM_SIZE,
				 sizeof(size_t),
				 &mem_size,
				 &bytes_written);

	if (error_cl(err, "clGetMemObjectInfo CL_MEM_SIZE"))
		return;

	std::cerr << "CL mem object size: " << mem_size << std::endl;

	err = clGetMemObjectInfo(mem,
				 CL_MEM_TYPE,
				 sizeof(cl_mem_object_type),
				 &mem_type,
				 &bytes_written);

	if (error_cl(err, "clGetMemObjectInfo CL_MEM_TYPE"))
		return;

	std::cerr << "CL mem type: ";
	switch (mem_type) {
	case CL_MEM_OBJECT_BUFFER:
		std::cerr << "CL_MEM_OBJECT_BUFFER" << std::endl;
		break;
	case CL_MEM_OBJECT_IMAGE2D:
		std::cerr << "CL_MEM_OBJECT_IMAGE2D" << std::endl;
		break;
	case CL_MEM_OBJECT_IMAGE3D:
		std::cerr << "CL_MEM_OBJECT_IMAGE3D" << std::endl;
		break;
	default:
		std::cerr << "UNKNOWN" << std::endl;
		break;
	}


}

void print_cl_image_2d_info(const cl_mem& mem)
{
	// Query image created from texture
        CLInfo* clinfo = CLInfo::instance();
        if (!clinfo->initialized())
                return;

	cl_int err;
	size_t img_width, img_height, img_depth;
	cl_image_format img_format;
	size_t bytes_written;

	err = clGetImageInfo(mem,
			     CL_IMAGE_WIDTH,
			     sizeof(size_t),
			     &img_width,
			     &bytes_written);

	if (error_cl(err, "clGetImageInfo CL_IMAGE_WIDTH"))
		return;

	err = clGetImageInfo(mem,
			     CL_IMAGE_HEIGHT,
			     sizeof(size_t),
			     &img_height,
			     &bytes_written);

	if (error_cl(err, "clGetImageInfo CL_IMAGE_HEIGHT"))
		return;

	err = clGetImageInfo(mem,
			     CL_IMAGE_DEPTH,
			     sizeof(size_t),
			     &img_depth,
			     &bytes_written);

	if (error_cl(err, "clGetImageInfo CL_IMAGE_DEPTH"))
		return;

	err = clGetImageInfo(mem,
			     CL_IMAGE_FORMAT,
			     sizeof(cl_image_format),
			     &img_format,
			     &bytes_written);

	if (error_cl(err, "clGetImageInfo CL_IMAGE_FORMAT"))
		return;

	std::cout << "OpenCL image from texture info: " << std::endl;
	std::cout << "\tTexture image width: " << img_width << std::endl;
	std::cout << "\tTexture image height: " << img_height << std::endl;
	std::cout << "\tTexture image depth: " << img_depth << std::endl;
	std::cout << "\tTexture image channel order: " 
		  << img_format.image_channel_order << std::endl;
	std::cout << "\tTexture image channel data type: " 
		  << img_format.image_channel_data_type << std::endl;

}

int create_cl_mem_from_gl_tex(const GLuint gl_tex, cl_mem* mem, size_t cq_i)
{
        CLInfo* clinfo = CLInfo::instance();
        if (!clinfo->initialized() || !clinfo->has_command_queue(cq_i))
                return -1;

	cl_int err;

	*mem = clCreateFromGLTexture2D(clinfo->context,
				       CL_MEM_READ_WRITE,
				       GL_TEXTURE_2D,0,
				       gl_tex,&err);
	
	if (error_cl(err, "clCreateFromGLTexture2D"))
		return 1;

	err = clEnqueueAcquireGLObjects(clinfo->get_command_queue(cq_i),
					1,
					mem,
					0,0,0);

	if (error_cl(err, "clEnqueueAcquireGLObjects"))
		return 1;

	err = clFinish(clinfo->get_command_queue(cq_i));

	if (error_cl(err, "clFinish"))
		return 1;

	return 0;
}

int32_t __error_cl(cl_int err_num, std::string msg, 
                   const char* file, const char* func,  int line)
{
	if (err_num != CL_SUCCESS){
		std::cerr << " *** OpenCL error: " << msg 
			  << " (error code " << err_num << ")" 
                          << std::endl
                          << " Function: " << func  
                          << " (File: " << file << " Line: " << line  << ")"
                          << std::endl;
		return 1;
	}
	return 0;
}

size_t cl_mem_size(const cl_mem& mem)
{
	size_t mem_size, bytes_written;
	cl_int err;
	

	err = clGetMemObjectInfo(mem,
				 CL_MEM_SIZE,
				 sizeof(size_t),
				 &mem_size,
				 &bytes_written);

	if (error_cl(err, "clGetMemObjectInfo CL_MEM_SIZE"))
		return -1;

	return mem_size;
}

int32_t acquire_gl_tex(cl_mem& tex_mem, size_t cq_i)
{
        CLInfo* clinfo = CLInfo::instance();
        if (!clinfo->initialized() || !clinfo->has_command_queue(cq_i))
                return -1;

        cl_int err;

        err = clEnqueueAcquireGLObjects (clinfo->get_command_queue(cq_i),
                                         1,
                                         &tex_mem,
                                         0,
                                         NULL,
                                         NULL);
	if (error_cl(err, "clEnqueueAcquireGLObjects "))
                return -1;

	err = clFinish(clinfo->get_command_queue(cq_i));

	if (error_cl(err, "clEnqueueAcquireGLObjects -> clFinish"))
                return -1;
        return 0;
}

int32_t release_gl_tex(cl_mem& tex_mem, size_t cq_i)
{
        CLInfo* clinfo = CLInfo::instance();
        if (!clinfo->initialized() || !clinfo->has_command_queue(cq_i))
                return -1;

        cl_int err;

        err = clEnqueueReleaseGLObjects (clinfo->get_command_queue(cq_i),
                                         1,
                                         &tex_mem,
                                         0,
                                         NULL,
                                         NULL);
	if (error_cl(err, "clEnqueueReleaseGLObjects "))
                return -1;

	err = clFinish(clinfo->get_command_queue(cq_i));

	if (error_cl(err, "clEnqueueReleaseGLObjects -> clFinish"))
                return -1;
        return 0;
}

