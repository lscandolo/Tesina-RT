#include <algorithm>
#include <iostream>
#include <sstream>

#include <cl-gl/opengl-init.hpp>
#include <cl-gl/opencl-init.hpp>
#include <rt/rt.hpp>

#define DO_BITONIC 1
#define DO_RADIX   1

function_id bitonic_local_id;

function_id bitonic2_id;
function_id bitonic4_id;
function_id bitonic8_id;
function_id bitonic16_id;
function_id bitonic32_id;

function_id bitonicl2_id;
function_id bitonicl4_id;

function_id radix_id;
function_id radix_hist_id;
function_id radix_sums_id;
function_id radix_rearrange_id;

CLInfo clinfo;
GLInfo glinfo;

DeviceInterface& device = *DeviceInterface::instance();
memory_id tex_id;
rt_time_t rt_time;

size_t window_size[] = {1,1};

void Bitonic(int);
void Radix(unsigned int);

int main (int argc, char** argv)
{

        /*---------------------- Initialize OpenGL and OpenCL ----------------------*/

        if (init_gl(argc,argv,&glinfo, window_size, "RT") != 0){
                std::cerr << "Failed to initialize GL" << std::endl;
                pause_and_exit(1);
        } else { 
                std::cout << "Initialized GL succesfully" << std::endl;
        }

        if (init_cl(glinfo,&clinfo) != CL_SUCCESS){
                std::cerr << "Failed to initialize CL" << std::endl;
                pause_and_exit(1);
        } else { 
                std::cout << "Initialized CL succesfully" << std::endl;
        }
        print_cl_info(clinfo);

        /* Initialize device interface */
        if (device.initialize(clinfo)) {
                std::cerr << "Failed to initialize device interface" << std::endl;
                pause_and_exit(1);
        }
        
#if DO_BITONIC

        std::vector<std::string> bitonic_names;
        bitonic_names.push_back("Bitonic_Local");
        bitonic_names.push_back("Bitonic_G2");
        bitonic_names.push_back("Bitonic_G4");
        bitonic_names.push_back("Bitonic_G8");
        bitonic_names.push_back("Bitonic_G16");
        bitonic_names.push_back("Bitonic_G32");
        bitonic_names.push_back("Bitonic_L2");
        bitonic_names.push_back("Bitonic_L4");
        rt_time.snap_time();


        std::vector<function_id> fids;
        fids = device.build_functions("src/kernel/sort.cl",bitonic_names);
        if (!fids.size())
                return -1;

        bitonic_local_id = fids[0];
        bitonic2_id = fids[1];
        bitonic4_id = fids[2];
        bitonic8_id = fids[3];
        bitonic16_id = fids[4];
        bitonic32_id = fids[5];
        bitonicl2_id = fids[6];
        bitonicl4_id = fids[7];
        std::cout << "Single Kernel load time: " << rt_time.msec_since_snap() 
                  << " ms" << std::endl;


        int i = 11;
        for (i = 0; i < 21; ++i) {
                Bitonic(pow(2,i));
                // Bitonic(bitonic_id, i+2);
        }
#endif

#if  DO_RADIX

        std::vector<std::string> radix_names;
        radix_names.push_back("Radix_Local");
        radix_names.push_back("Hist_Prefix_Sum");
        radix_names.push_back("Radix_Prefix_Sum");
        radix_names.push_back("Radix_Rearrange");
        rt_time.snap_time();

        std::vector<function_id> rids;
        rids = device.build_functions("src/kernel/sort.cl", radix_names);
        if (!fids.size())
                return -1;

        radix_id = rids[0];
        radix_hist_id = rids[1];
        radix_sums_id = rids[2];
        radix_rearrange_id = rids[3];
        std::cout << "Single Kernel load time: " << rt_time.msec_since_snap() 
                  << " ms" << std::endl;
        for (unsigned int i = 0; i < 21; ++i) {
                Radix(pow(2,i));
        }

#endif



        return 0;
}

