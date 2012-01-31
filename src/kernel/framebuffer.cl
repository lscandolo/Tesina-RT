#define HIT_FLAG 0x1
#define SHADOW_FLAG 0x2

#define HAS_HITP(x) ((x->flags) & HIT_FLAG)
#define HAS_HIT(x) ((x.flags) & HIT_FLAG)

typedef struct 
{
	float3 rgb;
} Color;

typedef struct 
{
	int rgb;
} ColorInt;

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
init(global ColorInt* image)
{
	int index = get_global_id(0);
	image[index].rgb = 0;
}

kernel void 
update(global ColorInt* image,
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

	const float3 minrgb = (float3)(0.f,0.f,0.f);
	const float3 maxrgb = (float3)(1.f,1.f,1.f);

	float3 f_rgb = ray_plus.contribution * (clamp(valrgb,minrgb,maxrgb));

	int rgb = 0.f;
	rgb += f_rgb.s0 * 255;
	rgb <<= 8;
	rgb += f_rgb.s1 * 255;
	rgb <<= 8;
	rgb += f_rgb.s2 * 255;

	global int* pixel_ptr = &(image[ray_plus.pixel].rgb);

	atomic_add(pixel_ptr, rgb);
}

kernel void 
copy(global ColorInt* image,
     write_only image2d_t tex)
{
	int width = get_image_width(tex);
	int heihgt = get_image_height(tex);
	int index = get_global_id(0);

	int2 xy = (int2)(index%width,index/width);

	int rgb = image[index].rgb;
	
	const float inv_255 = 1.f/255.f;

	float r = (rgb >> 16) * inv_255;
	float g = ((rgb & 0xff00) >> 8) * inv_255;
	float b = (rgb & 0xff) * inv_255;

	float4 texval = (float4)(r,g,b,1.f);

	write_imagef(tex, xy, texval);
	
}

