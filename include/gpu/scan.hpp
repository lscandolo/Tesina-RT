#pragma once
#ifndef RT_SCAN_HPP
#define RT_SCAN_HPP

#include <stdint.h>
#include <gpu/interface.hpp>
#include <gpu/function-library.hpp>

int32_t gpu_scan_uint(DeviceInterface& device, 
                      memory_id in_mem_id, size_t size,
                      memory_id out_mem_id, size_t command_queue_i = 0);


#endif /* RT_SCAN_HPP */
