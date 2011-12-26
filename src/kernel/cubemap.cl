typedef struct
{
	float3 ori;
	float3 dir;
	float3 invDir;
	float tMin;
	float tMax;
} Ray;

typedef struct {

	int hit;
	float t;
	int id;
	float2 uv;
  
} RayHitInfo;

bool __attribute__((always_inline))
in_range(float f1,float f2){
	return f1 <= 1.0f && f1 >= -1.0f && f2 <= 1.0f && f2 >= -1.0f;
}


kernel void 
cubemap(write_only image2d_t img,
	global RayHitInfo* ray_hit_info,
	global Ray* ray_buffer,
	read_only image2d_t x_pos,
	read_only image2d_t x_neg,
	read_only image2d_t y_pos,
	read_only image2d_t y_neg,
	read_only image2d_t z_pos,
	read_only image2d_t z_neg)

{
	int x = get_global_id(0);
	int y = get_global_id(1);
        int width = get_global_size(0)-1;
	int height = get_global_size(1)-1;
	int index = x + y * (width+1);

	/* Image writing computations */
	int2 image_size = (int2)(get_image_width(img), get_image_height(img));
	int2 coords = (int2)( ( (image_size.x-1) * x ) / width,
			      ( (image_size.y-1) * y ) / height);

	RayHitInfo info  = ray_hit_info[index];
	if (info.hit)
		return;

	Ray ray = ray_buffer[index];
	float3 d = ray.dir;
	d = normalize(d);

	float3 x_proy = d /  d.x;
	float3 y_proy = d /  d.y;
	float3 z_proy = d /  d.z;

	float4 final_val = (float4)(1.f,0.f,0.f,1.f);
	const sampler_t sampler = 
 		CLK_NORMALIZED_COORDS_TRUE |
		CLK_ADDRESS_CLAMP_TO_EDGE |
		CLK_FILTER_NEAREST;

	float2 cm_coords;

	if (d.x > 0.f && in_range(x_proy.y, x_proy.z)) {
		cm_coords.s0 = (-x_proy.z+1.f) * 0.5f;
		cm_coords.s1 = (x_proy.y+1.f) * 0.5f;
		final_val = read_imagef(x_pos, sampler, cm_coords);		
	}

	if (d.x < 0.f && in_range(x_proy.y, x_proy.z)) {
		cm_coords.s0 = (-x_proy.z+1.f) * 0.5f;
 		cm_coords.s1 = (-x_proy.y+1.f) * 0.5f;
		final_val = read_imagef(x_neg, sampler, cm_coords);		
	}

	if (d.y > 0.f && in_range(y_proy.x, y_proy.z)) {
		cm_coords.s0 = (y_proy.x+1.f) * 0.5f;
		cm_coords.s1 = (-y_proy.z+1.f) * 0.5f;
		final_val = read_imagef(y_pos, sampler, cm_coords);		
	}

	if (d.y < 0.f && in_range(y_proy.x, y_proy.z)) {
		cm_coords.s0 = (-y_proy.x+1.f) * 0.5f;
 		cm_coords.s1 = (-y_proy.z+1.f) * 0.5f;
		final_val = read_imagef(y_neg, sampler, cm_coords);		
	}

	if (d.z > 0.f && in_range(z_proy.x, z_proy.y)) {
		cm_coords.s0 = (z_proy.x+1.f) * 0.5f;
		cm_coords.s1 = (z_proy.y+1.f) * 0.5f;
		final_val = read_imagef(z_pos, sampler, cm_coords);		
 	}

	if (d.z < 0.f && in_range(z_proy.x, z_proy.y)) {
		cm_coords.s0 = (z_proy.x+1.f) * 0.5f;
 		cm_coords.s1 = (-z_proy.y+1.f) * 0.5f;
		final_val = read_imagef(z_neg, sampler, cm_coords);		
	}


	float4 valf = (float4)(1.f,1.f,1.f,1.f);

	write_imagef(img, coords, final_val);
		
}
