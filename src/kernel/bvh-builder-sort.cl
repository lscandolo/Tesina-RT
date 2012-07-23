#define getKey(x) x

#define M_AXIS_BITS  10
#define M_UINT_COUNT 1 //This value needs to be (1+M_AXIS_BITS / 32)

#define MORTON_MAX_F 1024.f // 2^M_AXIS_BITS
#define MORTON_MAX_INV_F 0.000976562.f //1/MORTON_MAX_F

typedef struct {

        unsigned int code[M_UINT_COUNT];

} morton_code_t;


int is_smaller(morton_code_t c1, morton_code_t c2)
{
        if (M_UINT_COUNT == 1) {
                return c1.code[0] < c2.code[0];
        } else {
                for (int i = 0; i < M_UINT_COUNT-1; ++i) {
                        if (c1.code[i] != c2.code[i])
                                return c1.code[i] < c2.code[i];
                }
                return c1.code[M_UINT_COUNT-1] < c2.code[M_UINT_COUNT-1];
        }
}

kernel void morton_sort_g2(global morton_code_t* morton_codes,
                           global unsigned int* triangles,
                           int inc, 
                           int dir, // len << 1
                           int inv, // ((N-1)&dir == 0) && dir != N2
                           int last_step, // dir == N2
                           int size)
{
        int i = get_global_id(0); // thread index
        
        int low = i & (inc - 1); //bits below inc
        i = (i << 1) - low; //insert 0 at position inc

        triangles += i;
        if (i + inc >= size) return;

        morton_code_t x0 = morton_codes[triangles[0]];
        morton_code_t x1 = morton_codes[triangles[inc]];

        bool order_asc = !(dir & i) && !last_step;
        bool inc_is_smaller = is_smaller(x1,x0);

        bool swap = (inc_is_smaller == order_asc) == inv;
        
        if (swap) {
                unsigned int aux;
                aux = triangles[0];
                triangles[0] = triangles[inc];
                triangles[inc] = aux;
        }
        
        return;
}

#define in_range(X) (i+((X)*inc)<size)

#define ORDERV(x,t,a,b) \
        {                                                               \
                if (in_range(b)) {                                      \
                        swap = ( is_smaller(x[t[b]],x[t[a]]) == order_asc ) == inv; \
                        if (swap){                                      \
                                aux = t[a]; t[a] = t[b]; t[b] = aux;    \
                        }                                               \
                }                                                       \
        }                                                               \

#define B2V(x,t,a) { ORDERV(x,t,a,a+1) }

#define B4V(x,t,a)                                      \
        {                                               \
                for (int i4=0;i4<2;i4++){               \
                        ORDERV(x,t,a+i4,a+i4+2);        \
                }                                       \
                B2V(x,t,a) B2V(x,t,a+2);                \
        }

#define B8V(x,t,a)                                      \
        {                                               \
                for (int i8=0;i8<4;i8++){               \
                        ORDERV(x,t,a+i8,a+i8+4);        \
                }                                       \
                B4V(x,t,a) B4V(x,t,a+4);                \
        }

#define B16V(x,t,a) {                                   \
                for (int i16=0;i16<8;i16++) {           \
                        ORDERV(x,t,a+i16,a+i16+8);      \
                }                                       \
                B8V(x,t,a) B8V(x,t,a+8);                \
        }

#define B32V(x,t,a)                                     \
        {                                               \
                for (int i32=0;i32<16;i32++) {          \
                        ORDERV(x,t,a+i32,a+i32+16);     \
                }                                       \
                B16V(x,t,a) B16V(x,t,a+16);             \
        }

kernel void morton_sort_g4(global morton_code_t* morton_codes,
                           global unsigned int* triangles,
                           int inc,
                           int dir, // len << 1
                           int inv, // ((N-1)&dir == 0) && dir != N2
                           int last_step, // dir == N2
                           int size)
{
        int i = get_global_id(0); // thread index
        
        int low = i & (inc - 1); //bits below inc
        i = ((i - low) << 2) + low; //insert 000 at position inc

        triangles    += i;
        if (i + inc >= size) return;
        bool order_asc = !(dir & i) && !last_step;

        morton_code_t x[4];
        unsigned int  t[4];
        unsigned int  tri[4];
        int offset = 0;
        for (int j = 0; j < 4 && in_range(j); ++j) {
                x[j] = morton_codes[triangles[offset]];
                t[j] = j;
                tri[j] = triangles[offset];
                offset+=inc;
        }
        
        bool swap;
        unsigned int aux;

        // sort!
        B4V(x,t,0);

        offset = 0;
        for (int j = 0; j < 4 && in_range(j) && in_range(t[j]); ++j) {
                triangles[offset] = tri[t[j]];
                offset+=inc;
        }

        return;
}

kernel void morton_sort_g8(global morton_code_t* morton_codes,
                           global unsigned int* triangles,
                           int inc,
                           int dir, // len << 1
                           int inv, // ((N-1)&dir == 0) && dir != N2
                           int last_step, // dir == N2
                           int size)
{
        int i = get_global_id(0); // thread index
        
        int low = i & (inc - 1); //bits below inc
        i = ((i - low) << 3) + low; //insert 000 at position inc

        triangles    += i;
        if (i + inc >= size) return;
        bool order_asc = !(dir & i) && !last_step;

        morton_code_t x[8];
        unsigned int  t[8];
        unsigned int  tri[8];
        int offset = 0;
        for (int j = 0; j < 8 && in_range(j); ++j) {
                x[j] = morton_codes[triangles[offset]];
                t[j] = j;
                tri[j] = triangles[offset];
                offset+=inc;
        }
        
        bool swap;
        unsigned int aux;

        // sort!
        B8V(x,t,0);

        offset = 0;
        for (int j = 0; j < 8 && in_range(j) && in_range(t[j]); ++j) {
                triangles[offset] = tri[t[j]];
                offset+=inc;
        }

        return;
}

kernel void morton_sort_g16(global morton_code_t* morton_codes,
                           global unsigned int* triangles,
                           int inc,
                           int dir, // len << 1
                           int inv, // ((N-1)&dir == 0) && dir != N2
                           int last_step, // dir == N2
                           int size)
{
        int i = get_global_id(0); // thread index
        
        int low = i & (inc - 1); //bits below inc
        i = ((i - low) << 4) + low; //insert 000 at position inc

        triangles    += i;
        if (i + inc >= size) return;
        bool order_asc = !(dir & i) && !last_step;

        morton_code_t x[16];
        unsigned int  t[16];
        unsigned int  tri[16];
        int offset = 0;
        for (int j = 0; j < 16 && in_range(j); ++j) {
                x[j] = morton_codes[triangles[offset]];
                t[j] = j;
                tri[j] = triangles[offset];
                offset+=inc;
        }
        
        bool swap;
        unsigned int aux;

        // sort!
        B16V(x,t,0);

        offset = 0;
        for (int j = 0; j < 16 && in_range(j) && in_range(t[j]); ++j) {
                triangles[offset] = tri[t[j]];
                offset+=inc;
        }

        return;
}




