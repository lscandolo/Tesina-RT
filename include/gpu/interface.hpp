#ifndef GPU_INTERFACE_HPP
#define GPU_INTERFACE_HPP

#include <stdint.h>
#include <string>
#include <vector>
#include <bitset>

#include <cl-gl/opencl-init.hpp>

#include <gpu/memory.hpp>
#include <gpu/function.hpp>

typedef size_t memory_id;
typedef size_t function_id;

#define PREALLOC_SIZE (1<<10)

//// Singleton
class DeviceInterface
{
public:
        static DeviceInterface* instance() {
                if (s_instance == NULL) {
                        s_instance = new DeviceInterface;
                }
                return s_instance;
        }

        bool good();
        int32_t initialize(CLInfo clinfo);

        memory_id new_memory();
        function_id new_function();
        DeviceMemory& memory(memory_id id);
        DeviceFunction& function(function_id id);

        std::vector<function_id> build_functions(std::string file,
                                                 std::vector<std::string>& names);

        int32_t delete_memory(memory_id id);
        int32_t delete_function(function_id id);

        bool valid_function_id(function_id id);
        bool valid_memory_id(memory_id id);

        int32_t acquire_graphic_resource(memory_id tex_id);
        int32_t release_graphic_resource(memory_id tex_id);

        int32_t enqueue_barrier();
        int32_t finish_commands();

        int32_t copy_memory(memory_id from, memory_id to, 
                            size_t bytes = 0, 
                            size_t from_offset = 0,
                            size_t to_offset = 0);

        size_t  max_group_size(uint32_t dim);

        DeviceInterface();

private:

        static DeviceInterface* s_instance;
        CLInfo m_clinfo;
        bool m_initialized;
        // std::vector<DeviceMemory> memory_objects;
        // std::vector<DeviceFunction> function_objects;

        CLInfo invalid_clinfo;
        DeviceMemory   invalid_memory;
        DeviceFunction invalid_function;


        std::vector<std::bitset<PREALLOC_SIZE> > memory_map;
        std::vector<std::bitset<PREALLOC_SIZE> > function_map;
        std::vector<std::vector<DeviceMemory> > memory_objects;
        std::vector<std::vector<DeviceFunction> > function_objects;
};

/*Very sucky implementation here. 
  TODO: make it so released memory objects are not stored 
    and we deal with invalid ids better */

#endif /* GPU_INTERFACE_HPP */
