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


kernel void
generate_secondary_rays_disc(global SampleTraceInfo* sample_trace_info,
                             global Sample* old_samples,
                             global Material* material_list,
                             global unsigned int* material_map,
                             global Sample* new_samples,
                             global int* sample_count,
                             int    refract_offset,
                             int    max_sample_count)
{
        int local_size = get_local_size(0);
        int group_id   = get_group_id(0);
        int l_idx = get_local_id(0);
	int index = get_global_id(0);

	SampleTraceInfo info  = sample_trace_info[index];
	if (!info.hit)
		return;

        int new_reflect_ray_id = sample_count[index];
        int new_refract_ray_id = sample_count[index+refract_offset];
        /* if (new_ray_id+2 >= max_sample_count) */
        /*         return; */

	Sample sample = old_samples[index];

	int material_index = material_map[info.id];
	Material mat = material_list[material_index];

        float3 d = -normalize(sample.ray.dir);
        float3 n = normalize(info.n);
        float R = 1.f;
        Ray new_ray;
        new_ray.tMin = 0.0001f;
        new_ray.tMax = 1e37f;

	if (mat.refractive_index > 0.f && new_refract_ray_id < max_sample_count) {

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
			new_samples[new_refract_ray_id].ray = new_ray;
			new_samples[new_refract_ray_id].pixel = sample.pixel;
			new_samples[new_refract_ray_id].contribution =
				sample.contribution * mat.reflectiveness * (1.f-R);
		}
        }

        if (mat.reflectiveness > 0.f && new_reflect_ray_id < max_sample_count) {

		d = -d;

                new_ray.ori = info.hit_point;
		new_ray.dir = normalize(d - 2.f * n * (dot(d,n)));
		new_ray.invDir = 1.f/new_ray.dir;
		new_samples[new_reflect_ray_id].ray = new_ray;
		new_samples[new_reflect_ray_id].pixel = sample.pixel;
		new_samples[new_reflect_ray_id].contribution =
			sample.contribution * mat.reflectiveness * R;

	}

}

kernel void
mark_secondary_rays_disc(global SampleTraceInfo* sample_trace_info,
                         global Sample* old_samples,
                         global Material* material_list,
                         global unsigned int* material_map,
                         global int* sample_count,
                                int  refract_offset)
{
	int index = get_global_id(0);
	SampleTraceInfo info = sample_trace_info[index];

        sample_count[index] = 0;
        sample_count[index+refract_offset] = 0;

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
                        sample_count[index+refract_offset]=1;
		}
        }

        if (mat.reflectiveness > 0.f) {

                sample_count[index]=1;
	}

}

void prefix_sum(local   int* bit,
                local   int* sum)
{
        size_t lid = get_local_id(0);
        size_t lsz = get_local_size(0) / 2;

        int offset = 1;

        sum[lid] = bit[lid];

        for (int d = lsz; d > 0; d >>= 1) {
                barrier(CLK_LOCAL_MEM_FENCE);

                if (lid < d) {
                        int ai = offset * (2*lid+1)-1;
                        int bi = ai + offset;
                        sum[bi] += sum[ai];
                }
                offset <<= 1;
        }
        barrier(CLK_LOCAL_MEM_FENCE);

        if (lid == 0) {
                sum[lsz*2 - 1] = 0;
        }
        
        for (int d = 1; d < lsz<<1; d <<= 1) // traverse down tree & build scan
        {
                offset >>= 1;
                barrier(CLK_LOCAL_MEM_FENCE);
                if (lid < d) {
                        int ai = offset * (2*lid+1)-1;
                        int bi = ai + offset;
                        int t = sum[ai];
                        sum[ai] = sum[bi];
                        sum[bi] += t;
                }
        }
        barrier(CLK_LOCAL_MEM_FENCE);
        if (lid == 0) {
                sum[lsz*2] = bit[lsz*2-1] + sum[lsz*2-1];
        }
        barrier(CLK_LOCAL_MEM_FENCE);
}

int compute_new_samples(const SampleTraceInfo info,
                        const Sample sample,
                        global Material* material_list,
                        global unsigned int* material_map,
                        Sample* reflection,
                        Sample* refraction)
{

	if (!info.hit)
		return 0;

	int material_index = material_map[info.id];
	Material mat = material_list[material_index];

        int output_count = 0;

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
			refraction->ray = new_ray;
			refraction->pixel = sample.pixel;
			refraction->contribution =
				sample.contribution * mat.reflectiveness * (1.f-R);
                        output_count++;
		}
        }

        if (mat.reflectiveness > 0.f) {

		d = -d;
                new_ray.ori = info.hit_point;
		new_ray.dir = normalize(d - 2.f * n * (dot(d,n)));
		new_ray.invDir = 1.f/new_ray.dir;
		reflection->ray = new_ray;
		reflection->pixel = sample.pixel;
		reflection->contribution =
			sample.contribution * mat.reflectiveness * R;
                output_count++;
	}

        return output_count;

}

kernel void 
gen_sec_ray(global SampleTraceInfo* sample_trace_info,
            global Material* material_list,
            global unsigned int* material_map,
            global Sample* old_samples,
            global Sample* new_samples,
            global int* counters, // Input and output counters
            local  int* local_output_samples, // Local output (for local ordering)
            local  int* local_prefix_sum, // Local output prefix sum (for local ordering)
            int    old_sample_count,
            int    max_sample_count)
{
        int wgsize = (int)get_local_size(0);
        int wi     = (int)get_local_id(0);

        global int* processed_samples = counters; // Input counter
        global int* output_samples = counters+1; // Output counter

        local int firstSample;

        if (wi == 0) {
                firstSample = atomic_add(processed_samples, wgsize);
        }
        barrier(CLK_LOCAL_MEM_FENCE);

        Sample reflection, refraction;

        while (firstSample < old_sample_count) {
                local_output_samples[wi] = 0;
                
                if (firstSample + wi < old_sample_count)  {
                        local_output_samples[wi] = 
                                compute_new_samples(sample_trace_info[firstSample + wi],
                                                    old_samples[firstSample + wi],
                                                    material_list,
                                                    material_map,
                                                    &reflection,
                                                    &refraction);
                }
                barrier(CLK_LOCAL_MEM_FENCE);

                /// Prefix sum
                prefix_sum(local_output_samples, local_prefix_sum);

                barrier(CLK_LOCAL_MEM_FENCE);

                local int localOffset;
                if (wi == 0) {
                        int outputSamples = local_prefix_sum[wgsize];
                        localOffset  = atomic_add(output_samples, outputSamples);
                }
                barrier(CLK_LOCAL_MEM_FENCE);

                global Sample* newSamplePtr = 
                        new_samples + localOffset + local_prefix_sum[wi];

                if (local_output_samples[wi] > 0) {
                        newSamplePtr[0] = reflection;
                        if (local_output_samples[wi] > 1) {
                                newSamplePtr[1] = refraction;
                        }
                }
                
                
                if (wi == 0) {
                        firstSample = atomic_add(processed_samples, wgsize);
                }
                barrier(CLK_LOCAL_MEM_FENCE);
                
        }
}
