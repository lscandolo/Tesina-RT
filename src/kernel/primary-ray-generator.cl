#define BLOCK_SIDE 32
#define BLOCK_SIZE BLOCK_SIDE*BLOCK_SIDE /*This MUST be BLOCK_SIDE*BLOCK_SIDE*/

typedef struct 
{
	float3 ori;
	float3 dir;
	float3 invDir;
	float tMin;
	float tMax;
} Ray;
 
typedef struct 
{
	Ray   ray;
	int  pixel;
	float contribution;
} Sample;

typedef struct
{
	float ox;
	float oy;
	float contribution;
} PixelSample;

kernel void
generate_primary_rays(global Sample* samples,
		      read_only float4 pos,
		      read_only float4 dir,
		      read_only float4 right,
		      read_only float4 up,
		      read_only int width,
		      read_only int height,
                      read_only int quad_size,
		      read_only int spp,
		      global    PixelSample* pixel_samples)

{
	int gid = get_global_id(0);
	int offset = get_global_offset(0);
  	int index = (gid-offset);

        int pixels = width * height;
        int i = gid%pixels;
	PixelSample psample = pixel_samples[gid/pixels];

        int block_side = quad_size;
        int block_area = quad_size * quad_size;

	int x_blocks = width / block_side;
	int y_blocks = height / block_side;

	int block_rays = x_blocks * y_blocks * (block_area);
	int right_rays = (width - x_blocks * block_side) * height;

	int x,y;

	if (i < block_rays) {
		
		int block = i / (block_area);
		int y_block = block / x_blocks;
		int x_block = block % x_blocks;

		int block_pos = i%(block_area);
		int block_x = block_pos % block_side;
		int block_y = block_pos / block_side;

		x = x_block * block_side + block_x;
		y = y_block * block_side + block_y;

	} else if (i < block_rays + right_rays) {
		int start_x = x_blocks * block_side;
		int slab_size = width - start_x;
		int right_i = i - block_rays;
		x = start_x + right_i % slab_size;
		y = right_i / slab_size;
	} else { 
		int start_y = y_blocks * block_side;
		int slab_size = height - start_y;
		int top_i = i - (block_rays + right_rays);
		x = top_i / slab_size;
		y = start_y + (top_i % slab_size);
	}

  	global Ray* ray = &(samples[index].ray);
	float xPosNDC = (0.5f + (float)x + psample.ox) / (float)width;
	float yPosNDC = (0.5f + (float)y + psample.oy) / (float)height;
	
	ray->ori = pos.xyz;
	ray->dir = normalize((dir + 
                              right * (xPosNDC * 2.f - 1.f) + 
                              up * (yPosNDC * 2.f - 1.f)).xyz);
        

	ray->invDir = 1.f / ray->dir;

	ray->tMin = 0.f;
	ray->tMax = 1e36f;

	samples[index].pixel = width * y + x;
	samples[index].contribution = psample.contribution;
}

/// Experimental, only works if width == height and they are a power of 2
kernel void
generate_primary_rays_zcurve(global Sample* samples,
                             read_only float4 pos,
                             read_only float4 dir,
                             read_only float4 right,
                             read_only float4 up,
                             read_only int width,
                             read_only int height,
                             read_only int quad_size,
                             read_only int spp,
                             global    PixelSample* pixel_samples)
{
        ///////////// Value
	int gid = get_global_id(0);        // Id of this item
	int offset = get_global_offset(0); // Offset from real first ray
  	int index = (gid-offset);          // Index of output sample
        /////////////////////////////////////

        int pixels = width * height; // Pixels on screen
        int i = gid%pixels;          // Pixel corresponding to this item
	PixelSample psample = pixel_samples[gid/pixels]; // Per-pixel sample for this item

        /////// Get biggest power of 2 smaller than min_side (for 32 bits)
        int min_side = min(width, height);
        int z_size = min_side;
        z_size--;
        z_size |= z_size >> 1; z_size |= z_size >> 2;  z_size |= z_size >> 4;
        z_size |= z_size >> 8; z_size |= z_size >> 16;
        z_size++; 
        z_size >>= (z_size != min_side);
        ////////////////////////////////////////////

        int x=0,y=0; // Output pixel

        ///////////////////////// Build z-curve samples
        if (i < z_size * z_size) {
                for (int j = 0; j < 32; ++j) {
                        if (j % 2 == 0)
                                x |= (( (1<<j) & i) != 0) << (j / 2);
                        else
                                y |= (( (1<<j) & i) != 0) << (j / 2);
                }
        }
        ///////////////////////// Build right swath
        else if (i - (z_size * z_size) < (width - z_size) * height) {

        }
        ///////////////////////// Build top swath
        else {

        }

        ///////////////////////////////////////////

  	global Ray* ray = &(samples[index].ray);

	float xPosNDC = (0.5f + (float)x + psample.ox) / (float)width;
	float yPosNDC = (0.5f + (float)y + psample.oy) / (float)height;
	
	ray->ori = pos.xyz;
	ray->dir = normalize((dir + 
                              right * (xPosNDC * 2.f - 1.f) + 
                              up * (yPosNDC * 2.f - 1.f)).xyz);
        

	ray->invDir = 1.f / ray->dir;

	ray->tMin = 0.f;
	ray->tMax = 1e37f;

	samples[index].pixel = width * y + x;
	samples[index].contribution = psample.contribution;
}
