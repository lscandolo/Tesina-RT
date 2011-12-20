typedef struct 
{
	float ori[3];
	float dir[3];
	float invDir[3];
	float tMin;
	float tMax;
} Ray;


kernel void
create_ray(global Ray* ray_buffer,
	   read_only float4 pos,
	   read_only float4 dir,
	   read_only float4 right,
	   read_only float4 up)
{
	int x = get_global_id(0);
	int y = get_global_id(1);
	int width = get_global_size(0);
	int height = get_global_size(1);

  	int index = x + width * y;

  	global Ray* ray = &(ray_buffer[index]); 

	float x_start = 0.5f / width;
 	float y_start = 0.5f / height;
 
	float xPosNDC = x_start + ((float)x / (float)width);
	float yPosNDC = y_start + ((float)y / (float)height);
	
	float4 ray_dir = dir + right * (xPosNDC * 2.f - 1.f) + up * (yPosNDC * 2.f - 1.f);

	ray->ori[0] = pos.x;
	ray->ori[1] = pos.y;
	ray->ori[2] = pos.z;

	ray->dir[0] = ray_dir.x;
	ray->dir[1] = ray_dir.y;
 	ray->dir[2] = ray_dir.z;
	
	if (ray_dir.x != 0.f)
		ray->invDir[0] = 1.f/ray_dir.x;
	if (ray_dir.y != 0.f)
		ray->invDir[1] = 1.f/ray_dir.y;
	if (ray_dir.z != 0.f)
		ray->invDir[2] = 1.f/ray_dir.z;

	ray->tMin = 0.f;
	ray->tMax = 1e37f;

}
