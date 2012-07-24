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
} Sample;

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
  
} SampleTraceInfo;

kernel void
generate_secondary_rays(global SampleTraceInfo* sample_trace_info,
                        global Sample* old_samples,
                        global Material* material_list,
                        global unsigned int* material_map,
                        global Sample* new_samples,
                        global int* sample_count,
                        int    max_sample_count)
{
        int local_size = get_local_size(0);
        int group_id   = get_group_id(0);
        int l_idx = get_local_id(0);
	int index = get_global_id(0);

	SampleTraceInfo info  = sample_trace_info[index];
	if (!info.hit)
		return;

        int new_ray_id = sample_count[index];
        if (new_ray_id+2 >= max_sample_count)
                return;

	Sample sample = old_samples[index];

	int material_index = material_map[info.id];
	Material mat = material_list[material_index];

        float3 d = -normalize(sample.ray.dir);
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
			new_samples[new_ray_id].ray = new_ray;
			new_samples[new_ray_id].pixel = sample.pixel;
			new_samples[new_ray_id].contribution =
				sample.contribution * mat.reflectiveness * (1.f-R);
                        new_ray_id++;
		}
        }

        if (mat.reflectiveness > 0.f) {

		d = -d;

                new_ray.ori = info.hit_point;
		new_ray.dir = normalize(d - 2.f * n * (dot(d,n)));
		new_ray.invDir = 1.f/new_ray.dir;
		new_samples[new_ray_id].ray = new_ray;
		new_samples[new_ray_id].pixel = sample.pixel;
		new_samples[new_ray_id].contribution =
			sample.contribution * mat.reflectiveness * R;

	}

}

kernel void
mark_secondary_rays(global SampleTraceInfo* sample_trace_info,
                    global Sample* old_samples,
                    global Material* material_list,
                    global unsigned int* material_map,
                    global int* sample_count)
{
	int index = get_global_id(0);
	SampleTraceInfo info = sample_trace_info[index];

        sample_count[index] = 0;

	if (!info.hit)
		return;

	Sample sample = old_samples[index];

	int material_index = material_map[info.id];
	Material mat = material_list[material_index];

        float3 d = -normalize(sample.ray.dir);
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

                        sample_count[index]+=1;
		}
        }

        if (mat.reflectiveness > 0.f) {

                sample_count[index]+=1;
	}

}
