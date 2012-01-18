#define HIT_FLAG 0x1
#define SHADOW_FLAG 0x2

#define HAS_HITP(x) ((x->flags) & HIT_FLAG)
#define HAS_HIT(x) ((x.flags) & HIT_FLAG)

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

typedef struct {

	int flags;
	float t;
	int id;
	float2 uv;
	float3 n;
  
} RayHitInfo;

typedef struct 
{
	float3 rgb;
} Color;

typedef struct 
{
	Color diffuse;
	float shininess;
	float reflectiveness;
	float refractive_index;

} Material;

bool __attribute__((always_inline))
in_range(float f1,float f2){
	return f1 <= 1.0f && f1 >= -1.0f && f2 <= 1.0f && f2 >= -1.0f;
}

kernel void 
update(global Color* image,
       global RayHitInfo* trace_info,
       global RayPlus* rays,
       global Material* material_list,
       global unsigned int* material_map,
       read_only image2d_t x_pos,
       read_only image2d_t x_neg,
       read_only image2d_t y_pos,
       read_only image2d_t y_neg,
       read_only image2d_t z_pos,
       read_only image2d_t z_neg,
       read_only int div)

{
	const sampler_t sampler = 
		CLK_NORMALIZED_COORDS_TRUE |
		CLK_ADDRESS_CLAMP_TO_EDGE |
		CLK_FILTER_NEAREST;

	int index = get_global_id(0);

	/* Image writing computations */
	/* int2 image_size = (int2)(get_image_width(img), get_image_height(img)); */
	/* int2 coords = (int2)( ( (image_size.x-1) * x ) / width, */
	/* 		      ( (image_size.y-1) * y ) / height); */

	float3 L = normalize((float3)(0.1f  * (div-8.f) + 0.1f,
				      0.1f  * (div-8.f),
				      0.2f));

	RayPlus ray_plus = rays[index];
	Ray ray = ray_plus.ray;
	RayHitInfo info  = trace_info[index];

	float3 valrgb;

	/* Hit branch: compute lighting from material and hit info */
	if (HAS_HIT(info)) {

		int material_index = material_map[info.id];
		Material mat = material_list[material_index];

		float3 diffuse_rgb = mat.diffuse.rgb;
		float3 specular_rgb = (float3)(1.0f,1.0f,1.0f);

		/*Light parameters are still hand picked */
		float ambient_intensity = 0.05f;
		float3 ambient_rgb = ambient_intensity * diffuse_rgb;

		float3 light_rgb   = (float3)(1.0f,0.2f,0.2f);

		valrgb = ambient_rgb;

		/* If it's not in shadow, compute diffuse and specular component */
		if (!(info.flags & SHADOW_FLAG)) {
			float3 d = ray.dir;
			float3 n = info.n;
			float3 r = d - 2.f * n * (dot(d,n));
			float cosL = clamp(dot(n,-L),0.f,1.f);
			cosL = pow(cosL,2);

			float spec = clamp(dot(r,-L),0.f,1.f);
			spec = pow(spec,8);
			spec *= cosL;

			/*Diffuse*/
			valrgb += light_rgb * diffuse_rgb * cosL;
			/*Specular*/
			valrgb += light_rgb * specular_rgb * spec;
		}

		valrgb *= (1.f-mat.reflectiveness);

		/* Code to paint mesh edges green */
		/* float u,v,w; */
		/* u = info.uv.s0; */
		/* v = info.uv.s1; */
		/* w = 1.f - (u+v); */
		/* if (u < 0.02 || v < 0.02 || w < 0.02) */
		/* 	valrgb = (float3)(0.f,1.f,0.f); */

	/* Miss branch: compute color from cubemap */
	} else {

		float3 d = normalize(ray.dir);
		
		float3 x_proy = d /  d.x;
		float3 y_proy = d /  d.y;
		float3 z_proy = d /  d.z;

		float4 final_val = (float4)(1.f,0.f,0.f,1.f);

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


		valrgb = final_val.xyz;
	}

	/* if (ray_plus.contribution == 1.f) */
	/* 	write_imagef(img, ray_plus.pixel, (float4)(0.f,0.f,0.f,1.f)); */

	/* float3 prev_val = read_imagef(r_img, sampler, ray_plus.pixel).xyz; */

	/* float3 prev_val = (float3)(0.f,0.f,0.f); */

	/* float3 write_val = prev_val + ray_plus.contribution * valrgb; */

	/* write_imagef(img, ray_plus.pixel, (float4)(write_val,1.f)); */

	if (ray_plus.contribution == 1.f)
		image[ray_plus.pixel].rgb = (float3)(0.f,0.f,0.f);

	float3 prev_val = image[ray_plus.pixel].rgb;
	float3 write_val = prev_val + ray_plus.contribution * valrgb;

	image[ray_plus.pixel].rgb = write_val;
		
}
