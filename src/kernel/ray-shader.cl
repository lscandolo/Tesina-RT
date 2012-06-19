#define CINT_MAX 1e6f

typedef float3 Color;
/* typedef int    ColorInt; */
typedef struct {
	unsigned int r;
	unsigned int g;
	unsigned int b;

}    ColorInt;


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
	Ray  ray;
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
        float3 hit_point;
  
} RayHitInfo;

typedef struct 
{
	Color diffuse;
	float shininess;
	float reflectiveness;
	float refractive_index;
        int texture;
} Material;

/* int __attribute__((always_inline)) */
/* to_int_color(float3 f_rgb) */
/* { */
/* 	int rgb = 0; */
/* 	rgb += floor(f_rgb.s0 * 255); */
/* 	rgb <<= 8; */
/* 	rgb += floor(f_rgb.s1 * 255); */
/* 	rgb <<= 8; */
/* 	rgb += floor(f_rgb.s2 * 255); */
/* 	return rgb; */
/* } */

ColorInt __attribute__((always_inline))
to_color_int(float3 f_rgb)
{
	ColorInt c;
	c.r = floor(f_rgb.s0 * CINT_MAX);
	c.g = floor(f_rgb.s1 * CINT_MAX);
	c.b = floor(f_rgb.s2 * CINT_MAX);
	return c;
}

bool __attribute__((always_inline))
in_range(float f1,float f2){
	return f1 <= 1.0f && f1 >= -1.0f && f2 <= 1.0f && f2 >= -1.0f;
}

