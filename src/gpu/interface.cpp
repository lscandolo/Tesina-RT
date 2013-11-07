#include <gpu/interface.hpp>

DeviceInterface* DeviceInterface::s_instance = NULL;

DeviceInterface::DeviceInterface() 
{
        memory_map.resize(1);
        memory_map[0].set();

        memory_objects.resize(1);
        memory_objects[0].resize(PREALLOC_SIZE);

        function_map.resize(1);
        function_map[0].set();

        function_objects.resize(1);
        function_objects[0].resize(PREALLOC_SIZE);

        m_initialized = false;
}

bool DeviceInterface::good()
{
        return m_initialized;
}

int32_t 
DeviceInterface::initialize()
{
        CLInfo* clinfo = CLInfo::instance();
        if (!clinfo->initialized() || m_initialized)
                return -1;

        m_initialized = true;
        memory_objects.reserve(PREALLOC_SIZE);
        function_objects.reserve(PREALLOC_SIZE);
        return 0;
}

memory_id 
DeviceInterface::new_memory()
{
        if (!good())
                return -1;
        for (size_t i = 0; i < memory_map.size(); ++i) {
                if (memory_map[i].none()) 
                        continue;

                for (size_t j = 0; j < memory_map[i].size(); j++)
                        if (memory_map[i][j]) {
                                memory_map[i].flip(j);
                                memory_objects[i][j] = DeviceMemory();
                                return (i*PREALLOC_SIZE + j);
                        }
        }

        size_t i = memory_map.size();
        memory_map.push_back(std::bitset<PREALLOC_SIZE>());
        memory_objects.push_back(
                std::vector<DeviceMemory>(PREALLOC_SIZE,DeviceMemory()));
        
        memory_map[i].set();
        memory_map[i][0].flip();

        memory_objects[i][0] = DeviceMemory();
        return i * PREALLOC_SIZE;
}

function_id 
DeviceInterface::new_function()
{
        if (!good())
                return -1;
        for (size_t i = 0; i < function_map.size(); ++i) {
                if (function_map[i].none()) 
                        continue;

                for (size_t j = 0; j < function_map[i].size(); j++)
                        if (function_map[i][j]) {
                                function_map[i].flip(j);
                                function_objects[i][j] = DeviceFunction();
                                return (i*PREALLOC_SIZE + j);
                        }
        }

        size_t i = function_map.size();
        function_map.push_back(std::bitset<PREALLOC_SIZE>());
        function_objects.push_back(
                std::vector<DeviceFunction>(PREALLOC_SIZE,DeviceFunction()));
        
        function_map[i].set();
        function_map[i][0].flip();

        function_objects[i][0] = DeviceFunction();
        return i * PREALLOC_SIZE;



        // if (!good())
        //         return -1;
        // function_objects.push_back(DeviceFunction(m_clinfo));
        // return function_objects.size() - 1;
}

DeviceMemory&
DeviceInterface::memory(memory_id id)
{
        if (!good() || !valid_memory_id(id))
                return invalid_memory;

        return memory_objects[id/PREALLOC_SIZE][id%PREALLOC_SIZE];

}

DeviceFunction& 
DeviceInterface::function(function_id id)
{
        if (!good() || !valid_function_id(id))
                return invalid_function;

        return function_objects[id/PREALLOC_SIZE][id%PREALLOC_SIZE];
}

std::vector<function_id>
DeviceInterface::build_functions(std::string file, std::vector<std::string>& names)
{
        std::vector<function_id> ids;

        if (names.size() == 0)
                return ids;

        ids.push_back(new_function());
        DeviceFunction& f = function(ids[0]);

        if (f.initialize(file, names[0]))  {
                for (int i = 0; i < ids.size(); ++i) {
                        delete_function(ids[i]);
                        ids.clear();
                }
                return ids;
        }

        cl_program file_program = f.m_program;

        for (int i = 1; i < names.size(); ++i) {
                
                function_id fid = new_function();
                ids.push_back(fid);
                DeviceFunction& f = function(fid);
                f.m_program = file_program;
                if (f.initialize(names[i])) {
                        for (int i = 0; i < ids.size(); ++i) {
                                delete_function(ids[i]);
                                ids.clear();
                        }
                        return ids;
                }
        }

        return ids;

}


bool 
DeviceInterface::valid_memory_id(memory_id id)
{
        return  id < memory_map.size()*PREALLOC_SIZE && 
                !memory_map[id/PREALLOC_SIZE][id%PREALLOC_SIZE];
        
}

bool 
DeviceInterface::valid_function_id(function_id id)
{
        return  id < function_map.size()*PREALLOC_SIZE && 
                !function_map[id/PREALLOC_SIZE][id%PREALLOC_SIZE];

}

int32_t 
DeviceInterface::delete_memory(memory_id id)
{
        if (!valid_memory_id(id))
                return -1;

        if (memory(id).valid())
                if (memory(id).release())
                        return -1;

        memory_map[id/PREALLOC_SIZE].flip(id%PREALLOC_SIZE);
        return 0;
}

