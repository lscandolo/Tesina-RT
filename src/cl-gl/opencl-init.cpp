#include "cl-gl/opencl-init.hpp"

#include <fstream>
#include <iostream>
#include <cstring>

int32_t
init_cl(const GLInfo& glinfo, CLInfo* clinfo)
{
	cl_int err;
	size_t bytes_returned;
	
	//retrieve the list of platforms available
	std::cout << "Retrieving the list of platforms available" << std::endl;
	err = clGetPlatformIDs(1, &clinfo->platform_id, &clinfo->num_of_platforms);

	if (error_cl(err, "glGetPlatformIDs"))
		return err;

	//try to get a supported gpu device
	std::cout << "Trying to get a supported gpu device" << std::endl;
	err = clGetDeviceIDs(clinfo->platform_id, CL_DEVICE_TYPE_GPU, 
			     1, &clinfo->device_id,
			     &clinfo->num_of_devices);
	if (error_cl(err, "clGetDeviceIDs"))
		return err;

	cl_int prop_count = 0;
#ifdef _WIN32
	clinfo->properties[prop_count++] = CL_CONTEXT_PLATFORM;
	clinfo->properties[prop_count++] = 
		(cl_context_properties) clinfo->platform_id;
	clinfo->properties[prop_count++] = 
		CL_GL_CONTEXT_KHR;
	clinfo->properties[prop_count++] = 
		(cl_context_properties)glinfo.renderingContext;
	clinfo->properties[prop_count++] = CL_WGL_HDC_KHR;
	clinfo->properties[prop_count++] = 
		(cl_context_properties)glinfo.deviceContext;
	clinfo->properties[prop_count++] = 0;
#elif defined __linux__
	clinfo->properties[prop_count++] = CL_CONTEXT_PLATFORM;
	clinfo->properties[prop_count++] = 
		(cl_context_properties)clinfo->platform_id;
	clinfo->properties[prop_count++] = 
		CL_GL_CONTEXT_KHR;
	clinfo->properties[prop_count++] = 
		(cl_context_properties)glinfo.renderingContext;
	clinfo->properties[prop_count++] = CL_GLX_DISPLAY_KHR;
	clinfo->properties[prop_count++] = 
		(cl_context_properties)glinfo.renderingDisplay;
	clinfo->properties[prop_count++] = 0;
#else
#error "UNKNOWN PLATFORM"
#endif

	//clinfo->properties[0] = CL_CONTEXT_PLATFORM;
	//clinfo->properties[1] = (cl_context_properties) clinfo->platform_id;
	//clinfo->properties[2] = 0;

	//create a context with the GPU device
	std::cerr << "Creating a context with the GPU device" << std::endl;
	clinfo->context = 
		clCreateContext (clinfo->properties,1,&clinfo->device_id,NULL,NULL,&err);
	if (error_cl(err, "clCreateContext"))
		return err;

	//create command queue using the context and device
	std::cerr << "Creating command queue using the context and device" << std::endl;
	clinfo->command_queue = 
		clCreateCommandQueue(clinfo->context,clinfo->device_id,0,&err);
	if (error_cl(err, "clCreateCommandQueue"))
		return err;

	// Get size of global memory in the device
	err = clGetDeviceInfo (clinfo->device_id,
			       CL_DEVICE_GLOBAL_MEM_SIZE, 
			       sizeof(cl_ulong),
			       &clinfo->global_mem_size, 
			       &bytes_returned);
	if (error_cl(err, "clGetDeviceInfo CL_DEVICE_GLOBAL_MEM_SIZE"))
	    return err;

	// Get bool stating if cl device supports images
	err = clGetDeviceInfo (clinfo->device_id,
			       CL_DEVICE_IMAGE_SUPPORT, 
			       sizeof(cl_bool),
			       &clinfo->image_support, 
			       &bytes_returned);
	if (error_cl(err, "clGetDeviceInfo CL_DEVICE_IMAGE_SUPPORT"))
	    return err;
	
	// Get max image2d height
	err = clGetDeviceInfo (clinfo->device_id,
			       CL_DEVICE_IMAGE2D_MAX_HEIGHT, 
			       sizeof(size_t),
			       &clinfo->image2d_max_height, 
			       &bytes_returned);
	if (error_cl(err, "clGetDeviceInfo CL_DEVICE_IMAGE2D_MAX_HEIGHT"))
	    return err;
	
	// Get max image2d width
	err = clGetDeviceInfo (clinfo->device_id,
			       CL_DEVICE_IMAGE2D_MAX_WIDTH, 
			       sizeof(size_t),
			       &clinfo->image2d_max_width, 
			       &bytes_returned);
	if (error_cl(err, "clGetDeviceInfo CL_DEVICE_IMAGE2D_MAX_WIDTH"))
	    return err;

	// Get amount of compute units in the device
	err = clGetDeviceInfo (clinfo->device_id,
			       CL_DEVICE_MAX_COMPUTE_UNITS, 
			       sizeof(cl_uint),
			       &clinfo->max_compute_units, 
			       &bytes_returned);
	if (error_cl(err, "clGetDeviceInfo CL_DEVICE_MAX_COMPUTE_UNITS"))
	    return err;

	// Get maximum size of global mem objects for the device
	err = clGetDeviceInfo (clinfo->device_id,
			       CL_DEVICE_MAX_MEM_ALLOC_SIZE, 
			       sizeof(cl_ulong),
			       &clinfo->max_mem_alloc_size, 
			       &bytes_returned);
	if (error_cl(err, "clGetDeviceInfo CL_DEVICE_MAX_MEM_ALLOC_SIZE"))
	    return err;

	// Get maximum size of a work group
	err = clGetDeviceInfo (clinfo->device_id,
			       CL_DEVICE_MAX_WORK_GROUP_SIZE, 
			       sizeof(size_t),
			       &clinfo->max_work_group_size, 
			       &bytes_returned);
	if (error_cl(err, "clGetDeviceInfo CL_DEVICE_MAX_WORK_GROUP_SIZE"))
	    return err;

	// Get maximum number of items that can be specified in each dimension
	err = clGetDeviceInfo (clinfo->device_id,
			       CL_DEVICE_MAX_WORK_ITEM_SIZES, 
			       sizeof(clinfo->max_work_item_sizes),
			       clinfo->max_work_item_sizes, 
			       &bytes_returned);
	if (error_cl(err, "clGetDeviceInfo CL_DEVICE_MAX_WORK_ITEM_SIZES"))
	    return err;

	clinfo->initialized = true;
	return CL_SUCCESS;
}


