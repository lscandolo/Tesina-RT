#define NO_FLAGS 0x0
#define HIT_FLAG 0x1
#define SHADOW_FLAG 0x2
#define INV_N_FLAG 0x4

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
	int   pixel;
	float contribution;
} RayPlus;


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

typedef struct {

	int flags;
	float t;
	int id;
	float2 uv;
	float3 n;
  
} RayHitInfo;

kernel void 
split(global RayHitInfo* ray_hit_info,
      global RayPlus* ray_in,
      global Material* material_list,
      global unsigned int* material_map,
      global RayPlus* ray_out,
      global int* ray_count)
{
	int index = get_global_id(0);

	RayHitInfo info  = ray_hit_info[index];
	Ray original_ray = ray_in[index].ray;

	if (!HAS_HIT(info)){
		return;
	}

	int material_index = material_map[info.id];
	Material mat = material_list[material_index];

	if (mat.refractive_index > 0.f) {

		float ind;
		if ((info.flags & INV_N_FLAG))
			ind = mat.refractive_index;
		else
			ind = 1.f/mat.refractive_index;


		float3 d = -normalize(original_ray.dir);
		float3 n = normalize(info.n);
		float w  = ind * (dot(d,n));
		float k  = (1.f + (w-ind) * (w+ind));
		if (k > 0.00001f) {

			k = sqrt(k);

			float3 dir = normalize((w-k)* n - ind*d);

			if (dot(d,n) < 0.1f)
				return;

			Ray refract_ray;
			int ray_id = atomic_inc(ray_count);

			if (ray_id >= 400*400)
				return;

			/* dir = original_ray.dir; */

			refract_ray.ori = original_ray.ori + original_ray.dir * info.t;
			refract_ray.dir = dir;
			refract_ray.invDir = 1.f/refract_ray.dir;
			refract_ray.tMin = 0.0001f;
			refract_ray.tMax = 1e37f;
			ray_out[ray_id].ray = refract_ray;
			ray_out[ray_id].pixel = ray_in[index].pixel;
			ray_out[ray_id].contribution =  ray_in[index].contribution * 0.95f;
			/* ray_out[ray_id].contribution =  //!! fix this */
			/* 	ray_in[index].contribution * mat.reflectiveness; */

		} 

		return;
	}

	if (mat.reflectiveness > 0.f) { 

		Ray reflect_ray;
		int ray_id = atomic_inc(ray_count);

		if (ray_id >= 400*400)
			return;

		float3 d = original_ray.dir;
		float3 n = info.n;
		reflect_ray.ori = original_ray.ori + original_ray.dir * info.t;
		reflect_ray.dir = d - 2.f * n * (dot(d,n));
		reflect_ray.invDir = 1.f/reflect_ray.dir;
		reflect_ray.tMin = 0.0001f;
		reflect_ray.tMax = 1e37f;
		ray_out[ray_id].ray = reflect_ray;
		ray_out[ray_id].pixel = ray_in[index].pixel;
		ray_out[ray_id].contribution = 
			ray_in[index].contribution * mat.reflectiveness;

		return;
	}
}


