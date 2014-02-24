/* #pragma OPENCL EXTENSION cl_amd_printf:enable */
////////////////////////////////////////////////////////////////////
//////////////// BITONIC SORT //////////////////////////////////////
////////////////////////////////////////////////////////////////////

#define getKey(x) x
typedef float data_t;

kernel void Bitonic_Local(global data_t * in,
                          global data_t * out,
                          int size,
                          int size_padded,
                          local data_t * aux)
{
  int gid = get_local_id(0); // index in workgroup
  int gid_2 = gid+size/2;

  // Load block in AUX[WG]
  aux[gid] = in[gid];
  if (gid_2 < size)
          aux[gid_2] = in[gid_2];
  barrier(CLK_LOCAL_MEM_FENCE); // make sure AUX is entirely up to date

  // Loop on sorted sequence length
  for (int length=1;length<size_padded;length<<=1)
  {
          int dir = (length<<1);
          bool inv = ((size-1) & dir) == 0 && dir != size_padded;
          bool last_step = (dir) == size_padded;
          /* bool direction = ((i & (length<<1)) != 0); // direction of sort:0=asc,1=desc*/
    // Loop on comparison distance (between keys)
    for (int inc=length;inc>0;inc>>=1)
    {
            int low = gid & (inc - 1); //bits below inc
            int i = (gid << 1) - low; //insert 0 at position inc
            bool order_asc = !(dir & i) && !last_step;

            int j = i + inc; // sibling to compare
            data_t iKey,jKey;
            iKey = aux[i];
            if (i+inc < size)
                    jKey = aux[j];

            bool j_is_smaller = (jKey < iKey);
            bool swap = ((j_is_smaller) == order_asc) == inv;
            barrier(CLK_LOCAL_MEM_FENCE);
            if (swap && (i + inc < size)) {
                    aux[i] = jKey;
                    aux[j] = iKey;
            }
            barrier(CLK_LOCAL_MEM_FENCE);
    }
  }

  // Write output
  in[gid] = aux[gid];
  if (gid_2 < size)
          in[gid_2] = aux[gid_2];
  
}

kernel void Bitonic_G2(global data_t * in,
                       global data_t * out,
                       int inc, 
                       int dir, // len << 1
                       int inv, // ((N-1)&dir == 0) && dir != N2
                       int last_step, // dir == N2
                       int size)
{
        int i = get_global_id(0); // thread index
        
        int low = i & (inc - 1); //bits below inc
        i = (i << 1) - low; //insert 0 at position inc

        in += i;
        if (i + inc >= size) return;

        data_t x0 = in[0];
        data_t x1 = in[inc];

        bool order_asc = !(dir & i) && !last_step;
        bool inc_is_smaller = (x1 < x0);

        bool swap = (inc_is_smaller == order_asc) == inv;
        
        if (swap) {
                in[0] =   x1;
                in[inc] = x0;
        }
        
        return;


}

/* #define SORT(a,b) { swap = (((b) < (a)) == order_asc) == inv;\ */
#define SORT(a,b) { swap = ( (b < a) == order_asc ) == inv;\
                    if (swap){\
                      aux = a; a = b; b = aux;} }

kernel void Bitonic_G4(global data_t * in,
                       global data_t * out,
                       int inc, 
                       int dir, // len << 1
                       int inv, // ((N-1)&dir == 0) && dir != N2
                       int last_step, // dir == N2
                       int size)
{
        int i = get_global_id(0); // thread index
        
        int low = i & (inc - 1); //bits below inc
        i = ((i - low) << 2) + low; //insert 00 at position inc

        in += i;
        if (i + inc >= size) return;

        bool x2_in_range = i + 2*inc < size;
        bool x3_in_range = i + 3*inc < size;
        data_t x0,x1,x2,x3, aux;

        x0 = in[0];
        x1 = in[inc];
        if (x2_in_range)
                x2 = in[2*inc];
        if (x3_in_range)
                x3 = in[3*inc];

        bool order_asc = !(dir & i) && !last_step;
        bool swap;

        if (x2_in_range) {
                SORT(x0,x2);
        }

        if (x3_in_range) {
                SORT(x1,x3);
                SORT(x2,x3);
        }
        SORT(x0,x1);

        in[0] = x0;
        in[inc] = x1;
        if (x2_in_range)
                in[2*inc] = x2;
        if (x3_in_range)
                in[3*inc] = x3;
        
        return;
}


