#define NUM_BANKS 16  
#define LOG_NUM_BANKS 4  
#define CONFLICT_FREE_OFFSET(n) (((n) >> LOG_NUM_BANKS) + ((n) >> (2 * LOG_NUM_BANKS)))
/* #define CONFLICT_FREE_OFFSET(n) ((n) >> (LOG_NUM_BANKS)) */

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
        
        for (int d = 1; d < local_size<<1; d <<= 1) // traverse down tree & build scan  
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

        if (in_start_idx + ai < size)
                out[ai]   = aux[ai + bankOffsetA];

        if (in_start_idx + bi < size)
                out[bi]   = aux[bi + bankOffsetB];

}
/////Orig:
/* BBox builder time: 8.34336 ms */
/* Morton encoder time: 22.7906 ms */
/* Morton sorter: 74.6043 ms */
/* bvh emmission time: 192.977 ms */
/* segment offset scan time: 46.577 */
/* treelet build time: 24.4586 */
/* node count time: 6.23509 */
/* node offset scan time: 31.1431 */
/* node build time: 26.7007 */
/* Node bbox compute time: 69.8693 ms */



void kernel scan_post_uint(global unsigned int* out,
                           global unsigned int* sums,
                           unsigned int block_size)

{
        size_t global_id = get_global_id(0);
        size_t local_id = get_local_id(0);

        local unsigned int sum;
        if (local_id == 0) {
                sum = sums[global_id/block_size]; // sums per each 2*group_size
        }
        barrier(CLK_LOCAL_MEM_FENCE);

        out[global_id] += sum;
}


/* void kernel scan_local_uint(global unsigned int* in, */
/*                             global unsigned int* sums, */
/*                             local  unsigned int* aux, */
/*                             unsigned int size, */
/*                             global unsigned int* out) */
/* { */
/*         int global_size = get_global_size(0); */
/*         int local_size = get_local_size(0); */
/*         int group_id   = get_group_id(0); */

/*         int in_start_idx = group_id * local_size * 2; */

/*         int g_idx = get_global_id(0); */
/*         int l_idx = get_local_id(0); */

/*         in  += in_start_idx; */
/*         out += in_start_idx; */

/*         /\* int bankOffsetA = CONFLICT_FREE_OFFSET(ai); *\/ */
/*         /\* int bankOffsetB = CONFLICT_FREE_OFFSET(bi); *\/ */

/*         /\* int ai = l_idx; *\/ */
/*         /\* int bi = l_idx + local_size; *\/ */


/*         aux[2*l_idx]   = in_start_idx + 2*l_idx   < size ? in[2*l_idx]   : 0; */
/*         aux[2*l_idx+1] = in_start_idx + 2*l_idx+1 < size ? in[2*l_idx+1] : 0; */

/*         int offset = 1; */

/*         for (int d = local_size; d > 0; d >>= 1) { */
/*                 barrier(CLK_GLOBAL_MEM_FENCE); */

/*                 if (l_idx < d) { */
/*                         int ai = offset * (2*l_idx+1)-1; */
/*                         int bi = ai + offset; */
/*                         aux[bi] += aux[ai]; */
/*                 } */
/*                 offset <<= 1; */
/*         } */
/*         barrier(CLK_GLOBAL_MEM_FENCE); */

/*         int sum = aux[local_size*2-1]; */

/*         if (l_idx == 0) { */
/*                 aux[local_size*2 - 1] = 0; */
/*         } */
        
/*         for (int d = 1; d < local_size<<1; d <<= 1) // traverse down tree & build scan   */
/*         {   */
/*                 offset >>= 1;   */
/*                 barrier(CLK_GLOBAL_MEM_FENCE); */
/*                 if (l_idx < d) {           */
/*                         int ai = offset * (2*l_idx+1)-1; */
/*                         int bi = ai + offset; */
/*                         int t = aux[ai];   */
/*                         aux[ai] = aux[bi];   */
/*                         aux[bi] += t;    */
/*                 } */
/*         } */
/*         barrier(CLK_GLOBAL_MEM_FENCE); */

/*         if (l_idx == 0) { */
/*                 sums[group_id] = sum; */
/*         } */
/*         barrier(CLK_GLOBAL_MEM_FENCE); */

/*         if (in_start_idx + 2*l_idx   < size) */
/*                 out[2*l_idx]   = aux[2*l_idx ]; */

/*         if (in_start_idx + 2*l_idx+1   < size) */
/*                 out[2*l_idx+1]   = aux[2*l_idx+1 ]; */

/* } */

/* void kernel scan_post_uint(global unsigned int* out, */
/*                            global unsigned int* sums, */
/*                            unsigned int block_size) */

/* { */
/*         size_t global_id = get_global_id(0); */
/*         size_t local_id = get_local_id(0); */

/*         local unsigned int sum; */
/*         if (local_id == 0) { */
/*                 sum = sums[global_id/block_size]; // sums per each 2*group_size */
/*         } */
/*         barrier(CLK_LOCAL_MEM_FENCE); */

/*         out[global_id] += sum; */
/* } */
