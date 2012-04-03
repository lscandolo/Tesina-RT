#ifndef GPU_INTERFACE_HPP
#define GPU_INTERFACE_HPP

#include <stdint.h>
#include <string>
#include <vector>

#include <cl-gl/opencl-init.hpp>

#include <gpu/memory.hpp>
#include <gpu/function.hpp>

typedef size_t memory_id;
typedef size_t function_id;

class DeviceInterface
{
public:
        DeviceInterface();

        bool good();
        int32_t initialize(CLInfo clinfo);

        memory_id new_memory();
        function_id new_function();
        DeviceMemory& memory(memory_id id);
        DeviceFunction& function(function_id id);

        int32_t delete_memory(memory_id id);
        int32_t delete_function(function_id id);

        bool valid_function_id(function_id id);
        bool valid_memory_id(memory_id id);

private:
        CLInfo m_clinfo;
        bool m_initialized;
        std::vector<DeviceMemory> memory_objects;
        std::vector<DeviceFunction> function_objects;

        CLInfo invalid_clinfo;
        DeviceMemory   invalid_memory;
        DeviceFunction invalid_function;
};

/*Very sucky implementation here. 
  TODO: make it so released memory objects are not stored 
    and we deal with invalid ids better */

#endif /* GPU_INTERFACE_HPP */