#define in_range(X) (i+((X)*inc)<size)

#define ORDERV(x,a,b) { if (in_range(b)) {\
                          swap = ( (x[b] < x[a]) == order_asc ) == inv;\
                          if (swap){\
                                  aux = x[a]; x[a] = x[b]; x[b] = aux;}}}

#define B2V(x,a) { ORDERV(x,a,a+1) }
#define B4V(x,a) { for (int i4=0;i4<2;i4++) { ORDERV(x,a+i4,a+i4+2) } B2V(x,a) B2V(x,a+2) }
#define B8V(x,a) { for (int i8=0;i8<4;i8++) { ORDERV(x,a+i8,a+i8+4) } B4V(x,a) B4V(x,a+4) }
#define B16V(x,a) { for (int i16=0;i16<8;i16++) { ORDERV(x,a+i16,a+i16+8) }\
                B8V(x,a) B8V(x,a+8) }

#define B32V(x,a) { for (int i32=0;i32<16;i32++) { ORDERV(x,a+i32,a+i32+16) }\
                B16V(x,a) B16V(x,a+16) }

kernel void Bitonic_G8(global data_t * in,
                       global data_t * out,
                       int inc, 
                       int dir, // len << 1
                       int inv, // ((N-1)&dir == 0) && dir != N2
                       int last_step, // dir == N2
                       int size)
{
        int i = get_global_id(0); // thread index
        
        int low = i & (inc - 1); //bits below inc
        i = ((i - low) << 3) + low; //insert 000 at position inc

        in += i;
        if (i + inc >= size) return;
        bool order_asc = !(dir & i) && !last_step;

        data_t x[8];
        int offset = 0;
        for (int j = 0; j < 8; ++j) {
                x[j] = in[offset];
                offset+=inc;
        }
        
        bool swap;
        data_t aux;

        // sort!
        B8V(x,0);


        offset = 0;
        for (int j = 0; j < 8 && in_range(j); ++j) {
                in[offset] = x[j];
                offset+=inc;
        }

        return;
}

kernel void Bitonic_G16(global data_t * in,
                        global data_t * out,
                        int inc, 
                        int dir, // len << 1
                        int inv, // ((N-1)&dir == 0) && dir != N2
                        int last_step, // dir == N2
                        int size)
{
        int i = get_global_id(0); // thread index
        
        int low = i & (inc - 1); //bits below inc
        i = ((i - low) << 4) + low; //insert 000 at position inc

        in += i;
        if (i + inc >= size) return;
        bool order_asc = !(dir & i) && !last_step;

        data_t x[16];
        int offset = 0;
        for (int j = 0; j < 16; ++j) {
                x[j] = in[offset];
                offset+=inc;
        }
        
        bool swap;
        data_t aux;

        /* Sort */
        B16V(x,0);


        offset = 0;
        for (int j = 0; j < 16 && in_range(j); ++j) {
                in[offset] = x[j];
                offset+=inc;
        }

        return;
}

kernel void Bitonic_G32(global data_t * in,
                        global data_t * out,
                        int inc, 
                        int dir, // len << 1
                        int inv, // ((N-1)&dir == 0) && dir != N2
                        int last_step, // dir == N2
                        int size)
{
        int i = get_global_id(0); // thread index
        
        int low = i & (inc - 1); //bits below inc
        i = ((i - low) << 5) + low; //insert 000 at position inc

        in += i;
        if (i + inc >= size) return;
        bool order_asc = !(dir & i) && !last_step;

        data_t x[32];
        int offset = 0;
        for (int j = 0; j < 32; ++j) {
                x[j] = in[offset];
                offset+=inc;
        }
        
        bool swap;
        data_t aux;

        /* Sort */
        B32V(x,0);


        offset = 0;
        for (int j = 0; j < 32 && in_range(j); ++j) {
                in[offset] = x[j];
                offset+=inc;
        }

        return;
}



