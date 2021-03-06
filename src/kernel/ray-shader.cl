#define CINT_MAX 1e6f

typedef float3 Color;
/* typedef int    ColorInt; */
typedef struct {
	unsigned int r;
	unsigned int g;
	unsigned int b;

}    ColorInt;


typedef enum {
        SPOT_L = 0,
        DIR_L = 1
} light_type;

typedef struct {

	float3 dir;
	Color  color;
} DirectionalLight;

typedef struct {
        float3 pos;
        float  radius;
        float  angle;
	float3 dir;
	Color  color;
} SpotLight;

typedef struct {

        light_type type;
        union {
                DirectionalLight directional;
                SpotLight spot;
        };

} Light;


typedef struct {
        
	Color ambient;
	Light light;
        
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
} Sample;

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
  
} SampleTraceInfo;

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
shade_sample(write_only global ColorInt* image,
             /* global SampleTraceInfo* trace_info, */
             /* global Sample*   samples, */
             const  SampleTraceInfo info,
             const  Sample sample,
             read_only global Material* material_list,
             read_only global unsigned int* material_map,
             read_only image2d_t x_pos,
             read_only image2d_t x_neg,
             read_only image2d_t y_pos,
             read_only image2d_t y_neg,
             read_only image2d_t z_pos,
             read_only image2d_t z_neg,
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
             const int use_cubemap,
             constant Lights* lights)
{

	const sampler_t sampler = 
		CLK_NORMALIZED_COORDS_TRUE |
		CLK_ADDRESS_REPEAT |
		CLK_FILTER_LINEAR;

	const sampler_t cb_sampler = 
		CLK_NORMALIZED_COORDS_TRUE |
		CLK_ADDRESS_CLAMP_TO_EDGE |
		CLK_FILTER_LINEAR;

	/* int index = get_global_id(0); */
        /* SampleTraceInfo info = trace_info[get_global_id(0)]; */
        /* Sample    sample = samples[get_global_id(0)]; */

	float3 L;
        float  LD;
        float3 dir_rgb;  
        if (lights->light.type == DIR_L) {
                L = lights->light.directional.dir;
                dir_rgb = lights->light.directional.color;
                LD = 1.f;
        }  else if (lights->light.type == SPOT_L) {
                L = normalize(info.hit_point - lights->light.spot.pos);
                dir_rgb = lights->light.spot.color;
                LD = dot(L,lights->light.spot.dir) - lights->light.spot.angle;
                LD /= 1.f - lights->light.spot.angle;
                /* LD = pow(LD,0.5f); */
                LD = clamp(LD,0.f,1.f);
        }
        dir_rgb *= LD;

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
		valrgb = ambient_rgb;

		/* If it's not in shadow, compute diffuse and specular component */
		if (!info.shadow_hit) {

			float3 d = sample.ray.dir;
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
		
		float3 x_proy = sample.ray.dir *  sample.ray.invDir.x;
		float3 y_proy = sample.ray.dir *  sample.ray.invDir.y;
		float3 z_proy = sample.ray.dir *  sample.ray.invDir.z;

		float2 cm_coords;

		if (sample.ray.dir.x > 0.f && in_range(x_proy.y, x_proy.z)) {
			cm_coords.s0 = (-x_proy.z+1.f) * 0.5f;
			cm_coords.s1 = (x_proy.y+1.f) * 0.5f;
			valrgb = read_imagef(x_pos, cb_sampler, cm_coords).xyz;		
		} else if (sample.ray.dir.x < 0.f && in_range(x_proy.y, x_proy.z)) {
			cm_coords.s0 = (-x_proy.z+1.f) * 0.5f;
			cm_coords.s1 = (-x_proy.y+1.f) * 0.5f;
			valrgb = read_imagef(x_neg, cb_sampler, cm_coords).xyz;		
		} else if (sample.ray.dir.y > 0.f && in_range(y_proy.x, y_proy.z)) {
			cm_coords.s0 = (y_proy.x+1.f) * 0.5f;
			cm_coords.s1 = (-y_proy.z+1.f) * 0.5f;
			valrgb = read_imagef(y_pos, cb_sampler, cm_coords).xyz;		
		} else if (sample.ray.dir.y < 0.f && in_range(y_proy.x, y_proy.z)) {
			cm_coords.s0 = (-y_proy.x+1.f) * 0.5f;
			cm_coords.s1 = (-y_proy.z+1.f) * 0.5f;
			valrgb = read_imagef(y_neg, cb_sampler, cm_coords).xyz;		
		} else if (sample.ray.dir.z > 0.f && in_range(z_proy.x, z_proy.y)) {
			cm_coords.s0 = (z_proy.x+1.f) * 0.5f;
			cm_coords.s1 = (z_proy.y+1.f) * 0.5f;
			valrgb = read_imagef(z_pos, cb_sampler, cm_coords).xyz;		
		} else if (sample.ray.dir.z < 0.f && in_range(z_proy.x, z_proy.y)) {
			cm_coords.s0 = (z_proy.x+1.f) * 0.5f;
			cm_coords.s1 = (-z_proy.y+1.f) * 0.5f;
			valrgb = read_imagef(z_neg, cb_sampler, cm_coords).xyz;		
		}
	} else {
                return;
        }

	const float3 minrgb = (float3)(0.f,0.f,0.f);
	const float3 maxrgb = (float3)(1.f,1.f,1.f);

	float3 f_rgb = clamp(sample.contribution * valrgb,minrgb,maxrgb);
	ColorInt rgb = to_color_int(f_rgb);

        /* if (use_atomics) { */
        /*         global unsigned int* r_ptr = &(image[sample.pixel].r); */
        /*         global unsigned int* g_ptr = &(image[sample.pixel].g); */
        /*         global unsigned int* b_ptr = &(image[sample.pixel].b); */
                
        /*         atomic_add(r_ptr, rgb.r); */
        /*         atomic_add(g_ptr, rgb.g); */
        /*         atomic_add(b_ptr, rgb.b); */

        /* } else { */
                image[sample.pixel].r += rgb.r;
                image[sample.pixel].g += rgb.g;
                image[sample.pixel].b += rgb.b;
        /* } */

}

kernel void 
shade_primary(write_only global ColorInt* image,
              read_only global SampleTraceInfo* trace_info,
              read_only global Sample* samples,
              read_only global Material* material_list,
              read_only global unsigned int* material_map,
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
              unsigned int sample_count, 
              int use_cubemap, 
              constant Lights* lights)
{
        SampleTraceInfo hit_info = trace_info[get_global_id(0)];
        Sample    sample = samples[get_global_id(0)];

        shade_sample(image,
                     hit_info,
                     sample,
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
                     lights);
}

kernel void 
shade_secondary(write_only global ColorInt* image,
                read_only global SampleTraceInfo* trace_info,
                read_only global Sample* samples,
                read_only global Material* material_list,
                read_only global unsigned int* material_map,
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
                unsigned int sample_count, 
                int use_cubemap, 
                constant Lights* lights)
{
        size_t index = get_global_id(0);

        SampleTraceInfo hit_info = trace_info[index];
        Sample    sample = samples[index];

        int pixel = sample.pixel;

        if (index != 0) {
                if (samples[index-1].pixel == pixel) {
                        return;
                }
        }
        
        while(1) {
                shade_sample(image,
                             hit_info,
                             sample,
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
                             lights);
                index++;
                sample = samples[index];
                if (sample.pixel != pixel || index >= sample_count)
                        return;
                hit_info = trace_info[index];
        }
                
}
