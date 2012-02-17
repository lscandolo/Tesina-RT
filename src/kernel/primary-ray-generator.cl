#define BLOCK_SIDE 16
#define BLOCK_SIZE 256 /*This MUST be BLOCK_SIDE*BLOCK_SIDE*/

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
} RayPlus;

int get_morton_x(int val)
{
	int x = 0;
	int val_bit = 1;

	for (int i = 0; i < 12; ++i) {

		int v = val & val_bit;
		v = v?1:0;
		x |= (v << i);
		val_bit <<= 2;
	}
	return x;
}

int get_morton_y(int val)
{
	int y = 0;
	int val_bit = 2;

	for (int i = 0; i < 12; ++i) {
		int v = val & val_bit;
		v = v?1:0;
		y |= (v << i);
		val_bit <<= 2;
	}
	return y;
}

kernel void
_generate_primary_rays(global RayPlus* ray_buffer,
		      read_only float4 pos,
		      read_only float4 dir,
		      read_only float4 right,
		      read_only float4 up,
		      read_only int width,
		      read_only int height)

{
	int x = get_global_id(0);
	int offset_x = get_global_offset(0);
  	int index = (x-offset_x);

  	global Ray* ray = &(ray_buffer[index].ray); 

	int real_x = x % width;
	int real_y = x / width;
	/* int real_x = get_morton_x(x); */
	/* int real_y = get_morton_y(x); */

	float xPosNDC = (0.5f + (float)real_x) / (float)width;
	float yPosNDC = (0.5f + (float)real_y) / (float)height;
	
	ray->ori = pos.xyz;
	ray->dir = (dir + right * (xPosNDC * 2.f - 1.f) + up * (yPosNDC * 2.f - 1.f)).xyz;

	ray->invDir = 1.f / ray->dir;

	ray->tMin = 0.f;
	ray->tMax = 1e37f;

	ray_buffer[index].pixel = width * real_y + real_x;
	ray_buffer[index].contribution = 1.f;
}


kernel void
generate_primary_rays(global RayPlus* ray_buffer,
			    read_only float4 pos,
			    read_only float4 dir,
			    read_only float4 right,
			    read_only float4 up,
			    read_only int width,
			    read_only int height)

{
	int i = get_global_id(0);
	int offset_i = get_global_offset(0);
  	int index = (i-offset_i);

  	global Ray* ray = &(ray_buffer[index].ray);

	int x_blocks = width / BLOCK_SIDE;
	int y_blocks = height / BLOCK_SIDE;

	int block_rays = x_blocks * y_blocks * (BLOCK_SIZE);
	int right_rays = (width - x_blocks * BLOCK_SIDE) * height;
	/* int top_rays = width*height - (block_rays + right_rays); */

	int x,y;

	if (i < block_rays) {
		
		int block = i / (BLOCK_SIZE);
		int y_block = block / x_blocks;
		int x_block = block % x_blocks;

		int block_pos = i%(BLOCK_SIZE);
		int block_x = block_pos % BLOCK_SIDE;
		int block_y = block_pos / BLOCK_SIDE;

		x = x_block * BLOCK_SIDE + block_x;
		y = y_block * BLOCK_SIDE + block_y;

	} else if (i < block_rays + right_rays) {
		int start_x = x_blocks * BLOCK_SIDE;
		int slab_size = width - start_x;
		int right_i = i - block_rays;
		x = start_x + right_i % slab_size;
		y = right_i / slab_size;
	} else { 
		int start_y = y_blocks * BLOCK_SIDE;
		int start_x = x_blocks * BLOCK_SIDE - 1;
		int slab_size = height - start_y;
		int top_i = i - (block_rays + right_rays);
		x = start_x - top_i / slab_size;
		y = start_y + top_i % slab_size;
	}

	float xPosNDC = (0.5f + (float)x) / (float)width;
	float yPosNDC = (0.5f + (float)y) / (float)height;
	
	ray->ori = pos.xyz;
	ray->dir = (dir + right * (xPosNDC * 2.f - 1.f) + up * (yPosNDC * 2.f - 1.f)).xyz;

	ray->invDir = 1.f / ray->dir;

	ray->tMin = 0.f;
	ray->tMax = 1e37f;

	ray_buffer[index].pixel = width * y + x;
	ray_buffer[index].contribution = 1.f;
}