kernel void Bitonic_L2(global data_t* in,
                       global data_t* out,
                       int first_inc,
                       int dir,
                       int inv,
                       int last_step,
                       int size,
                       int wg_size,
                       local data_t* aux)

{
        int gid = get_global_id(0); // thread index
        int wgBits = 2*wg_size - 1; // bit mask to get index in 
                                    //local memory AUX (size is 2*WG)

        for (int inc=first_inc;inc>0;inc>>=1)
        {
                int low = gid & (inc - 1); // low order bits (below INC)
                int i = (gid<<1) - low; // insert 0 at position INC
                bool order_asc = !(dir & i) && !last_step;
                data_t x0,x1;
                
                bool out_of_range = i + inc >= size;

                // Load
                if (inc == first_inc)
                {
                        // First iteration: load from global memory
                        x0 = in[i];
                        if (!out_of_range)
                                x1 = in[i+inc];
                } else {
                        // Other iterations: load from local memory
                        barrier(CLK_LOCAL_MEM_FENCE);
                        x0 = aux[i & wgBits];
                        if (!out_of_range)
                                x1 = aux[(i+inc) & wgBits];
                }
                
                // Sort
                bool inc_is_smaller = (x1 < x0);
                bool swap = ((inc_is_smaller == order_asc) == inv) && !out_of_range;
                if (swap) {
                        data_t aux = x0;
                        x0 = x1;
                        x1 = aux;
                }
                        
                // Store
                if (inc == 1)
                {
                        // Last iteration: store to global memory
                        in[i] = x0;
                        if (!out_of_range)
                                in[i+inc] = x1;
                } else {
                        // Other iterations: store to local memory
                        barrier(CLK_LOCAL_MEM_FENCE);
                        aux[i & wgBits] = x0;
                        if (!out_of_range)
                                aux[(i+inc) & wgBits] = x1;
                }
        }
}

kernel void Bitonic_L4(global data_t* in,
                       global data_t* out,
                       int first_inc,
                       int dir,
                       int inv,
                       int last_step,
                       int size,
                       int wg_size,
                       local data_t* aux_mem)
{
        int gid = get_global_id(0); // thread index
        int wgBits = 4*wg_size- 1; // bit mask to get index in
                                   //local memory AUX (size is 4*WG)
        int inc,low,i;
        bool swap;
        data_t x[4],aux;

        // First iteration, global input, local output
        inc = first_inc>>1;
        low = gid & (inc - 1); // low order bits (below INC)
        i = ((gid - low) << 2) + low; // insert 00 at position INC

        bool order_asc = !(dir & i) && !last_step;

        for (int k=0;k<4;k++) 
                if (in_range(k))
                        x[k] = in[i+k*inc];

        B4V(x,0);

        for (int k=0;k<4;k++) 
                if(in_range(k)) 
                        aux_mem[(i+k*inc) & wgBits] = x[k];
        barrier(CLK_LOCAL_MEM_FENCE);

        // Internal iterations, local input and output
        for ( ;inc>1;inc>>=2)
        {
                low = gid & (inc - 1); // low order bits (below INC)
                i = ((gid - low) << 2) + low; // insert 00 at position INC
                order_asc = !(dir & i) && !last_step;
                for (int k=0;k<4;k++) 
                        if (in_range(k))
                                x[k] = aux_mem[(i+k*inc) & wgBits];
                B4V(x,0);
                barrier(CLK_LOCAL_MEM_FENCE);
                for (int k=0;k<4;k++) 
                        if(in_range(k))
                                aux_mem[(i+k*inc) & wgBits] = x[k];
                barrier(CLK_LOCAL_MEM_FENCE);
        }

        // Final iteration, local input, global output, INC=1
        i = gid << 2;
        order_asc = !(dir & i) && !last_step;
        for (int k=0;k<4;k++) 
                if (i+k < size)
                        x[k] = aux_mem[(i+k) & wgBits];
        B4V(x,0);
        for (int k=0;k<4;k++)
                if (i+k < size)
                        in[i+k] = x[k];
}


