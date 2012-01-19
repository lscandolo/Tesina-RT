#define NO_FLAGS 0x0
#define HIT_FLAG 0x1
#define SHADOW_FLAG 0x2
#define INV_N_FLAG 0x4

#define HAS_HITP(x) ((x->flags) & HIT_FLAG)
#define HAS_HIT(x) ((x.flags) & HIT_FLAG)

/* #define MAX_RAYS 400*400*4 */
#define MAX_RAYS 512*512*4

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
	RayPlus rayplus = ray_in[index];
	Ray original_ray = rayplus.ray;

	if (!HAS_HIT(info)){
		return;
	}

	int material_index = material_map[info.id];
	Material mat = material_list[material_index];

	if (mat.refractive_index > 0.f) {

		float etai;
		float etat;
		if ((info.flags & INV_N_FLAG)){ 
			etat = 1.f;
			etai = mat.refractive_index;
		} else {
			etai = 1.f;
			etat = mat.refractive_index;
		}


		float3 d = -normalize(original_ray.dir);
		float3 n = normalize(info.n);

		float cosi = dot(d,n);

		float rel_eta = (etai / etat);
		float w  = rel_eta * cosi;
		float k  = (1.f + (w-rel_eta) * (w+rel_eta));

		float R = 1.f;
		
		if (k > 0.f) {

			/*Schlik approximation*/
			float R0 = (etat - 1.f) / (etat+1.f);
			R0 *= R0;
			float cos5 = pow((1.f-cosi),5.f);
			R = R0 + (1.f-R0)*cos5;

			k = sqrt(k);
			float3 dir = normalize((w-k)* n - etai*d);

			Ray refract_ray;
			int ray_id = atomic_inc(ray_count);

			if (ray_id >= MAX_RAYS)
				return;

			refract_ray.ori = original_ray.ori + original_ray.dir * info.t;
			refract_ray.dir = dir;
			refract_ray.invDir = 1.f/refract_ray.dir;
			refract_ray.tMin = 0.0001f;
			refract_ray.tMax = 1e37f;
			ray_out[ray_id].ray = refract_ray;
			ray_out[ray_id].pixel = rayplus.pixel;
			ray_out[ray_id].contribution =  
				rayplus.contribution * mat.reflectiveness * (1.f-R);
			/* ray_out[ray_id].contribution =  //!! fix this */
			/* 	ray_in[index].contribution * mat.reflectiveness; */

		}

		Ray reflect_ray;
		d = -d;
		int ray_id = atomic_inc(ray_count);

		if (ray_id >= MAX_RAYS)
			return;

		reflect_ray.ori = original_ray.ori + original_ray.dir * info.t;
		reflect_ray.dir = d - 2.f * n * (dot(d,n));
		reflect_ray.invDir = 1.f/reflect_ray.dir;
		reflect_ray.tMin = 0.0001f;
		reflect_ray.tMax = 1e37f;
		ray_out[ray_id].ray = reflect_ray;
		ray_out[ray_id].pixel = rayplus.pixel;
		ray_out[ray_id].contribution =
			rayplus.contribution * mat.reflectiveness * R;

		return;
	}

	if (mat.reflectiveness > 0.f) { 

		Ray reflect_ray;

		int ray_id = atomic_inc(ray_count);
		if (ray_id >= MAX_RAYS)
			return;

		float3 d = original_ray.dir;
		float3 n = info.n;
		reflect_ray.ori = original_ray.ori + original_ray.dir * info.t;
		reflect_ray.dir = d - 2.f * n * (dot(d,n));
		reflect_ray.invDir = 1.f/reflect_ray.dir;
		reflect_ray.tMin = 0.0001f;
		reflect_ray.tMax = 1e37f;
		ray_out[ray_id].ray = reflect_ray;
		ray_out[ray_id].pixel = rayplus.pixel;
		ray_out[ray_id].contribution = 
			rayplus.contribution * mat.reflectiveness * 0.5f;

		return;
	}
}