void 
shade_sample(global ColorInt* image,
             /* global RayHitInfo* trace_info, */
             /* global RayPlus*   rays, */
             const  RayHitInfo info,
             const  RayPlus ray_plus,
             global Material* material_list,
             global unsigned int* material_map,
             read_only image2d_t x_pos,
             read_only image2d_t x_neg,
             read_only image2d_t y_pos,
             read_only image2d_t y_neg,
             read_only image2d_t z_pos,
             read_only image2d_t z_neg,
             read_only image2d_t texture_0,
             read_only image2d_t texture_1,
             read_only image2d_t texture_2,
             read_only image2d_t texture_3,
             read_only image2d_t texture_4,
             read_only image2d_t texture_5,
             read_only image2d_t texture_6,
             read_only image2d_t texture_7,
             read_only image2d_t texture_8,
             read_only image2d_t texture_9,
             read_only image2d_t texture_10,
             read_only image2d_t texture_11,
             read_only image2d_t texture_12,
             read_only image2d_t texture_13,
             read_only image2d_t texture_14,
             read_only image2d_t texture_15,
             read_only image2d_t texture_16,
             read_only image2d_t texture_17,
             read_only image2d_t texture_18,
             read_only image2d_t texture_19,
             read_only image2d_t texture_20,
             read_only image2d_t texture_21,
             read_only image2d_t texture_22,
             read_only image2d_t texture_23,
             read_only image2d_t texture_24,
             read_only image2d_t texture_25,
             const int use_cubemap,
             const bool use_atomics,
             global Lights* lights)
{

	const sampler_t sampler = 
		CLK_NORMALIZED_COORDS_TRUE |
		CLK_ADDRESS_CLAMP_TO_EDGE |
		CLK_FILTER_LINEAR;

	/* int index = get_global_id(0); */
        /* RayHitInfo info = trace_info[get_global_id(0)]; */
        /* RayPlus    ray_plus = rays[get_global_id(0)]; */

	float3 L = lights->directional.dir; 

	float3 valrgb;

	/* Hit branch: compute lighting from material and hit info */
	if (info.hit) {

		int material_index = material_map[info.id];
		Material mat = material_list[material_index];

                float3 diffuse_rgb;
                if (mat.texture < 0)
                        diffuse_rgb = mat.diffuse;
                else if (mat.texture == 0)
                        diffuse_rgb = read_imagef(texture_0, sampler, info.uv).xyz;
                else if (mat.texture == 1)
                        diffuse_rgb = read_imagef(texture_1, sampler, info.uv).xyz;
                else if (mat.texture == 2)
                        diffuse_rgb = read_imagef(texture_2, sampler, info.uv).xyz;
                else if (mat.texture == 3)
                        diffuse_rgb = read_imagef(texture_3, sampler, info.uv).xyz;
                else if (mat.texture == 4)
                        diffuse_rgb = read_imagef(texture_4, sampler, info.uv).xyz;
                else if (mat.texture == 5)
                        diffuse_rgb = read_imagef(texture_5, sampler, info.uv).xyz;
                else if (mat.texture == 6)
                        diffuse_rgb = read_imagef(texture_6, sampler, info.uv).xyz;
                else if (mat.texture == 7)
                        diffuse_rgb = read_imagef(texture_7, sampler, info.uv).xyz;
                else if (mat.texture == 8)
                        diffuse_rgb = read_imagef(texture_8, sampler, info.uv).xyz;
                else if (mat.texture == 9)
                        diffuse_rgb = read_imagef(texture_9, sampler, info.uv).xyz;
                else if (mat.texture == 10)
                        diffuse_rgb = read_imagef(texture_10, sampler, info.uv).xyz;
                else if (mat.texture == 11)
                        diffuse_rgb = read_imagef(texture_11, sampler, info.uv).xyz;
                else if (mat.texture == 12)
                        diffuse_rgb = read_imagef(texture_12, sampler, info.uv).xyz;
                else if (mat.texture == 13)
                        diffuse_rgb = read_imagef(texture_13, sampler, info.uv).xyz;
                else if (mat.texture == 14)
                        diffuse_rgb = read_imagef(texture_14, sampler, info.uv).xyz;
                else if (mat.texture == 15)
                        diffuse_rgb = read_imagef(texture_15, sampler, info.uv).xyz;
                else if (mat.texture == 16)
                        diffuse_rgb = read_imagef(texture_16, sampler, info.uv).xyz;
                else if (mat.texture == 17)
                        diffuse_rgb = read_imagef(texture_17, sampler, info.uv).xyz;
                else if (mat.texture == 18)
                        diffuse_rgb = read_imagef(texture_18, sampler, info.uv).xyz;
                else if (mat.texture == 19)
                        diffuse_rgb = read_imagef(texture_19, sampler, info.uv).xyz;
                else if (mat.texture == 20)
                        diffuse_rgb = read_imagef(texture_20, sampler, info.uv).xyz;
                else if (mat.texture == 21)
                        diffuse_rgb = read_imagef(texture_21, sampler, info.uv).xyz;
                else if (mat.texture == 22)
                        diffuse_rgb = read_imagef(texture_22, sampler, info.uv).xyz;
                else if (mat.texture == 23)
                        diffuse_rgb = read_imagef(texture_23, sampler, info.uv).xyz;
                else if (mat.texture == 24)
                        diffuse_rgb = read_imagef(texture_24, sampler, info.uv).xyz;
                else if (mat.texture == 25)
                        diffuse_rgb = read_imagef(texture_25, sampler, info.uv).xyz;

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
			spec *= cosL * mat.shininess;

			/*Diffuse*/
			valrgb += dir_rgb * diffuse_rgb * cosL;
			/*Specular*/
			valrgb += dir_rgb * /* specular_rgb * */ spec;
		}

		valrgb *= (1.f-mat.reflectiveness);

		/* Code to paint mesh edges green */
		/* float u,v,w; */
		/* u = info.uv.s0; */
		/* v = info.uv.s1; */
		/* w = 1.f - (u+v); */
		/* if (u < 0.02f || v < 0.02f || w < 0.02f) */
		/* 	valrgb = (float3)(0.f,1.f,0.f); */

	/* Miss branch: compute color from cubemap */
	} else if (use_cubemap) {

		float3 d = (ray_plus.ray.dir);
		float3 inv_d = (ray_plus.ray.invDir);
		
		float3 x_proy = d *  inv_d.x;
		float3 y_proy = d *  inv_d.y;
		float3 z_proy = d *  inv_d.z;

		float2 cm_coords;

		if (d.x > 0.f && in_range(x_proy.y, x_proy.z)) {
			cm_coords.s0 = (-x_proy.z+1.f) * 0.5f;
			cm_coords.s1 = (x_proy.y+1.f) * 0.5f;
			valrgb = read_imagef(x_pos, sampler, cm_coords).xyz;		
		} else if (d.x < 0.f && in_range(x_proy.y, x_proy.z)) {
			cm_coords.s0 = (-x_proy.z+1.f) * 0.5f;
			cm_coords.s1 = (-x_proy.y+1.f) * 0.5f;
			valrgb = read_imagef(x_neg, sampler, cm_coords).xyz;		
		} else if (d.y > 0.f && in_range(y_proy.x, y_proy.z)) {
			cm_coords.s0 = (y_proy.x+1.f) * 0.5f;
			cm_coords.s1 = (-y_proy.z+1.f) * 0.5f;
			valrgb = read_imagef(y_pos, sampler, cm_coords).xyz;		
		} else if (d.y < 0.f && in_range(y_proy.x, y_proy.z)) {
			cm_coords.s0 = (-y_proy.x+1.f) * 0.5f;
			cm_coords.s1 = (-y_proy.z+1.f) * 0.5f;
			valrgb = read_imagef(y_neg, sampler, cm_coords).xyz;		
		} else if (d.z > 0.f && in_range(z_proy.x, z_proy.y)) {
			cm_coords.s0 = (z_proy.x+1.f) * 0.5f;
			cm_coords.s1 = (z_proy.y+1.f) * 0.5f;
			valrgb = read_imagef(z_pos, sampler, cm_coords).xyz;		
		} else if (d.z < 0.f && in_range(z_proy.x, z_proy.y)) {
			cm_coords.s0 = (z_proy.x+1.f) * 0.5f;
			cm_coords.s1 = (-z_proy.y+1.f) * 0.5f;
			valrgb = read_imagef(z_neg, sampler, cm_coords).xyz;		
		}
	} else {
                return;
        }

	const float3 minrgb = (float3)(0.f,0.f,0.f);
	const float3 maxrgb = (float3)(1.f,1.f,1.f);

	float3 f_rgb = clamp(ray_plus.contribution * valrgb,minrgb,maxrgb);

	ColorInt rgb = to_color_int(f_rgb);

        if (use_atomics) {
                global unsigned int* r_ptr = &(image[ray_plus.pixel].r);
                global unsigned int* g_ptr = &(image[ray_plus.pixel].g);
                global unsigned int* b_ptr = &(image[ray_plus.pixel].b);
                
                atomic_add(r_ptr, rgb.r);
                atomic_add(g_ptr, rgb.g);
                atomic_add(b_ptr, rgb.b);

        } else {

                image[ray_plus.pixel].r += rgb.r;
                image[ray_plus.pixel].g += rgb.g;
                image[ray_plus.pixel].b += rgb.b;
        }

}