////////////////////////////////////////////////////////////////////
////////////////// RADIX SORT //////////////////////////////////////
////////////////////////////////////////////////////////////////////


kernel void Prefix_Sum(local  unsigned int* bit,
                       local  unsigned int* count)
{

        size_t lid = get_local_id(0);
        size_t lsz = get_local_size(0);

        int offset = 1;

        count[2*lid] =   bit[2*lid];
        count[2*lid+1] = bit[2*lid+1];

        for (int d = lsz; d > 0; d >>= 1) {
                barrier(CLK_LOCAL_MEM_FENCE);

                if (lid < d) {
                        int ai = offset * (2*lid+1)-1;
                        int bi = ai + offset;
                        count[bi] += count[ai];
                }
                offset <<= 1;
        }
        barrier(CLK_LOCAL_MEM_FENCE);

        if (lid == 0) {
                count[lsz*2 - 1] = 0;
        }
        
        for (int d = 1; d < lsz<<1; d <<= 1) // traverse down tree & build scan
        {
                offset >>= 1;
                barrier(CLK_LOCAL_MEM_FENCE);
                if (lid < d) {
                        int ai = offset * (2*lid+1)-1;
                        int bi = ai + offset;
                        int t = count[ai];
                        count[ai] = count[bi];
                        count[bi] += t;
                }
        }
        barrier(CLK_LOCAL_MEM_FENCE);

}

kernel void Rearrange(local  unsigned int* in,
                      local  unsigned int* bit,
                      local  unsigned int* count,
                      local  unsigned int* out)
{
        size_t lid = get_local_id(0);
        size_t lsz = get_local_size(0);

        unsigned int total_falses = count[2*lsz-1] + bit[2*lsz-1];

        bool bit1 = !bit[2*lid];
        bool bit2 = !bit[2*lid+1];

        unsigned int id1 = bit1? 2*lid-count[2*lid]+total_falses : count[2*lid];
        unsigned int id2 = bit2? (2*lid+1)-count[2*lid+1]+total_falses : count[2*lid+1];

        out[id1] = in[2*lid];
        out[id2] = in[2*lid+1];
        barrier(CLK_LOCAL_MEM_FENCE);
}


#define RBITS 4
#define RBITS_MASK 0xf // (1<<RBITS) - 1

#define EPT 2 // Elements processed by thread (must be >= 2 and a pow of 2)

void Update_Global_Histogram(local unsigned int*  local_histogram,
                             unsigned int pass,
                             local unsigned int* local_out,
                             global unsigned int* global_histogram)
{
        size_t lid   = get_local_id(0);
        size_t lsz   = get_local_size(0);
        size_t gr    = get_group_id(0);
        size_t gnum  = get_num_groups(0);

        unsigned int l0,l1;

        l0 = (local_out[lid] >> (RBITS*pass)) & RBITS_MASK;
        l1 = (local_out[lid+1] >> (RBITS*pass)) & RBITS_MASK;

        if (lid <= RBITS_MASK)
                local_histogram[lid] = 0;
        barrier(CLK_LOCAL_MEM_FENCE);


        ///////////// Compute local histogram
        if (l0 != l1){
                local_histogram[l0] = lid+1; 
        }

        l0 = (local_out[lid+lsz] >> (RBITS*pass))& RBITS_MASK;
        if (lid != lsz-1) {
                l1 = (local_out[lid+lsz+1] >> (RBITS*pass))& RBITS_MASK;
                if (l0 != l1) 
                        local_histogram[l0] = lid+lsz+1;
        } else {
                local_histogram[l0] = 2*lsz;
        }
        barrier(CLK_LOCAL_MEM_FENCE);

        ////////// (Fill in the gaps where there was no switch)
        if (lid == 0) {
                for (int i = 1; i < (1<<RBITS); ++i)
                        if (local_histogram[i] == 0)
                                local_histogram[i] = local_histogram[i-1];
        }
        barrier(CLK_LOCAL_MEM_FENCE);


        ///////////// Move to global histogram
        if (lid == 0) {
                global_histogram[gr] = local_histogram[0];
        } else if (lid < (1<<RBITS)) {
                global_histogram[gr+lid*gnum] = 
                        local_histogram[lid]-local_histogram[lid-1];
        }
        barrier(CLK_LOCAL_MEM_FENCE);

}