void Bitonic(int N)
{

        // N = pow(2,20)  ;
        // N = 32 - 8 ;
        // N = pow(2,7) - 23;
        // N = 8388608;
        // N *= 0.77856f;
        N  = std::max(1,N);
        int MIN_PRINT = 0;
        double msec;

        memory_id in_mem_id = device.new_memory();
        memory_id out_mem_id = device.new_memory();

        DeviceMemory& in_mem = device.memory(in_mem_id);
        DeviceMemory& out_mem = device.memory(out_mem_id);

        DeviceFunction& bitonic_local = device.function(bitonic_local_id);
        DeviceFunction& bitonic2 = device.function(bitonic2_id);
        DeviceFunction& bitonic4 = device.function(bitonic4_id);
        DeviceFunction& bitonic8 = device.function(bitonic8_id);
        DeviceFunction& bitonic16 = device.function(bitonic16_id);
        DeviceFunction& bitonic32 = device.function(bitonic32_id);
        DeviceFunction& bitonicl2 = device.function(bitonicl2_id);
        DeviceFunction& bitonicl4 = device.function(bitonicl4_id);

        if (in_mem.initialize(sizeof(cl_float) * N) || 
            out_mem.initialize(sizeof(cl_float) * N))
                exit(1);

        std::vector<float> vals(N);
        for (int i = 0; i < vals.size(); ++i) {
                vals[i] = rand()%4096;
                // vals[i] = 1.f/((rand()%512)+1.f);
        }
        std::vector<float> sorted_vals = vals;
        
        rt_time.snap_time();
        std::sort(sorted_vals.begin(), sorted_vals.end());
        msec = rt_time.msec_since_snap();
        std::cout << "CPU sort time: " << msec << " ms\n";

        if (N < MIN_PRINT) {
                std::cout <<  "In vals: " << std::endl;
                for (int i = 0; i < vals.size(); ++i) {
                        std::cout << vals[i] << " ";
                }
                std::cout << std::endl;
        }

        if (in_mem.write(sizeof(float)*N, &(vals[0])))
                exit(1);

        int inc = 0;
        int dir = 0;

        bool order = true;
        int len;
        int kernels = 0;

        int N2 = 1; // Lowest power of two equal or greater than N
        while (N2 < N)
                N2 <<= 1;
        std::cout << "N: " << N << std::endl;
        std::cout << "N2: " << N2 << std::endl;

        size_t bg2_work_items = N/2 + N%2;
        size_t bg4_work_items = N/4 + (N- N%4);
        size_t bg8_work_items = N/8 + (N- N%8);
        size_t bg16_work_items = N/16 + (N- N%16);
        size_t bg32_work_items = N/32 + (N- N%32);

        size_t bl2_work_items = N/2 + (N-N%2);
        size_t bl4_work_items = N/4 + (N-N%4);

        bg2_work_items = std::max((size_t)1,bg2_work_items);
        bg4_work_items = std::max((size_t)1,bg4_work_items);
        bg8_work_items = std::max((size_t)1,bg8_work_items);
        bg16_work_items = std::max((size_t)1,bg16_work_items);
        bg32_work_items = std::max((size_t)1,bg32_work_items);

        bl2_work_items = std::max((size_t)1,bl2_work_items);
        bl4_work_items = std::max((size_t)1,bl4_work_items);

        size_t max_wg_size = device.max_group_size(0);

        // std::cout << "Start conf: " << std::endl;
        // for (int i = 0; i < vals.size(); ++i) {
        //         std::cout << vals[i] << " ";
        // }
        // std::cout << std::endl;
        rt_time.snap_time();
        if (N < 2*max_wg_size && false) {
                std::cout << "Sorting locally\n";
                bitonic_local.set_arg(0,in_mem);
                bitonic_local.set_arg(1,out_mem);
                bitonic_local.set_arg(2,sizeof(int),&N);
                bitonic_local.set_arg(3,sizeof(int),&N2);
                bitonic_local.set_arg(4,sizeof(cl_float)*N,NULL);
                if (bitonic_local.execute_single_dim(N/2 + N%2))
                        exit(1);
        } else {
                for (len = 1; len < N2; len<<=1) {
                        dir = (len<<1);

                        int inv = ((N-1) & dir) == 0 && dir != N2;
                        int last_step = (dir) == N2;

                        for (inc = len; inc > 0; inc >>= 1) {

                                // /* This local optimization is actually slower!!*/
                                // if (inc<<0 == max_wg_size>>1 || 
                                //     inc<<2 == max_wg_size>>1 || 
                                //     inc<<4 == max_wg_size>>1) {
                                //         int wg_size = std::min((int)max_wg_size,(int)N2);
                                //         // std::cout << "bitonic_l4 called" << std::endl;
                                //         bitonicl4.set_arg(0,in_mem);
                                //         bitonicl4.set_arg(1,out_mem);
                                //         bitonicl4.set_arg(2,sizeof(int),&inc);
                                //         bitonicl4.set_arg(3,sizeof(int),&dir);
                                //         bitonicl4.set_arg(4,sizeof(int),&inv);
                                //         bitonicl4.set_arg(5,sizeof(int),&last_step);
                                //         bitonicl4.set_arg(6,sizeof(int),&N);
                                //         bitonicl4.set_arg(7,sizeof(int),&wg_size);
                                //         bitonicl4.set_arg(8,
                                //                           sizeof(cl_float)*4*max_wg_size,
                                //                           NULL);
                                //         if (bitonicl4.enqueue_single_dim(bl4_work_items))
                                //                 exit(1);
                                //         inc = 0;
                                // } else if (inc <= max_wg_size) {
                                //         int wg_size = std::min((int)max_wg_size,(int)N2);
                                //         wg_size = max_wg_size;
                                //         bitonicl2.set_arg(0,in_mem);
                                //         bitonicl2.set_arg(1,out_mem);
                                //         bitonicl2.set_arg(2,sizeof(int),&inc);
                                //         bitonicl2.set_arg(3,sizeof(int),&dir);
                                //         bitonicl2.set_arg(4,sizeof(int),&inv);
                                //         bitonicl2.set_arg(5,sizeof(int),&last_step);
                                //         bitonicl2.set_arg(6,sizeof(int),&N);
                                //         bitonicl2.set_arg(7,sizeof(int),&wg_size);
                                //         bitonicl2.set_arg(8,
                                //                           sizeof(cl_float)*2*max_wg_size,
                                //                           NULL);
                                //         if (bitonicl2.enqueue_single_dim(bl2_work_items))
                                //                 exit(1);
                                //         inc = 0;
                                // } else if (inc>8) {

                                if (inc>8 && false) { //This is slower than the G16 kernel

                                        // std::cout << "Enqueued b32\n";
                                        inc >>= 4;
                                        bitonic32.set_arg(0,in_mem);
                                        bitonic32.set_arg(1,out_mem);
                                        bitonic32.set_arg(2,sizeof(int),&inc);
                                        bitonic32.set_arg(3,sizeof(int),&dir);
                                        bitonic32.set_arg(4,sizeof(int),&inv);
                                        bitonic32.set_arg(5,sizeof(int),&last_step);
                                        bitonic32.set_arg(6,sizeof(int),&N);
                                        if (bitonic32.enqueue_single_dim(bg32_work_items))
                                                exit(1);

                                } else if (inc>4) {
                                        // std::cout << "Enqueued b16\n";
                                        inc >>= 3;
                                        bitonic16.set_arg(0,in_mem);
                                        bitonic16.set_arg(1,out_mem);
                                        bitonic16.set_arg(2,sizeof(int),&inc);
                                        bitonic16.set_arg(3,sizeof(int),&dir);
                                        bitonic16.set_arg(4,sizeof(int),&inv);
                                        bitonic16.set_arg(5,sizeof(int),&last_step);
                                        bitonic16.set_arg(6,sizeof(int),&N);
                                        if (bitonic16.enqueue_single_dim(bg16_work_items))
                                                exit(1);
                                
                                } else if (inc>2) {
                                        // std::cout << "Enqueued b8\n";
                                        inc >>= 2;
                                        bitonic8.set_arg(0,in_mem);
                                        bitonic8.set_arg(1,out_mem);
                                        bitonic8.set_arg(2,sizeof(int),&inc);
                                        bitonic8.set_arg(3,sizeof(int),&dir);
                                        bitonic8.set_arg(4,sizeof(int),&inv);
                                        bitonic8.set_arg(5,sizeof(int),&last_step);
                                        bitonic8.set_arg(6,sizeof(int),&N);
                                        if (bitonic8.enqueue_single_dim(bg8_work_items))
                                                exit(1);
                                
                                } else if (inc>1) {
                                        // std::cout << "Enqueued b4\n";
                                        inc >>= 1;
                                        bitonic4.set_arg(0,in_mem);
                                        bitonic4.set_arg(1,out_mem);
                                        bitonic4.set_arg(2,sizeof(int),&inc);
                                        bitonic4.set_arg(3,sizeof(int),&dir);
                                        bitonic4.set_arg(4,sizeof(int),&inv);
                                        bitonic4.set_arg(5,sizeof(int),&last_step);
                                        bitonic4.set_arg(6,sizeof(int),&N);
                                        if (bitonic4.enqueue_single_dim(bg4_work_items))
                                                exit(1);
                                
                                } else {
                                        // std::cout << "Enqueued b2\n";
                                        bitonic2.set_arg(0,in_mem);
                                        bitonic2.set_arg(1,out_mem);
                                        bitonic2.set_arg(2,sizeof(int),&inc);
                                        bitonic2.set_arg(3,sizeof(int),&dir);
                                        bitonic2.set_arg(4,sizeof(int),&inv);
                                        bitonic2.set_arg(5,sizeof(int),&last_step);
                                        bitonic2.set_arg(6,sizeof(int),&N);

                                        if (bitonic2.enqueue_single_dim(bg2_work_items))
                                                exit(1);
                                }

                                kernels++;
                                order = !order;
                                device.enqueue_barrier();
                                // in_mem.read(sizeof(float)*N, &(vals[0]));
                                // std::cout << "inc: "  << inc << std::endl;
                                // for (int i = 0; i < vals.size(); ++i) {
                                //         std::cout << vals[i] << " ";
                                // }
                                // std::cout << std::endl;
                        
                        }
                        // std::cout << std::endl;
                }
        }
        device.finish_commands();
        msec = rt_time.msec_since_snap();

        
        if (order || true) {
                if (in_mem.read(sizeof(float)*N, &(vals[0])))
                        exit(1);
        } else {
                if (out_mem.read(sizeof(float)*N, &(vals[0])))
                        exit(1);
        }

        if (N < MIN_PRINT) {
                std::cout <<  "Out vals: " << std::endl;
                for (int i = 0; i < vals.size(); ++i) {
                        std::cout << vals[i] << " ";
                }
                std::cout << std::endl;
        }

        std::cout << "Kernels launched: " << kernels << std::endl;

        for (int i = 0; i < vals.size(); ++i) {
                if (vals[i] != sorted_vals[i]) {
                        std::cout << "Sorting error at " << i << std::endl;
                        std::cout << "Sorted array: " << std::endl;
                        for (int j = 0; j < sorted_vals.size(); ++j) {
                                std::cout << sorted_vals[j] << " ";
                        }
                        std::cout << std::endl;
                        std::cout <<  "Out vals: " << std::endl;
                        for (int i = 0; i < vals.size(); ++i) {
                                std::cout << vals[i] << " ";
                        }
                        std::cout << std::endl;
                        // exit(1);
                        break;
                }
        }

        std::cout << "Time spent: " << msec << " ms"  << std::endl;
        std::cout << std::endl;

        in_mem.release();
        out_mem.release();

        return;
}