kernel void 
shade_secondary(global ColorInt* image,
                global RayHitInfo* trace_info,
                global RayPlus* rays,
                global Material* material_list,
                global unsigned int* material_map,
                read_only image2d_t x_pos, read_only image2d_t x_neg,
                read_only image2d_t y_pos, read_only image2d_t y_neg,
                read_only image2d_t z_pos, read_only image2d_t z_neg,
                read_only image2d_t texture_0, read_only image2d_t texture_1,
                read_only image2d_t texture_2, read_only image2d_t texture_3,
                read_only image2d_t texture_4, read_only image2d_t texture_5,
                read_only image2d_t texture_6, read_only image2d_t texture_7,
                read_only image2d_t texture_8, read_only image2d_t texture_9,
                read_only image2d_t texture_10, read_only image2d_t texture_11,
                read_only image2d_t texture_12, read_only image2d_t texture_13,
                read_only image2d_t texture_14, read_only image2d_t texture_15,
                read_only image2d_t texture_16, read_only image2d_t texture_17,
                read_only image2d_t texture_18, read_only image2d_t texture_19,
                read_only image2d_t texture_20, read_only image2d_t texture_21,
                read_only image2d_t texture_22, read_only image2d_t texture_23,
                read_only image2d_t texture_24, read_only image2d_t texture_25,
                int use_cubemap, global Lights* lights)
{

        RayHitInfo hit_info = trace_info[get_global_id(0)];
        RayPlus    ray_plus = rays[get_global_id(0)];

        shade_sample(image,
                     /* trace_info, */
                     /* rays, */
                     hit_info,
                     ray_plus,
                     material_list,
                     material_map,
                     x_pos, x_neg,
                     y_pos, y_neg,
                     z_pos, z_neg,
                     texture_0, texture_1,
                     texture_2, texture_3,
                     texture_4, texture_5,
                     texture_6, texture_7,
                     texture_8, texture_9,
                     texture_10, texture_11,
                     texture_12, texture_13,
                     texture_14, texture_15,
                     texture_16, texture_17,
                     texture_18, texture_19,
                     texture_20, texture_21,
                     texture_22, texture_23,
                     texture_24, texture_25,
                     use_cubemap,
                     true,
                     lights);
}

