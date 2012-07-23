#include <gpu/function-library.hpp>

DeviceFunctionLibrary* DeviceFunctionLibrary::s_instance = NULL;

int32_t 
DeviceFunctionLibrary::load_functions(CLInfo clinfo) 
{
        m_initialized = false;
        if (device.initialize(clinfo))
                return -1;

        function_id scan_local_uint_id = device.new_function();
        DeviceFunction& scan_local_uint = device.function(scan_local_uint_id);
        if (scan_local_uint.initialize("src/kernel/scan.cl",
                                       "scan_local_uint"))
                return -1;
        ids[gpu::scan_local_uint] = scan_local_uint_id;


        function_id scan_post_uint_id  = device.new_function();
        DeviceFunction& scan_post_uint = device.function(scan_post_uint_id);
        if (scan_post_uint.initialize("src/kernel/scan.cl",
                                      "scan_post_uint"))
                return -1;
        ids[gpu::scan_post_uint] = scan_post_uint_id;

        
        m_initialized = true;
        std::cerr << "Initialized gpu function library" << std::endl;
        return 0;
}
