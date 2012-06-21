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

typedef float3 Color;

typedef struct 
{
	Color diffuse;
	float shininess;
	float reflectiveness;
	float refractive_index;
        int texture;

} Material;

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

/* kernel void */
/* generate_secondary_rays(global RayHitInfo* ray_hit_info, */
/* 			global RayPlus* ray_in, */
/* 			global Material* material_list, */
/* 			global unsigned int* material_map, */
/* 			global RayPlus* ray_out, */
/* 			global int* ray_count, */
/* 			const  int max_rays) */
/* { */

/* 	int index = get_global_id(0); */
/* 	RayHitInfo info  = ray_hit_info[index]; */

/* 	if (!info.hit) */
/* 		return; */

/* 	RayPlus rayplus = ray_in[index]; */
/* 	Ray original_ray = rayplus.ray; */

/* 	int material_index = material_map[info.id]; */
/* 	Material mat = material_list[material_index]; */


/*         int refract_ray_id = 2*index; */
/*         int reflect_ray_id = 2*index+1; */

/*         ray_out[refract_ray_id].contribution = -rayplus.pixel; */
/*         ray_out[reflect_ray_id].contribution = -rayplus.pixel; */

/*         float3 d = -normalize(original_ray.dir); */
/*         float3 n = normalize(info.n); */
/*         float R = 1.f; */

/* 	if (mat.refractive_index > 0.f) { */

/* 		float etai; */
/* 		float etat; */
/* 		if (info.inverse_n){ */
/* 			etat = 1.f; */
/* 			etai = mat.refractive_index; */
/* 		} else { */
/* 			etai = 1.f; */
/* 			etat = mat.refractive_index; */
/* 		} */

/* 		float cosi = dot(d,n); */

/* 		float rel_eta = (etai / etat); */
/* 		float w  = rel_eta * cosi; */
/* 		float k  = (1.f + (w-rel_eta) * (w+rel_eta)); */

/* 		if (k > 0.f) { */

/* 			/\*Schlik approximation*\/ */
/* 			float R0 = (etat - 1.f) / (etat+1.f); */
/* 			R0 *= R0; */
/* 			float cos5 = pow((1.f-cosi),5.f); */
/* 			R = R0 + (1.f-R0)*cos5; */

/* 			k = sqrt(k); */
/* 			float3 dir = normalize((w-k)* n - etai*d); */

/* 			Ray refract_ray; */

/* 			refract_ray_id = atomic_inc(ray_count); */
/* 			if (refract_ray_id >= max_rays) */
/* 				return; */

/*                         refract_ray.ori = info.hit_point; */
/* 			refract_ray.dir = dir; */
/* 			refract_ray.invDir = 1.f/refract_ray.dir; */
/* 			refract_ray.tMin = 0.0001f; */
/* 			refract_ray.tMax = 1e37f; */
/* 			ray_out[refract_ray_id].ray = refract_ray; */
/* 			ray_out[refract_ray_id].pixel = rayplus.pixel; */
/* 			ray_out[refract_ray_id].contribution = */
/* 				rayplus.contribution * mat.reflectiveness * (1.f-R); */
/* 		} */
/*         } */

/*         if (mat.reflectiveness > 0.f) { */

/* 		Ray reflect_ray; */
/* 		d = -d; */

/* 		reflect_ray_id = atomic_inc(ray_count); */
/* 		if (reflect_ray_id >= max_rays) */
/* 			return; */

/* 		reflect_ray.ori = info.hit_point; */
/* 		reflect_ray.dir = d - 2.f * n * (dot(d,n)); */
/* 		reflect_ray.invDir = 1.f/reflect_ray.dir; */
/* 		reflect_ray.tMin = 0.0001f; */
/* 		reflect_ray.tMax = 1e37f; */
/* 		ray_out[reflect_ray_id].ray = reflect_ray; */
/* 		ray_out[reflect_ray_id].pixel = rayplus.pixel; */
/* 		ray_out[reflect_ray_id].contribution = */
/* 			rayplus.contribution * mat.reflectiveness * R; */

