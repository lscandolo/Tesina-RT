#include <gpu/scan.hpp>
#include <math.h>

#define NUM_BANKS 32
#define LOG_NUM_BANKS 5
#define CONFLICT_FREE_OFFSET(n) (((n) >> LOG_NUM_BANKS) + ((n) >> (2 * LOG_NUM_BANKS)))

static size_t pad(size_t size, size_t pad_multiplier) {
        size_t rem = size%pad_multiplier;
        if (!rem)
                return size;
        return size + pad_multiplier - rem;
}

///////////// Size: size of in_mem to do the scan
///////////// Needs out_mem to be of at least (size+1)
int32_t gpu_scan_uint(DeviceInterface& device, 
                      memory_id in_mem_id, size_t size,
                      memory_id out_mem_id, size_t cq_i)
{
        
        DeviceMemory& in_mem = device.memory(in_mem_id);
        DeviceMemory& out_mem = device.memory(out_mem_id);

        if (!device.good() || !in_mem.valid() || !out_mem.valid() ||
            in_mem.size() < size*sizeof(cl_uint) ||
            out_mem.size() < (size+1)*sizeof(cl_uint)) {
                return -1;
        }
        
        DeviceFunctionLibrary* gpulib = DeviceFunctionLibrary::instance();
        if (!gpulib->valid())
                return -1;

        DeviceFunction& scan_local = gpulib->function(gpu::scan_local_uint);
        DeviceFunction& scan_post =  gpulib->function(gpu::scan_post_uint);
        size_t group_size = std::min(scan_local.max_group_size(),
                                     scan_post.max_group_size());
        size_t block_size = 2 * group_size;

        size_t out_size = size+1;
        size_t padded_size = pad(out_size, block_size);

        std::vector<memory_id> sum_mems;
        
        size_t remaining_size = padded_size;

        do {
                memory_id totals_mem_id = device.new_memory();
                DeviceMemory& totals_mem = device.memory(totals_mem_id);
                size_t totals_count = remaining_size/block_size + 1;
                if (totals_mem.initialize(totals_count*sizeof(cl_uint))) {
                        std::cerr << "Scan error initializing sums memory" << std::endl;
                        return -1;
                }
                sum_mems.push_back(totals_mem_id);
                remaining_size = remaining_size/block_size;
        } while (remaining_size);

        int32_t sum_idx = 0;

        memory_id in_id  = in_mem_id;
        memory_id sum_id = sum_mems[sum_idx];
        memory_id out_id = out_mem_id;

        remaining_size = padded_size;

        while (remaining_size) {
                DeviceMemory& in_mem =  device.memory(in_id);
                DeviceMemory& sum_mem = device.memory(sum_id);
                DeviceMemory& out_mem = device.memory(out_id);

                uint32_t in_mem_size;
                if (sum_idx == 0)
                        in_mem_size = size;
                else
                        in_mem_size = remaining_size+1;


                size_t local_mem_size = (block_size + CONFLICT_FREE_OFFSET(block_size));
                local_mem_size *= sizeof(cl_uint);
                if (scan_local.set_arg(0, in_mem) || 
                    scan_local.set_arg(1, sum_mem) || 
                    scan_local.set_arg(2, local_mem_size, NULL) ||
                    scan_local.set_arg(3, sizeof(cl_uint), &in_mem_size) ||
                    scan_local.set_arg(4, out_mem) ||
                    (uint32_t)sum_idx >= sum_mems.size()) {
                        std::cerr << "Scan error seting args for scan_local" << std::endl;
                        return -1;
                }

                padded_size = pad(remaining_size,block_size);

                size_t scan_local_size = padded_size/2; /*2 elements per thread*/

                if (scan_local.enqueue_single_dim(scan_local_size, group_size, 0, cq_i)) {
                        std::cerr << "Scan error enqeueing scan_local" << std::endl;
                        return -1;
                }
                device.enqueue_barrier(cq_i);
                
                remaining_size /= block_size;
                sum_idx ++;
                in_id = sum_id;
                out_id = sum_id;
                sum_id = sum_mems[sum_idx];
        }

        if (out_size <= block_size) {
                for (size_t i = 0; i < sum_mems.size(); ++i)
                        if (device.delete_memory(sum_mems[i]))
                                return -1;
                return 0;
        }

        sum_idx = sum_mems.size()-2;
        out_id =  sum_idx > 0 ? sum_mems[sum_idx-1] : out_mem_id;
        sum_id =  sum_mems[sum_idx];

        do {
                DeviceMemory& out_mem =  device.memory(out_id);
                DeviceMemory& sum_mem = device.memory(sum_id);

                cl_uint bsize = block_size;
                if (scan_post.set_arg(0, out_mem) || 
                    scan_post.set_arg(1, sum_mem) ||
                    scan_post.set_arg(2, sizeof(cl_uint), &bsize)) {
                        std::cerr << "Scan error seting args for scan_post" << std::endl;
                        return -1;
                }
                

                uint32_t scan_post_size = 
                        sum_idx > 0 ? out_mem.size() / sizeof(cl_uint) :
                                      out_size;

                if (scan_post.enqueue_single_dim(scan_post_size, group_size, 0, cq_i)) {
                        std::cerr << "Scan error enqeueing scan_post" << std::endl;
                        return -1;
                }
                device.enqueue_barrier(cq_i);

                sum_idx--;
                if (sum_idx < 0)
                        break;

                out_id =  sum_idx > 0 ? sum_mems[sum_idx-1] : out_mem_id;
                sum_id =  sum_mems[sum_idx];

        } while (1);

        for (size_t i = 0; i < sum_mems.size(); ++i)
                if (device.delete_memory(sum_mems[i]))
                        return -1;

        return 0;
}