int32_t
init_cl_kernel(CLInfo* clinfo, const char* kernel_file, 
	       std::string kernel_name,
	       CLKernelInfo* clkernelinfo)
{
	cl_int err;

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

	err = clBuildProgram(clkernelinfo->program,0,NULL,NULL,NULL,NULL);
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

int32_t execute_cl(const CLKernelInfo& clkernelinfo){

	cl_int err;
	
	CLInfo& clinfo = *(clkernelinfo.clinfo);

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
		err = clEnqueueNDRangeKernel(clinfo.command_queue,
					     clkernelinfo.kernel,
					     clkernelinfo.work_dim,
					     clkernelinfo.global_work_offset,
					     clkernelinfo.global_work_size,
					     clkernelinfo.local_work_size,
					     0,NULL,NULL);
	else
		err = clEnqueueNDRangeKernel(clinfo.command_queue,
					     clkernelinfo.kernel,
					     clkernelinfo.work_dim,
					     clkernelinfo.global_work_offset,
					     clkernelinfo.global_work_size,
					     NULL,
					     0,NULL,NULL);

	if (error_cl(err, "clEnqueueNDRangeKernel"))
		return 1;

	/* finish command queue */
	err = clFinish(clinfo.command_queue);
	if (error_cl(err, "clFinsish"))
		return 1;

	return 0;	
}


int32_t create_empty_cl_mem(const CLInfo& clinfo, cl_mem_flags flags, 
			uint32_t size, cl_mem* mem)
{
	cl_int err;
	*mem = clCreateBuffer(clinfo.context,
			     flags,
			     size,
			     NULL,
			     &err);
	if (error_cl(err, "clCreateBuffer"))
		return 1;
	return 0;
}

int32_t create_filled_cl_mem(const CLInfo& clinfo, cl_mem_flags flags, uint32_t size, 
			 const void* values, cl_mem* mem)
{
	cl_int err;
	*mem = clCreateBuffer(clinfo.context,
			     flags | CL_MEM_COPY_HOST_PTR,
			     size,
			     const_cast<void*>(values), //This is ugly but necessary
			      // Kronos, why u not have a const variant ?!?!
			     &err);
	if (error_cl(err, "clCreateBuffer"))
		return 1;
	return 0;

}

int32_t create_host_cl_mem(const CLInfo& clinfo, cl_mem_flags flags, uint32_t size, 
			   void* ptr, cl_mem* mem)
{
	cl_int err;
	*mem = clCreateBuffer(clinfo.context,
			      flags | CL_MEM_USE_HOST_PTR,
			      size,
			      ptr, //This is ugly but necessary
			      // Kronos, why u not have a const variant ?!?!
			      &err);
	if (error_cl(err, "clCreateBuffer"))
		return 1;
	return 0;

}

int32_t copy_to_cl_mem(const CLInfo& clinfo, uint32_t size,
		       const void* values, cl_mem& mem, uint32_t offset){
	cl_int err;
	err = clEnqueueWriteBuffer(clinfo.command_queue,
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


	err = clFinish(clinfo.command_queue);
	if (error_cl(err, "clFinsish"))
		return 1;

	return 0;
}

int32_t copy_from_cl_mem(const CLInfo& clinfo, uint32_t size,
			 void* values, cl_mem& mem, uint32_t offset){
	cl_int err;
	err = clEnqueueReadBuffer(clinfo.command_queue,
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

	err = clFinish(clinfo.command_queue);
	if (error_cl(err, "clFinsish"))
		return 1;

	return 0;
}

void print_cl_info(const CLInfo& clinfo)
{

	cl_int err;
	cl_uint max_samplers;
	char device_vendor[128], device_name[128], device_cl_version[128], c_version[128];
	size_t bytes_returned;
	err = clGetDeviceInfo (clinfo.device_id,
			       CL_DEVICE_VENDOR, sizeof(device_vendor),
			       device_vendor, &bytes_returned);
	if (error_cl(err, "clGetDeviceInfo CL_DEVICE_VENDOR"))
	    return;

	err = clGetDeviceInfo (clinfo.device_id,
			       CL_DEVICE_VERSION, sizeof(device_cl_version),
			       device_cl_version, &bytes_returned);

	if (error_cl(err, "clGetDeviceInfo CL_DEVICE_VERSION"))
	    return;

	err = clGetDeviceInfo (clinfo.device_id,
			       CL_DEVICE_NAME, sizeof(device_name),
			       device_name, &bytes_returned);

	if (error_cl(err, "clGetDeviceInfo CL_DEVICE_NAME"))
	    return;

	err = clGetDeviceInfo (clinfo.device_id,
			       CL_DEVICE_OPENCL_C_VERSION, sizeof(c_version),
			       c_version, &bytes_returned);

	if (error_cl(err, "clGetDeviceInfo CL_DEVICE_OPENCL_C_VERSION"))
	    return;

	err = clGetDeviceInfo (clinfo.device_id,
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
		  << clinfo.global_mem_size << std::endl;
	std::cout << "\tOpenCL device maximum mem alloc size: " 
		  << clinfo.max_mem_alloc_size << std::endl;
	std::cout << "\tOpenCL device image2d max size: " 
		  << clinfo.image2d_max_width << " x "
		  << clinfo.image2d_max_width << std::endl;
	std::cout << "\tOpenCL device compute units: " 
		  << clinfo.max_compute_units << std::endl;
	std::cout << "\tOpenCL device max work group size: " 
		  << clinfo.max_work_group_size << std::endl;
	std::cout << "\tOpenCL device max work item sizes per dimension: " 
		  << clinfo.max_work_item_sizes[0] << " "
		  << clinfo.max_work_item_sizes[1] << " "
		  << clinfo.max_work_item_sizes[2] << std::endl;
	

	std::cout << std::endl;
}

void print_cl_mem_info(const cl_mem& mem){

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

int create_cl_mem_from_gl_tex(const CLInfo& clinfo, const GLuint gl_tex, cl_mem* mem)
{
	cl_int err;

	*mem = clCreateFromGLTexture2D(clinfo.context,
				       CL_MEM_READ_WRITE,
				       GL_TEXTURE_2D,0,
				       gl_tex,&err);
	
	if (error_cl(err, "clCreateFromGLTexture2D"))
		return 1;

	err = clEnqueueAcquireGLObjects(clinfo.command_queue,
					1,
					mem,
					0,0,0);

	if (error_cl(err, "clEnqueueAcquireGLObjects"))
		return 1;

	err = clFinish(clinfo.command_queue);

	if (error_cl(err, "clFinish"))
		return 1;

	return 0;
}

inline int error_cl(cl_int err_num, std::string msg)
{
	if (err_num != CL_SUCCESS){
		std::cerr << " *** OpenCL error: " << msg 
			  << " (error code " << err_num << ")" <<std::endl;
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
