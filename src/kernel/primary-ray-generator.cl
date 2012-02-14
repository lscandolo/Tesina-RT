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
generate_primary_rays(global RayPlus* ray_buffer,
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

	float x_start = 0.5f / width;
 	float y_start = 0.5f / height;
 
	int real_x = x % width;
	int real_y = x / width;
	/* int real_x = get_morton_x(x); */
	/* int real_y = get_morton_y(x); */

	float xPosNDC = x_start + ((float)real_x / (float)width);
	float yPosNDC = y_start + ((float)real_y / (float)height);
	
	ray->ori = pos.xyz;
	ray->dir = (dir + right * (xPosNDC * 2.f - 1.f) + up * (yPosNDC * 2.f - 1.f)).xyz;

	/* if (ray->dir.x != 0.f) */
		ray->invDir.x = 1.f/ray->dir.x;
	/* if (ray->dir.y != 0.f) */
		ray->invDir.y = 1.f/ray->dir.y;
	/* if (ray->dir.z != 0.f) */
		ray->invDir.z = 1.f/ray->dir.z;

	ray->tMin = 0.f;
	ray->tMax = 1e37f;

	ray_buffer[index].pixel = width * real_y + real_x;
	ray_buffer[index].contribution = 1.f;
}

