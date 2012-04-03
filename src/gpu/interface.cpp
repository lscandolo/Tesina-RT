#include <gpu/interface.hpp>

DeviceInterface::DeviceInterface() : 
        invalid_memory(invalid_clinfo), invalid_function(invalid_clinfo)
{
        m_initialized = false;
}

bool DeviceInterface::good()
{
        return m_initialized && m_clinfo.initialized;
}

int32_t 
DeviceInterface::initialize(CLInfo clinfo)
{
        m_clinfo = clinfo;
        m_initialized = true;
        return 0;
}

memory_id 
DeviceInterface::new_memory()
{
        if (!good())
                return -1;
        memory_objects.push_back(DeviceMemory(m_clinfo));
        return memory_objects.size() - 1;
}

function_id 
DeviceInterface::new_function()
{
        if (!good())
                return -1;
        function_objects.push_back(DeviceFunction(m_clinfo));
        return function_objects.size() - 1;
}

DeviceMemory&
DeviceInterface::memory(memory_id id)
{
        if (!good() || !valid_memory_id(id))
                return invalid_memory;

        return memory_objects[id];
}

DeviceFunction& 
DeviceInterface::function(function_id id)
{
        if (!good() || !valid_function_id(id))
                return invalid_function;

        return function_objects[id];
}

bool 
DeviceInterface::valid_memory_id(memory_id id)
{
        return id >= 0 && id < memory_objects.size();
}

bool 
DeviceInterface::valid_function_id(function_id id)
{
        return id >= 0 && id < function_objects.size();
}

int32_t 
DeviceInterface::delete_memory(memory_id id)
{
        if (!valid_memory_id(id))
                return -1;
        return memory_objects[id].release();
}

int32_t 
DeviceInterface::delete_function(function_id id)
{
        if (!good())
                return -1;

        if (!valid_function_id(id))
                return -1;
        return function_objects[id].release();
}
