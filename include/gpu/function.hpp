#ifndef GPU_FUNCTION_HPP
#define GPU_FUNCTION_HPP

#include <stdint.h>
#include <string>

// #ifdef _WIN32
//   #include <GL/GL.h>
//   #include <CL/cl.h>
//   #include <CL/cl_gl.h>

// #elif defined __linux__
//   #include <GL/gl.h>
//   #include <CL/cl.h>
//   #include <CL/cl_gl.h>

// #else
//   #error "UNKNOWN PLATFORM"
// #endif

#include <cl-gl/opencl-init.hpp>
#include <gpu/memory.hpp>

class DeviceInterface;

class DeviceFunction {

        friend class DeviceInterface;

public:
        static bool sync;

        DeviceFunction(CLInfo clinfo);
        bool valid();
        int32_t initialize(std::string file, std::string name);
        int32_t set_arg(int32_t arg_num, size_t arg_size, void* arg);
        int32_t set_arg(int32_t arg_num, DeviceMemory& mem);
        int32_t set_dims(uint8_t dims);
        int32_t set_global_size(size_t size[3]);
        int32_t set_global_offset(size_t offset[3]);
        int32_t set_local_size(size_t size[3]);
        int32_t execute();
        int32_t execute_single_dim(size_t global_size, size_t local_size = 0, 
                                   size_t global_offset = 0);
        int32_t enqueue();
        int32_t enqueue_single_dim(size_t global_size, size_t local_size = 0,
                                   size_t global_offset = 0);
        int32_t release();
        size_t max_group_size();

private:

        int32_t initialize(std::string name);

        bool m_initialized;
        CLInfo m_clinfo;
        cl_program m_program;
        cl_kernel m_kernel;
        int32_t m_arg_count;
        uint8_t m_work_dim;
	size_t m_global_size[3];
	size_t m_global_offset[3];
	size_t m_local_size[3];
        size_t m_max_group_size;
};

#endif /* GPU_FUNCTION_HPP */
