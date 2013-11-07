#pragma once
#ifndef RT_FUNCTION_LIBRARY_HPP
#define RT_FUNCTION_LIBRARY_HPP

#include <stdint.h>
#include <map>
#include <gpu/interface.hpp>

namespace gpu{

        enum LibraryFunction {
                scan_local_uint,
                scan_post_uint
        };
        
};

//// Singleton
class DeviceFunctionLibrary {
public:
        static DeviceFunctionLibrary* instance() {
                if (s_instance == NULL) {
                        s_instance = new DeviceFunctionLibrary; 
                }
                return s_instance;
        }
        int32_t initialize();

        DeviceFunction& function(gpu::LibraryFunction f) {
                DeviceInterface* device = DeviceInterface::instance();
                return device->function(ids[f]);
        }
        
        bool valid(){return m_initialized;}

        DeviceFunctionLibrary(){}
private:
        int32_t load_functions();

        static DeviceFunctionLibrary* s_instance;
        std::map<gpu::LibraryFunction, function_id> ids;
        bool m_initialized;
};

#endif /* RT_FUNCTION_LIBRARY_HPP */