void Radix(unsigned int N)
{
        int EPT = 2; //Elements to process per thread/work item
        int RBITS = 4; //Bits to order per pass

        size_t group_size = device.max_group_size(0);
        size_t block_size = EPT*group_size;
        int N2 = 1;
        N2 = N;
        N2 += (N%block_size)? block_size - (N%block_size): 0;

        double msec;

        memory_id in_mem_id = device.new_memory();
        memory_id out_mem_id = device.new_memory();

        DeviceMemory& in_mem = device.memory(in_mem_id);
        DeviceMemory& out_mem = device.memory(out_mem_id);

        DeviceFunction& radix = device.function(radix_id);
        DeviceFunction& radix_hist = device.function(radix_hist_id);
        DeviceFunction& radix_sums = device.function(radix_sums_id);
        DeviceFunction& radix_rearrange = device.function(radix_rearrange_id);

        if (in_mem.initialize(sizeof(cl_uint) * N) || 
            out_mem.initialize(sizeof(cl_uint) * N2))
                exit(1);

        std::vector<unsigned int> vals(N);
        for (int i = 0; i < vals.size(); ++i) {
                vals[i] = rand()%(1<<17);
                // vals[i] = rand()%(1<<31);
                // vals[i] = rand()%(1<<24);
                // vals[i] = i;
                // vals[i] = 1.f/((rand()%512)+1.f);
        }
        std::vector<unsigned int> sorted_vals = vals;
        rt_time.snap_time();

        if (in_mem.write(sizeof(unsigned int)*N, &(vals[0])))
                exit(1);

        std::cout << "Number of elements: " << N << std::endl;
        std::cout << "Padded number of elements: " << N2 << std::endl;

        std::sort(sorted_vals.begin(), sorted_vals.end());
        msec = rt_time.msec_since_snap();
        std::cout << "CPU sort time: " << msec << " ms\n";


        //////////////// Pseudocode:
        //// Radix-sort blocks
        //// Compute offset for all buckets (store in column-major order)
        //// Prefix sum the table of bucket offset
        //// Compute the output location for each element using prefix sum results 
        //// Rinse and repeat
        /////////////////////////////


        ///////////////////// Local radix sort

        rt_time.snap_time();
        size_t histogram_entries = (N2/block_size)*(1<<RBITS);

        memory_id histogram_mem_id = device.new_memory();
        DeviceMemory& histogram_mem = device.memory(histogram_mem_id);
        if (histogram_mem.initialize(sizeof(unsigned int)*histogram_entries))
                return;

        memory_id histogram_sums_id = device.new_memory();
        DeviceMemory& histogram_sums = device.memory(histogram_sums_id);
        if (N2 > block_size) {
                if (histogram_sums.initialize(histogram_mem.size()/(block_size)+1))
                        return;
        } else {
                if (histogram_sums.initialize(1))
                        return;
        }
        

        int passes = 32/RBITS;
        // int passes = 1;

        std::vector<unsigned int> out_vals(N2);
        std::vector<unsigned int> histogram(histogram_entries);

        for (unsigned int pass = 0; pass < passes; pass++) {

                radix.set_arg(0,in_mem);
                radix.set_arg(1,sizeof(unsigned int)*(block_size),NULL);
                radix.set_arg(2,sizeof(unsigned int)*(block_size),NULL);
                radix.set_arg(3,sizeof(unsigned int)*(block_size),NULL);
                radix.set_arg(4,sizeof(unsigned int)*(block_size),NULL);
                radix.set_arg(5,sizeof(unsigned int),&pass);
                radix.set_arg(6,sizeof(int),&N);
                radix.set_arg(7,sizeof(unsigned int)*(1<<RBITS),NULL);
                radix.set_arg(8,histogram_mem);

                //// If we can do everything in a group, then the output should go to in_mem
                if (N2 > block_size) {
                        radix.set_arg(9,out_mem);
                } else {
                        radix.set_arg(9,in_mem);
                }
        
                radix.set_dims(1);
                if (radix.enqueue_single_dim(N2/EPT,group_size)) {
                        std::cout << "Error executing radix\n";
                }
                device.enqueue_barrier();

                if (N2 <= block_size) 
                        continue;

                size_t histogram_work_size = histogram_entries/EPT;
                if (histogram_work_size%block_size) {
                        histogram_work_size += block_size-(histogram_work_size%block_size);
                }

                radix_hist.set_arg(0,histogram_mem);
                radix_hist.set_arg(1,histogram_sums);
                radix_hist.set_arg(2,sizeof(unsigned int), &block_size);
                radix_hist.set_arg(3,sizeof(unsigned int), &histogram_entries);
                radix_hist.set_arg(4,sizeof(unsigned int)*block_size,NULL);
                if (radix_hist.enqueue_single_dim(histogram_work_size,group_size)) {
                        std::cout << "Error executing radix hist\n";
                }
                device.enqueue_barrier();

                unsigned int sum_count = histogram_sums.size()/sizeof(unsigned int)+1;
                radix_sums.set_arg(0,histogram_sums);
                radix_sums.set_arg(1,sizeof(unsigned int)*sum_count, NULL);
                radix_sums.set_arg(2,sizeof(unsigned int), &sum_count);
                radix_sums.set_arg(3,sizeof(unsigned int), &block_size);
                radix_sums.set_arg(4,histogram_mem);
                if (radix_sums.enqueue_single_dim(histogram_entries)) {
                        std::cout << "Error executing sums\n";
                }
                device.enqueue_barrier();


                unsigned int blocks = histogram_entries / (1<<RBITS);
                radix_rearrange.set_arg(0,out_mem);
                radix_rearrange.set_arg(1,histogram_mem);
                radix_rearrange.set_arg(2,sizeof(unsigned int), &blocks);

                radix_rearrange.set_arg(3,sizeof(unsigned int), &N);
                radix_rearrange.set_arg(4,sizeof(unsigned int)*(1<<RBITS),NULL);
                radix_rearrange.set_arg(5,in_mem);
                if (radix_rearrange.enqueue_single_dim(N2,group_size)) {
                        std::cout << "Error executing rearrange\n";
                }
                device.enqueue_barrier();

        }
        device.finish_commands();
        msec = rt_time.msec_since_snap();
        std::cout << "Local radix sort time: " << msec << " ms\n";
        /////////////////////////////////


        std::cout << std::endl;
        histogram_mem.release();
        histogram_sums.release();
        out_mem.release();

        if (in_mem.read(sizeof(unsigned int)*N, &(out_vals[0])))
                exit(1);

        bool sorting_error = false;
        for (uint32_t i = 0; i < N; ++i){
                if (out_vals[i] != sorted_vals[i]) {
                        std::cout << "Sorting error at " << i << "!!!!!" << std::endl;
                        sorting_error = true;
                        break;
                }
        }

        in_mem.release();
        return;
}
