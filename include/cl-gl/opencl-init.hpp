#ifndef OPENCL_INIT_HPP
#define OPENCL_INIT_HPP

#include "cl-gl/opengl-init.hpp"

#ifdef _WIN32
  #include <Windows.h>
  #include <GL/GL.h>
  #include <CL/cl.hpp>
  #include <CL/cl_gl.h>

#elif defined __linux__
  #include <GL/gl.h>
  #include <CL/cl.hpp>
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

	bool initialized;	

	CLInfo(){initialized = false;}
	~CLInfo(){
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
	size_t global_work_size[3];
	size_t local_work_size[3];
	int work_dim;
	int arg_count;

	CLKernelInfo() 
		{
			for (int i = 0; i < 3; ++i) {
				local_work_size[i] = 0;
				global_work_size[i] = 0;
			}
			arg_count = -1;
			work_dim = -1;
		}
};

cl_int init_cl(const GLInfo& glinfo, CLInfo* clinfo);
int init_cl_kernel(CLInfo* clinfo, const char* kernel_file, 
		   std::string kernel_name,
		   CLKernelInfo* clkernelinfo);
int create_empty_cl_mem(const CLInfo& clinfo, cl_mem_flags flags, int size, cl_mem* mem);
int create_filled_cl_mem(const CLInfo& clinfo, cl_mem_flags flags, 
			 int size, void* values, cl_mem* mem);
int create_cl_mem_from_gl_tex(const CLInfo& clinfo, const GLuint gl_tex, cl_mem* mem);
int execute_cl(const CLKernelInfo& clkernelinfo);
int error_cl(cl_int err_num, std::string msg);
void print_cl_info(const CLInfo& clinfo);
void print_cl_image_2d_info(const cl_mem& mem);
void print_cl_mem_info(const cl_mem& mem);

#endif //OPENCL_INIT_HPP