kernel void 
shade_primary(global ColorInt* image,
                global RayHitInfo* trace_info,
                global RayPlus* rays,
                global Material* material_list,
                global unsigned int* material_map,
                read_only image2d_t x_pos, read_only image2d_t x_neg,
                read_only image2d_t y_pos, read_only image2d_t y_neg,
                read_only image2d_t z_pos, read_only image2d_t z_neg,
                read_only image2d_t texture_0, read_only image2d_t texture_1,
                read_only image2d_t texture_2, read_only image2d_t texture_3,
                read_only image2d_t texture_4, read_only image2d_t texture_5,
                read_only image2d_t texture_6, read_only image2d_t texture_7,
                read_only image2d_t texture_8, read_only image2d_t texture_9,
                read_only image2d_t texture_10, read_only image2d_t texture_11,
                read_only image2d_t texture_12, read_only image2d_t texture_13,
                read_only image2d_t texture_14, read_only image2d_t texture_15,
                read_only image2d_t texture_16, read_only image2d_t texture_17,
                read_only image2d_t texture_18, read_only image2d_t texture_19,
                read_only image2d_t texture_20, read_only image2d_t texture_21,
                read_only image2d_t texture_22, read_only image2d_t texture_23,
                read_only image2d_t texture_24, read_only image2d_t texture_25,
                int use_cubemap, global Lights* lights)
{
        RayHitInfo hit_info = trace_info[get_global_id(0)];
        RayPlus    ray_plus = rays[get_global_id(0)];

        shade_sample(image,
                     /* trace_info, */
                     /* rays, */
                     hit_info,
                     ray_plus,
                     material_list,
                     material_map,
                     x_pos, x_neg,
                     y_pos, y_neg,
                     z_pos, z_neg,
                     texture_0, texture_1,
                     texture_2, texture_3,
                     texture_4, texture_5,
                     texture_6, texture_7,
                     texture_8, texture_9,
                     texture_10, texture_11,
                     texture_12, texture_13,
                     texture_14, texture_15,
                     texture_16, texture_17,
                     texture_18, texture_19,
                     texture_20, texture_21,
                     texture_22, texture_23,
                     texture_24, texture_25,
                     use_cubemap,
                     false,
                     lights);
}