/* 	} */

/* } */



kernel void
generate_secondary_rays(global RayHitInfo* ray_hit_info,
                        global RayPlus* ray_in,
                        global Material* material_list,
                        global unsigned int* material_map,
                        global RayPlus* ray_out,
                        global int* ray_count,
                        global int* totals,
                        int    max_ray_count)
{
        int local_size = get_local_size(0);
        int group_id   = get_group_id(0);
        int l_idx = get_local_id(0);
	int index = get_global_id(0);

        int local_start_idx = group_id * local_size;
        ray_count += local_start_idx;

        local int total;
        if (l_idx == 0) {
                if (group_id < 2)
                        total = 0;
                else
                        total = totals[(group_id>>1)-1];
        }
        barrier(CLK_GLOBAL_MEM_FENCE);

	RayHitInfo info  = ray_hit_info[index];
	if (!info.hit)
		return;

        int new_ray_id = ray_count[l_idx] + total;
        if (new_ray_id >= max_ray_count)
                return;

	RayPlus rayplus = ray_in[index];

	int material_index = material_map[info.id];
	Material mat = material_list[material_index];

        float3 d = -normalize(rayplus.ray.dir);
        float3 n = normalize(info.n);
        float R = 1.f;
        Ray new_ray;
        new_ray.tMin = 0.0001f;
        new_ray.tMax = 1e37f;

	if (mat.refractive_index > 0.f) {

		float etai;
		float etat;
		if (info.inverse_n){
			etat = 1.f;
			etai = mat.refractive_index;
		} else {
			etai = 1.f;
			etat = mat.refractive_index;
		}

		float cosi = dot(d,n);

		float rel_eta = (etai / etat);
		float w  = rel_eta * cosi;
		float k  = (1.f + (w-rel_eta) * (w+rel_eta));

		if (k > 0.f) {

			/*Schlik approximation*/
			float R0 = (etat - 1.f) / (etat+1.f);
			R0 *= R0;
			float cos5 = pow((1.f-cosi),5.f);
			R = R0 + (1.f-R0)*cos5;

			k = sqrt(k);

                        new_ray.ori = info.hit_point;
			new_ray.dir = normalize((w-k)* n - etai*d);
			new_ray.invDir = 1.f/new_ray.dir;
			ray_out[new_ray_id].ray = new_ray;
			ray_out[new_ray_id].pixel = rayplus.pixel;
			ray_out[new_ray_id].contribution =
				rayplus.contribution * mat.reflectiveness * (1.f-R);
                        new_ray_id++;
		}
        }

        if (mat.reflectiveness > 0.f) {

		d = -d;

                new_ray.ori = info.hit_point;
		new_ray.dir = d - 2.f * n * (dot(d,n));
		new_ray.invDir = 1.f/new_ray.dir;
		ray_out[new_ray_id].ray = new_ray;
		ray_out[new_ray_id].pixel = rayplus.pixel;
		ray_out[new_ray_id].contribution =
			rayplus.contribution * mat.reflectiveness * R;

	}

}

kernel void
mark_secondary_rays(global RayHitInfo* ray_hit_info,
                    global RayPlus* ray_in,
                    global Material* material_list,
                    global unsigned int* material_map,
                    global int* ray_count)
{
	int index = get_global_id(0);
	RayHitInfo info  = ray_hit_info[index];

        /* int reflect_ray_id = 2*index; */
        /* int refract_ray_id = 2*index+1; */

        ray_count[index] = 0;

        /* ray_count[refract_ray_id] = 0; */
        /* ray_count[reflect_ray_id] = 0; */

	if (!info.hit)
		return;

	RayPlus rayplus = ray_in[index];

	int material_index = material_map[info.id];
	Material mat = material_list[material_index];

        float3 d = -normalize(rayplus.ray.dir);
        float3 n = normalize(info.n);

	if (mat.refractive_index > 0.f) {

		float etai;
		float etat;
		if (info.inverse_n){
			etat = 1.f;
			etai = mat.refractive_index;
		} else {
			etai = 1.f;
			etat = mat.refractive_index;
		}

		float cosi = dot(d,n);
		float rel_eta = (etai / etat);
		float w  = rel_eta * cosi;
		float k  = (1.f + (w-rel_eta) * (w+rel_eta));

		if (k > 0.f) {

                        ray_count[index]+=1;
		}
        }

        if (mat.reflectiveness > 0.f) {

                ray_count[index]+=1;
	}

}

