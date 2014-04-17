#define NUM_BANKS 16  
#define LOG_NUM_BANKS 4  
#define CONFLICT_FREE_OFFSET(n) (((n) >> LOG_NUM_BANKS) + ((n) >> (2 * LOG_NUM_BANKS)))

/* #define CONFLICT_FREE_OFFSET(n) ((n) >> (LOG_NUM_BANKS)) */
/* #define CONFLICT_FREE_OFFSET(n) n */

void kernel scan_local_uint(global unsigned int* in,
                            global unsigned int* sums,
                            local  unsigned int* aux,
                            unsigned int size,
                            global unsigned int* out)
{
        int global_size = get_global_size(0);
        int local_size = get_local_size(0);
        int group_id   = get_group_id(0);

        int in_start_idx = group_id * local_size * 2;

        /////////// Have to check range
        if (group_id == get_num_groups(0) - 1) {
        /* if (group_id == get_num_groups(0) - 1) { */

                int g_idx = get_global_id(0);
                int l_idx = get_local_id(0);

                in  += in_start_idx;
                out += in_start_idx;

                int ai = l_idx;
                int bi = l_idx + local_size;
                int bankOffsetA = CONFLICT_FREE_OFFSET(ai);
                int bankOffsetB = CONFLICT_FREE_OFFSET(bi);

                if (in_start_idx + ai < size)
                        aux[ai+bankOffsetA] = in[ai];
                else
                        aux[ai+bankOffsetA] = 0;

                if (in_start_idx + bi < size)
                        aux[bi+bankOffsetB] = in[bi];
                else
                        aux[bi+bankOffsetB] = 0;

                int offset = 1;

                for (int d = local_size; d > 0; d >>= 1) {
                        barrier(CLK_LOCAL_MEM_FENCE);

                        if (l_idx < d) {
                                int ai = offset * (2*l_idx+1)-1;
                                int bi = ai + offset;
                                ai += CONFLICT_FREE_OFFSET(ai);
                                bi += CONFLICT_FREE_OFFSET(bi);
                                aux[bi] += aux[ai];
                        }
                        offset <<= 1;
                }
                barrier(CLK_LOCAL_MEM_FENCE);


                if (l_idx == 0) {
                        int sum_id = local_size*2-1 + CONFLICT_FREE_OFFSET(local_size*2-1);
                        sums[group_id] = aux[sum_id];
                        aux[sum_id] = 0;
                }
        
                // traverse down tree & build scan  
                for (int d = 1; d < local_size<<1; d <<= 1) 
                {  
                        offset >>= 1;  
                        barrier(CLK_LOCAL_MEM_FENCE);
                        if (l_idx < d) {          
                                int ai = offset * (2*l_idx+1)-1;
                                int bi = ai + offset;
                                ai += CONFLICT_FREE_OFFSET(ai);
                                bi += CONFLICT_FREE_OFFSET(bi);
                                int t = aux[ai];  
                                aux[ai] = aux[bi];  
                                aux[bi] += t;   
                        }
                }
                barrier(CLK_LOCAL_MEM_FENCE);

                if (in_start_idx + ai <= size)
                        out[ai]   = aux[ai + bankOffsetA];

                if (in_start_idx + bi <= size)
                        out[bi]   = aux[bi + bankOffsetB];

        ///////////// Don't have to check range
        } else {

                int g_idx = get_global_id(0);
                int l_idx = get_local_id(0);

                in  += in_start_idx;
                out += in_start_idx;

                int ai = l_idx;
                int bi = l_idx + local_size;
                int bankOffsetA = CONFLICT_FREE_OFFSET(ai);
                int bankOffsetB = CONFLICT_FREE_OFFSET(bi);

                aux[ai+bankOffsetA] = in[ai];
                aux[bi+bankOffsetB] = in[bi];

                int offset = 1;

                for (int d = local_size; d > 0; d >>= 1) {
                        barrier(CLK_LOCAL_MEM_FENCE);

                        if (l_idx < d) {
                                int ai = offset * (2*l_idx+1)-1;
                                int bi = ai + offset;
                                ai += CONFLICT_FREE_OFFSET(ai);
                                bi += CONFLICT_FREE_OFFSET(bi);
                                aux[bi] += aux[ai];
                        }
                        offset <<= 1;
                }
                barrier(CLK_LOCAL_MEM_FENCE);


                if (l_idx == 0) {
                        int sum_id = local_size*2-1 + CONFLICT_FREE_OFFSET(local_size*2-1);
                        sums[group_id] = aux[sum_id];
                        aux[sum_id] = 0;
                }
        
                // traverse down tree & build scan  
                for (int d = 1; d < local_size<<1; d <<= 1) 
                {  
                        offset >>= 1;  
                        barrier(CLK_LOCAL_MEM_FENCE);
                        if (l_idx < d) {          
                                int ai = offset * (2*l_idx+1)-1;
                                int bi = ai + offset;
                                ai += CONFLICT_FREE_OFFSET(ai);
                                bi += CONFLICT_FREE_OFFSET(bi);
                                int t = aux[ai];  
                                aux[ai] = aux[bi];  
                                aux[bi] += t;   
                        }
                }
                barrier(CLK_LOCAL_MEM_FENCE);

                out[ai]   = aux[ai + bankOffsetA];
                out[bi]   = aux[bi + bankOffsetB];

        }
}

void kernel scan_post_uint(global unsigned int* out,
                           global unsigned int* sums,
                           unsigned int block_size)

{
        size_t global_id = get_global_id(0);
        size_t local_id = get_local_id(0);

        local unsigned int sum;
        if (local_id == 0) {
                if (global_id >= block_size)
                        sum = sums[global_id/block_size];
                else
                        sum = 0;
        }
        barrier(CLK_LOCAL_MEM_FENCE);

        out[global_id] += sum;
}