#define MAX_VAL 0xffffffffu // ((1<<32)-1)

kernel void Radix_Local(global unsigned int* in,
                        local  unsigned int* local_in,
                        local  unsigned int* bit,
                        local  unsigned int* local_out,
                        local  unsigned int* count,
                        unsigned int pass,
                        unsigned int valid_elements,
                        local unsigned int*  local_histogram,
                        global unsigned int* global_histogram,
                        global unsigned int* out){

        size_t gid = get_global_id(0);

        size_t lid = get_local_id(0);
        size_t lsz = get_local_size(0);
        size_t gr  = get_group_id(0);

        /////// Move in to start of block to sort
        /////// We process 2*local_size elements per group
        size_t start_offset = 2*lsz*gr;
        in+= 2*lsz*gr;
        out+= 2*lsz*gr;

        if (start_offset + 2*lid < valid_elements)
                local_in[2*lid]   = in[2*lid];
        else
                local_in[2*lid]   = MAX_VAL;
                        
        if (start_offset + 2*lid+1 < valid_elements)
                local_in[2*lid+1] = in[2*lid+1];
        else
                local_in[2*lid+1] = MAX_VAL;

        barrier(CLK_LOCAL_MEM_FENCE);

        //////////// Fill count with { bit = 1 -> 0 //////////////////
        ////////////                 { bit = 0 -> 1 //////////////////
        unsigned int bitmask = 0x1 << (RBITS*pass);
        unsigned int bits_processed = 0;

        do {
                bit[2*lid]   = !(local_in[2*lid] & bitmask);
                barrier(CLK_LOCAL_MEM_FENCE);
                bit[2*lid+1] = !(local_in[2*lid+1] & bitmask);
                barrier(CLK_LOCAL_MEM_FENCE);

                //////////// Do prefix sum on count /////////////////////////
                Prefix_Sum(bit,count);
                barrier(CLK_LOCAL_MEM_FENCE);

                //////////// Rearrange count based on the count /////////////
                Rearrange(local_in,bit,count,local_out);
                barrier(CLK_LOCAL_MEM_FENCE);

                bitmask<<=1;
                bits_processed++;

                if (bits_processed < RBITS) {
                        local_in[2*lid] = local_out[2*lid];
                        local_in[2*lid+1] = local_out[2*lid+1];
                        barrier(CLK_LOCAL_MEM_FENCE);
                }


        } while(bits_processed < RBITS);

        ///////////// Update global histogram //////////////////////////
        Update_Global_Histogram(local_histogram,pass,local_out,global_histogram);
        barrier(CLK_GLOBAL_MEM_FENCE|CLK_LOCAL_MEM_FENCE);

        out[2*lid]   = local_out[2*lid];
        out[2*lid+1] = local_out[2*lid+1];
        return;
}


kernel void Hist_Prefix_Sum(global unsigned int* histogram,
                            global unsigned int* local_sums,
                            unsigned int block_size,
                            unsigned int histogram_entries,
                            local  unsigned int* aux)
{
        size_t gid = get_global_id(0);

        size_t lid = get_local_id(0);
        size_t lsz = get_local_size(0);
        size_t gr  = get_group_id(0);

        size_t start_offset = gr*block_size;
        histogram += start_offset;

        
        if (start_offset + 2*lid < histogram_entries)
                aux[2*lid] = histogram[2*lid];

        if (start_offset + 2*lid+1 < histogram_entries)
                aux[2*lid+1] = histogram[2*lid+1];
        barrier(CLK_LOCAL_MEM_FENCE);

        int offset = 1;
        for (int d = lsz; d > 0; d >>= 1) {
                barrier(CLK_LOCAL_MEM_FENCE);

                if (lid < d) {
                        int ai = offset * (2*lid+1)-1;
                        int bi = ai + offset;
                        aux[bi] += aux[ai];
                }
                offset <<= 1;
        }
        barrier(CLK_LOCAL_MEM_FENCE);

        if (lid == 0) {
                local_sums[gr] = aux[lsz*2-1];
                aux[lsz*2-1] = 0;
        }
        
        
        for (int d = 1; d < lsz<<1; d <<= 1) // traverse down tree & build scan
        {
                offset >>= 1;
                barrier(CLK_LOCAL_MEM_FENCE);
                if (lid < d) {
                        int ai = offset * (2*lid+1)-1;
                        int bi = ai + offset;
                        int t = aux[ai];
                        aux[ai] = aux[bi];
                        aux[bi] += t;
                }
        }
        barrier(CLK_LOCAL_MEM_FENCE);

        if (start_offset + 2*lid < histogram_entries)
                histogram[2*lid] = aux[2*lid];

        if (start_offset + 2*lid+1 < histogram_entries)
                histogram[2*lid+1] = aux[2*lid+1];
}


