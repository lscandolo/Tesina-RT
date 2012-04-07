#ifndef GPU_MEMORY_HPP
#define GPU_MEMORY_HPP

#include <stdint.h>
#include <string>

#include <cl-gl/opengl-init.hpp>
#include <cl-gl/opencl-init.hpp>

enum DeviceMemoryMode
{
        READ_ONLY_MEMORY,
        WRITE_ONLY_MEMORY,
        READ_WRITE_MEMORY
};

class DeviceMemory {

public:
        DeviceMemory(CLInfo clinfo);
        bool valid() const;
        int32_t initialize(size_t size, DeviceMemoryMode mode = READ_WRITE_MEMORY);
        int32_t initialize(size_t size, const void* values, 
                           DeviceMemoryMode mode = READ_WRITE_MEMORY);
        int32_t initialize_from_gl_texture(const GLuint gl_tex);
        size_t write(size_t nbytes, const void* values, size_t offset = 0);
        size_t read(size_t nbytes, void* buffer, size_t offset = 0);
        int32_t release();
        size_t size() const;
        cl_mem* ptr();
private:
        bool m_initialized;
        CLInfo m_clinfo;
        cl_mem m_mem;
        size_t m_size;
};

#endif /* GPU_MEMORY_HPP */