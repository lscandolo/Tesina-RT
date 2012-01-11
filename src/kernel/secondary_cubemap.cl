#define REFLECTION_FLAG 0x1
#define REFRACTION_FLAG 0x2
#define INTERIOR_HIT    0x4

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

typedef struct 
{
	float3 rgb;
} Color;

typedef struct 
{
	float3 hit_point;
	float3 dir;
	float3 normal;
	int flags;
	float refraction_index;
} bounce;

typedef struct {

	int    flags;
	int    mat_id;
	Color  color1;
	Color  color2;
} ray_level;


bool __attribute__((always_inline))
in_range(float f1,float f2){
	return f1 <= 1.0f && f1 >= -1.0f && f2 <= 1.0f && f2 >= -1.0f;
}


float3
cm_color(float3 d,
	 read_only image2d_t x_pos,
	 read_only image2d_t x_neg,
	 read_only image2d_t y_pos,
	 read_only image2d_t y_neg,
	 read_only image2d_t z_pos,
	 read_only image2d_t z_neg)
{

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

	return final_val.xyz;
}

kernel void 
cubemap(write_only image2d_t img,
	global ray_level* ray_levels,
	global bounce* bounce_info,
	read_only image2d_t x_pos,
	read_only image2d_t x_neg,
	read_only image2d_t y_pos,
	read_only image2d_t y_neg,
	read_only image2d_t z_pos,
	read_only image2d_t z_neg)

{
	int x = get_global_id(0);
	int y = get_global_id(1);
        int width = get_global_size(0);
	int height = get_global_size(1);
	int index = x + y * (width);

	/* Image writing computations */
	int2 image_size = (int2)(get_image_width(img), get_image_height(img));
	int2 coords = (int2)( ( (image_size.x-1) * x ) / (width-1),
			      ( (image_size.y-1) * y ) / (height-1) );

	ray_level level = ray_levels[index];
	bounce b =  bounce_info[index];

	float3 dir;

	if (b.flags & REFLECTION_FLAG) {

		dir = b.dir - 2.f * b.normal * (dot(b.dir,b.normal));
		ray_levels[index].color1.rgb = cm_color(dir,
							x_pos,x_neg,
							y_pos,y_neg,
							z_pos,z_neg);
	}

	if (b.flags & REFRACTION_FLAG && false) {

		/*Compute refraction dir*/
		dir = b.dir - 2.f * b.normal * (dot(b.dir,b.normal));
		ray_levels[index].color2.rgb = cm_color(dir,
							x_pos,x_neg,
							y_pos,y_neg,
							z_pos,z_neg);
	}


	/*This will be gone soon*/

	if (b.flags & REFLECTION_FLAG) {
		float3 col = ray_levels[index].color1.rgb;
		float4 valf = (float4)(0.2f * col + 0.8f * level.color1.rgb, 1.0f);
		write_imagef(img, coords, valf);
	}
}