kernel void Radix_Prefix_Sum(global unsigned int* sums,
                             local unsigned int* local_sums,
                             unsigned int count,
                             unsigned int block_size,
                             global unsigned int* global_histogram)
{
        size_t gid = get_global_id(0);
        size_t lid = get_local_id(0);

        if (lid == 0) {
                local_sums[0] = 0;
        }
        barrier(CLK_LOCAL_MEM_FENCE);

        for (unsigned int i = 1; i < count; ++i) { 
                if (lid == 0) {
                        local_sums[i] = sums[i-1] + local_sums[i-1];
                }
        }
        barrier(CLK_LOCAL_MEM_FENCE);

        unsigned int sum = local_sums[0];
        global_histogram[gid] += sum;
}

kernel void Radix_Rearrange(global unsigned int* in,
                            global unsigned int* global_histogram,
                            unsigned int blocks,
                            unsigned int valid_elements,
                            local  unsigned int* local_offsets,
                            global unsigned int* out)
{
	size_t gid = get_global_id(0);

        size_t lsz = get_local_size(0);
        size_t lid = get_local_id(0);

        size_t gr  = get_group_id(0);
        size_t groups  = get_num_groups(0);

        unsigned int block = ((gr/EPT) % blocks);
        //// Save sizes of buckets
        if (lid < (1<<RBITS) ) {
                local_offsets[lid]  = global_histogram[block+blocks*lid + 1];
                local_offsets[lid] -= global_histogram[block+blocks*lid];
        }
        if (block == blocks-1  && lid == (1<<RBITS)-1) {
                local_offsets[lid]  = get_global_size(0);
                local_offsets[lid] -= global_histogram[block+blocks*lid];
        }
        barrier(CLK_LOCAL_MEM_FENCE);

        //// Compute offset of buckets from sizes
        /* if (lid == 0) { */
        /*         unsigned int last = 0,new_val; */
        /*         for (int i = 1; i < (1<<RBITS); ++i) { */
        /*                 new_val  = local_offsets[i-1] + last; */
        /*                 last = local_offsets[i]; */
        /*                 local_offsets[i]  = new_val; */
        /*         } */
        /*         local_offsets[0] = 0; */
        /* } */
        /* barrier(CLK_LOCAL_MEM_FENCE); */
        unsigned int last = 0,new_val;
        for (int i = 1; i < (1<<RBITS); ++i) {
                if (lid == 0) {
                        new_val  = local_offsets[i-1] + last;
                        last = local_offsets[i];
                        local_offsets[i]  = new_val;
                }
        }

        if (lid == 0)                 
                local_offsets[0] = 0;

        barrier(CLK_LOCAL_MEM_FENCE);

        unsigned int id = lid + lsz*(gr%EPT);

        //// Compute which block bucket this thread belongs to
        unsigned int lbucket = (1<<RBITS)-1;
        unsigned int loffset = id - local_offsets[(1<<RBITS)-1];
        for (int i = 1; i < (1<<RBITS); ++i) {
                if (id < local_offsets[i]) {
                        lbucket = i-1;
                        loffset = id - local_offsets[i-1];
                        break;
                }
        }

        unsigned int global_bucket =  block + blocks*lbucket;
        unsigned int out_idx = global_histogram[global_bucket] + loffset;

        if (out_idx < valid_elements)
                out[out_idx] = in[gid];

}