int32_t 
DeviceInterface::delete_function(function_id id)
{
        if (!good())
                return -1;

        if (!valid_function_id(id))
                return -1;
        if (function(id).valid())
                if (function(id).release())
                        return -1;

        function_map[id/PREALLOC_SIZE].flip(id%PREALLOC_SIZE);
        return 0;
}

int32_t
DeviceInterface::acquire_graphic_resource(memory_id tex_id, bool enqueue_barrier, 
                                          size_t command_queue_i)
{
        if (!good())
                return -1;

        CLInfo* clinfo = CLInfo::instance();
        if (!clinfo->initialized() || !clinfo->has_command_queue(command_queue_i))
                return -1;

        cl_command_queue command_queue = clinfo->get_command_queue(command_queue_i);


        cl_int err;
        err = clEnqueueAcquireGLObjects(command_queue,
                1,
                memory(tex_id).ptr(),
                0,
                NULL,
                NULL);
        if (error_cl(err, "clEnqueueAcquireGLObjects"))
                return -1;

        if (enqueue_barrier) {
                err = clEnqueueBarrier(command_queue);
                if (error_cl(err, "clEnqueueBarrier"))
                return -1;
        }
        return 0;
}
int32_t
DeviceInterface::release_graphic_resource(memory_id tex_id, bool enqueue_barrier,
                                          size_t command_queue_i)
{
        if (!good())
                return -1;

        CLInfo* clinfo = CLInfo::instance();
        if (!clinfo->initialized() || !clinfo->has_command_queue(command_queue_i))
                return -1;

        cl_command_queue command_queue = clinfo->get_command_queue(command_queue_i);

        cl_int err;
        err = clEnqueueReleaseGLObjects(command_queue,
                1,
                memory(tex_id).ptr(),
                0,
                NULL,
                NULL);
        if (error_cl(err, "clEnqueueReleaseGLObjects"))
                return -1;

        if (enqueue_barrier) {
                err = clEnqueueBarrier(command_queue);
                if (error_cl(err, "clEnqueueBarrier"))
                        return -1;
        }
        return 0;
}

int32_t 
DeviceInterface::enqueue_barrier(size_t command_queue_i){
        if (!good())
                return -1;

        CLInfo* clinfo = CLInfo::instance();
        if (!clinfo->initialized() || !clinfo->has_command_queue(command_queue_i))
                return -1;

        cl_command_queue command_queue = clinfo->get_command_queue(command_queue_i);

        cl_int err;
        if (clinfo->sync())
                err = clFinish(command_queue);
        else
                err = clEnqueueBarrier(command_queue);
	if (error_cl(err, "clEnqueueBarrier"))
		return -1;

        return 0;
}

int32_t 
DeviceInterface::finish_commands(size_t command_queue_i){
        if (!good())
                return -1;

        CLInfo* clinfo = CLInfo::instance();
        if (!clinfo->initialized() || !clinfo->has_command_queue(command_queue_i))
                return -1;

        cl_command_queue command_queue = clinfo->get_command_queue(command_queue_i);
        cl_int err;
	err = clFinish(command_queue);
	if (error_cl(err, "clFinish"))
		return -1;

        return 0;
}

int32_t 
DeviceInterface::copy_memory(memory_id from, memory_id to, 
                             size_t bytes, size_t from_offset, 
                             size_t to_offset, size_t command_queue_i) 
{
        if (!good())
                return -1;

        CLInfo* clinfo = CLInfo::instance();
        if (!clinfo->initialized() || !clinfo->has_command_queue(command_queue_i))
                return -1;

        cl_command_queue command_queue = clinfo->get_command_queue(command_queue_i);

        if (from >= memory_objects.size() || to >= memory_objects.size()) 
                return -1;

        DeviceMemory& from_mem = memory(from);
        DeviceMemory& to_mem = memory(to);
        
        if (!from_mem.valid() || !to_mem.valid())
                return -1;

        if (bytes == 0)
                bytes = from_mem.size();

        if (bytes + to_offset   > to_mem.size() ||
            bytes + from_offset > from_mem.size())
                return -1;

        cl_int err;
        err = clEnqueueCopyBuffer(command_queue,
                                  *from_mem.ptr(),
                                  *to_mem.ptr(),
                                  from_offset,
                                  to_offset,
                                  bytes,
                                  NULL,NULL,NULL);

        if (error_cl(err, "clEnqueueCopyBuffer"))
                return -1;

        err = clFinish(command_queue);
        if (error_cl(err, "clFinish"))
                return -1;

        return 0;
}

size_t  
DeviceInterface::max_group_size(uint32_t dim)
{
        CLInfo* clinfo = CLInfo::instance();
        if (!clinfo->initialized())
                return 0;
        if (dim < 3)
                return clinfo->max_work_item_sizes[dim];
        return 0;
}
