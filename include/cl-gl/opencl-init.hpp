#ifndef OPENCL_INIT_HPP
#define OPENCL_INIT_HPP

#include <cl-gl/opengl-init.hpp>

#ifdef _WIN32
  #include <Windows.h>
  #include <GL/GL.h>
  #include <CL/cl.h>
  #include <CL/cl_gl.h>

#elif defined __linux__
  #include <GL/gl.h>
  #include <CL/cl.h>
  #include <CL/cl_gl.h>

#else
  #error "UNKNOWN PLATFORM"
#endif

#include <stdint.h>
#include <string>


struct CLInfo
{
	cl_context context;
	cl_command_queue command_queue;
	cl_uint num_of_platforms;
	cl_platform_id platform_id;
	cl_device_id device_id;
	cl_uint num_of_devices;
	cl_context_properties properties[10];

	cl_ulong global_mem_size;
	cl_bool  image_support;
	size_t image2d_max_height,image2d_max_width;

	cl_uint  max_compute_units;
	cl_ulong max_mem_alloc_size;
	size_t   max_work_group_size;
	size_t   max_work_item_sizes[3];

	bool initialized;	

	CLInfo(){initialized = false;}
	void release_resources()
		{
			if (!initialized) return;
			clReleaseCommandQueue(command_queue);
			clReleaseContext(context);
		}

};

struct CLKernelInfo
{
	CLInfo* clinfo;
	cl_program program;
	cl_kernel kernel;
	size_t global_work_offset[3];
	size_t global_work_size[3];
	size_t local_work_size[3];
	int8_t work_dim;
	int8_t arg_count; // If you're using more than 127 args you're doing sth wrong...

	CLKernelInfo() 
		{
			for (int8_t i = 0; i < 3; ++i) {
				local_work_size[i] = 0;
				global_work_size[i] = 0;
				global_work_offset[i] = 0;
			}
			arg_count = -1;
			work_dim = -1;
		}
};

cl_int init_cl(const GLInfo& glinfo, CLInfo* clinfo);
int32_t init_cl_kernel(CLInfo* clinfo, const char* kernel_file, 
		   std::string kernel_name,
		   CLKernelInfo* clkernelinfo);
int32_t create_empty_cl_mem(const CLInfo& clinfo, cl_mem_flags flags,
			uint32_t size, cl_mem* mem);
int32_t create_filled_cl_mem(const CLInfo& clinfo, cl_mem_flags flags, 
			 uint32_t size, const void* values, cl_mem* mem);
int32_t create_host_cl_mem(const CLInfo& clinfo, cl_mem_flags flags, 
			   uint32_t size, void* ptr, cl_mem* mem);
int32_t copy_to_cl_mem(const CLInfo& clinfo, uint32_t size,
		       const void* values, cl_mem& mem, uint32_t offset = 0);
int32_t copy_from_cl_mem(const CLInfo& clinfo, uint32_t size,
			 void* values, cl_mem& mem, uint32_t offset = 0);
int32_t create_cl_mem_from_gl_tex(const CLInfo& clinfo, const GLuint gl_tex, cl_mem* mem);
int32_t execute_cl(const CLKernelInfo& clkernelinfo);
int32_t error_cl(cl_int err_num, std::string msg);
void print_cl_info(const CLInfo& clinfo);
void print_cl_image_2d_info(const cl_mem& mem);
void print_cl_mem_info(const cl_mem& mem);
size_t cl_mem_size(const cl_mem& mem);

int32_t acquire_gl_tex(cl_mem& tex_mem, const CLInfo& clinfo);
int32_t release_gl_tex(cl_mem& tex_mem, const CLInfo& clinfo);

#endif //OPENCL_INIT_HPP
