typedef float3 Color;
typedef int    ColorInt;

typedef struct {

	float3 dir;
	Color  color;
} DirectionalLight;

typedef struct {
	
	Color ambient;
	DirectionalLight directional;

} Lights;

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

	bool hit;
	bool shadow_hit;
	bool inverse_n;
	bool reserved;
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

int __attribute__((always_inline))
to_int_color(float3 f_rgb)
{
	int rgb = 0;
	rgb += f_rgb.s0 * 255;
	rgb <<= 8;
	rgb += f_rgb.s1 * 255;
	rgb <<= 8;
	rgb += f_rgb.s2 * 255;
	return rgb;
}

bool __attribute__((always_inline))
in_range(float f1,float f2){
	return f1 <= 1.0f && f1 >= -1.0f && f2 <= 1.0f && f2 >= -1.0f;
}

kernel void 
shade(global ColorInt* image,
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
      global Lights* lights)

{

	const sampler_t sampler = 
		CLK_NORMALIZED_COORDS_TRUE |
		CLK_ADDRESS_CLAMP_TO_EDGE |
		CLK_FILTER_LINEAR;

	int index = get_global_id(0);

	float3 L = lights->directional.dir; 

	RayPlus ray_plus = rays[index];
	RayHitInfo info  = trace_info[index];

	float3 valrgb;

	/* Hit branch: compute lighting from material and hit info */
	if (info.hit) {

		int material_index = material_map[info.id];
		Material mat = material_list[material_index];

		float3 diffuse_rgb = mat.diffuse;
		float3 specular_rgb = (float3)(1.0f,1.0f,1.0f);

		float3 ambient_rgb = lights->ambient * diffuse_rgb;

		float3 dir_rgb   = lights->directional.color;

		valrgb = ambient_rgb;

		/* If it's not in shadow, compute diffuse and specular component */
		if (!info.shadow_hit) {

			float3 d = ray_plus.ray.dir;
			float3 n = info.n;
			float3 r = d - 2.f * n * (dot(d,n));
			float cosL = clamp(dot(n,-L),0.f,1.f);
			cosL = pow(cosL,2);

			float spec = clamp(dot(r,-L),0.f,1.f);
			spec = pow(spec,8);
			spec *= cosL;

			/*Diffuse*/
			valrgb += dir_rgb * diffuse_rgb * cosL;
			/*Specular*/
			valrgb += dir_rgb * specular_rgb * spec;
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

		float3 d = (ray_plus.ray.dir);
		float3 inv_d = (ray_plus.ray.invDir);
		
		float3 x_proy = d *  inv_d.x;
		float3 y_proy = d *  inv_d.y;
		float3 z_proy = d *  inv_d.z;

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

	int rgb = to_int_color(f_rgb);

	global int* pixel_ptr = (image + ray_plus.pixel);
	atomic_add(pixel_ptr, rgb);

}