kernel void local_prefix_sum(global int* in,
                             global int* out,
                             local  int* aux,
                             global int* totals)
{
        int local_size = get_local_size(0);
        int group_id   = get_group_id(0);

        int in_start_idx = group_id * local_size * 2;

        int g_idx = get_global_id(0);
        int l_idx = get_local_id(0);

        in  += in_start_idx;
        out += in_start_idx;

        aux[2*l_idx] = in[2*l_idx];
        aux[2*l_idx+1] = in[2*l_idx+1];
        
        int offset = 1;

        for (int d = local_size; d > 0; d >>= 1) {
                barrier(CLK_GLOBAL_MEM_FENCE);

                if (l_idx < d) {
                        int ai = offset * (2*l_idx+1)-1;
                        int bi = ai + offset;
                        aux[bi] += aux[ai];
                }
                offset <<= 1;
        }
        barrier(CLK_GLOBAL_MEM_FENCE);

        int sum = aux[local_size*2-1];

        if (l_idx == 0) {
                aux[local_size*2 - 1] = 0;
        }
        
        for (int d = 1; d < local_size<<1; d <<= 1) // traverse down tree & build scan  
        {  
                offset >>= 1;  
                barrier(CLK_GLOBAL_MEM_FENCE);
                if (l_idx < d) {          
                        int ai = offset * (2*l_idx+1)-1;
                        int bi = ai + offset;
                        int t = aux[ai];  
                        aux[ai] = aux[bi];  
                        aux[bi] += t;   
                }
        }
        barrier(CLK_GLOBAL_MEM_FENCE);

        if (l_idx == 0) {
                /* aux[local_size*2 ] = sum; */
                totals[group_id] = sum;
        }
        barrier(CLK_GLOBAL_MEM_FENCE);

        out[2*l_idx]   = aux[2*l_idx ];
        out[2*l_idx+1] = aux[2*l_idx+1 ];
        
        
}

kernel void totals_prefix_sum(global int* totals,
                              int n) {
        for (int i = 1; i < n; ++i)
                totals[i] += totals[i-1];
}



kernel void global_prefix_sum(global int* in,
                              global int* out,
                              global int* totals)
{

        local int sum;

        int local_size = get_local_size(0);
        int group_id   = get_group_id(0);
        int num_groups = get_num_groups(0);

        int in_start_idx = group_id * local_size * 2;
        int g_idx = get_global_id(0);
        int l_idx = get_local_id(0);
        in  += in_start_idx;
        out += in_start_idx;
        
        if (group_id == 0) return;

        if (l_idx == 0) {
                sum = totals[group_id-1];
        }
        barrier(CLK_GLOBAL_MEM_FENCE);

        out[2*l_idx]   += sum;
        out[2*l_idx+1] += sum;

}


/* kernel void copy(global RayPlus *ray_in, */
/*                  global RayPlus *ray_out, */
/*                  global int *count) */
/* { */
/*         int g_idx = get_global_id(0); */

/*         if (g_idx == 0) { */
/*                 if (count[0]) */
/*                         ray_out[0] = ray_in[0]; */
/*                 return; */
/*         } else { */
/*                 if (count[g_idx] != count[g_idx-1]) */
/*                         ray_out[count[g_idx]-1] = ray_in[g_idx]; */
/*                 return; */
/*         } */
/* } */
