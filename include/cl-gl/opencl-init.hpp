#ifndef OPENCL_INIT_HPP
#define OPENCL_INIT_HPP

#define CL_USE_DEPRECATED_OPENCL_1_1_APIS //For new amd app drivers

#include <cl-gl/opengl-init.hpp>

#ifdef _WIN32
  #include <Windows.h>
  #include <GL/GL.h>
  #include <CL/cl.h>
  #include <CL/cl_gl.h>

  #ifndef __func__
  #define __func__ __FUNCTION__
  #endif

#elif defined __linux__
  #include <GL/gl.h>
  #include <CL/cl.h>
  #include <CL/cl_gl.h>

#else
  #error "UNKNOWN PLATFORM"
#endif

#include <stdint.h>
#include <string>
#include <iostream>

#include <vector>

class CLInfo
{

private:
        bool     m_initialized;
        bool     m_sync;
        static CLInfo*  pinstance;
        std::vector<cl_command_queue> command_queues;

public:
        static CLInfo* instance();

	cl_context context;
	cl_uint num_of_platforms;
	cl_platform_id platform_id;
	cl_device_id device_id;
	cl_uint num_of_devices;
	cl_context_properties properties[10];


	cl_ulong global_mem_size;
	cl_ulong local_mem_size;
	cl_bool  image_support;
	size_t image2d_max_height,image2d_max_width;

	cl_uint  max_compute_units;
	cl_ulong max_mem_alloc_size;
	size_t   max_work_group_size;
	size_t   max_work_item_sizes[3];

	CLInfo();
	bool initialized();	
        cl_int  initialize(size_t command_queues = 1);
        void set_sync(bool s);
        bool sync();
	void release_resources();
        cl_command_queue get_command_queue(size_t i);
        bool             has_command_queue(size_t i);
        void             print_info();
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

int32_t init_cl_kernel(const char* kernel_file, std::string kernel_name, 
                       CLKernelInfo* clkernelinfo);
int32_t create_empty_cl_mem(cl_mem_flags flags,	uint32_t size, cl_mem* mem);
int32_t create_filled_cl_mem(cl_mem_flags flags, uint32_t size, 
                             const void* values, cl_mem* mem);
int32_t create_host_cl_mem(cl_mem_flags flags, uint32_t size, void* ptr, cl_mem* mem);
int32_t copy_to_cl_mem(uint32_t size, const void* values, cl_mem& mem, 
                       uint32_t offset = 0, size_t cq_i = 0);
int32_t copy_from_cl_mem(uint32_t size, void* values, cl_mem& mem, 
                         uint32_t offset = 0, size_t cq_i = 0);
int32_t create_cl_mem_from_gl_tex(const GLuint gl_tex, cl_mem* mem, 
                                  size_t cq_i = 0);
int32_t execute_cl(const CLKernelInfo& clkernelinfo, size_t cq_i = 0);
void print_cl_image_2d_info(const cl_mem& mem);
void print_cl_mem_info(const cl_mem& mem);
size_t cl_mem_size(const cl_mem& mem);

int32_t acquire_gl_tex(cl_mem& tex_mem, size_t cq_i = 0);
int32_t release_gl_tex(cl_mem& tex_mem, size_t cq_i = 0);


int32_t __error_cl(cl_int err_num, std::string msg,
                   const char* file, const char* func,  int line);

#define error_cl(x,y) __error_cl(x,y,__FILE__,__func__,__LINE__)

#endif //OPENCL_INIT_HPP
