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
        static int32_t initialize(CLInfo clinfo){
                DeviceFunctionLibrary::s_instance = NULL;
                if (DeviceFunctionLibrary::instance()->load_functions(clinfo))
                        return -1;
                DeviceFunctionLibrary::instance()->m_initialized = true;
                return 0;
        }

        DeviceFunction& function(gpu::LibraryFunction f) {return device.function(ids[f]);}
        
        bool valid(){return m_initialized;}

        DeviceFunctionLibrary(){}
private:
        int32_t load_functions(CLInfo clinfo);

        static DeviceFunctionLibrary* s_instance;
        DeviceInterface device;
        std::map<gpu::LibraryFunction, function_id> ids;
        bool m_initialized;
};

#endif /* RT_FUNCTION_LIBRARY_HPP */
