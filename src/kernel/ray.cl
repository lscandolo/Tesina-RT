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

/* kernel void */
/* create_ray(global RayPlus* ray_buffer, */
/* 	   read_only float4 pos, */
/* 	   read_only float4 dir, */
/* 	   read_only float4 right, */
/* 	   read_only float4 up) */
/* { */
/* 	int x = get_global_id(0); */
/*   	int y = get_global_id(1); */
/* 	int width = get_global_size(0); */
/* 	int height = get_global_size(1); */

/*   	int index = x + width * y; */

/*   	global Ray* ray = &(ray_buffer[index].ray);  */

/* 	float x_start = 0.5f / width; */
/*  	float y_start = 0.5f / height; */
 
/* 	float xPosNDC = x_start + ((float)x / (float)width); */
/* 	float yPosNDC = y_start + ((float)y / (float)height); */
	
/* 	ray->ori = pos.xyz; */
/* 	ray->dir = (dir + right * (xPosNDC * 2.f - 1.f) + up * (yPosNDC * 2.f - 1.f)).xyz; */

/* 	if (ray->dir.x != 0.f) */
/* 		ray->invDir.x = 1.f/ray->dir.x; */
/* 	if (ray->dir.y != 0.f) */
/* 		ray->invDir.y = 1.f/ray->dir.y; */
/* 	if (ray->dir.z != 0.f) */
/* 		ray->invDir.z = 1.f/ray->dir.z; */

/* 	ray->tMin = 0.f; */
/* 	ray->tMax = 1e37f; */

/* 	ray_buffer[index].pixel = index; */
/* 	ray_buffer[index].contribution = 1.f; */
/* } */


kernel void
create_ray(global RayPlus* ray_buffer,
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
	int real_y = (x/width);

	float xPosNDC = x_start + ((float)real_x / (float)width);
	float yPosNDC = y_start + ((float)real_y / (float)height);
	
	ray->ori = pos.xyz;
	ray->dir = (dir + right * (xPosNDC * 2.f - 1.f) + up * (yPosNDC * 2.f - 1.f)).xyz;

	if (ray->dir.x != 0.f)
		ray->invDir.x = 1.f/ray->dir.x;
	if (ray->dir.y != 0.f)
		ray->invDir.y = 1.f/ray->dir.y;
	if (ray->dir.z != 0.f)
		ray->invDir.z = 1.f/ray->dir.z;

	ray->tMin = 0.f;
	ray->tMax = 1e37f;

	ray_buffer[index].pixel = width * real_y + real_x;
	ray_buffer[index].contribution = 1.f;
}

