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
        float3 position;
        float3 normal;
        float4 tangent;
        float3 bitangent;
        float2 texCoord;
} Vertex;


typedef unsigned int tri_id;

typedef struct {
	float3 hi;
	float3 lo;
} BBox;

typedef struct {

	BBox bbox;
	unsigned int l_child, r_child;
	unsigned int parent;
	unsigned int start_index, end_index;
	char split_axis;
	char leaf;

} BVHNode;

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

void __attribute__((always_inline))
merge_hit_info(RayHitInfo* best_info, RayHitInfo* new_info){
	if (!best_info->hit ||
	    (new_info->hit && new_info->t < best_info->t)) {
		*best_info = *new_info;
	}
}
 
bool /* __attribute__((always_inline)) */
bbox_hit(BBox bbox,
	 Ray ray)
{
	float tMin = ray.tMin;
	float tMax = ray.tMax;

	float3 axis_t_lo, axis_t_hi;

 	axis_t_lo = (bbox.lo - ray.ori) * ray.invDir;
	axis_t_hi = (bbox.hi - ray.ori) * ray.invDir;

	float3 axis_t_max = fmax(axis_t_lo, axis_t_hi);
	float3 axis_t_min = fmin(axis_t_lo, axis_t_hi);

	if (fabs(ray.invDir.x) > 0.0001f) {
		tMin = fmax(tMin, axis_t_min.x); tMax = fmin(tMax, axis_t_max.x);
	}
	if (fabs(ray.invDir.y) > 0.0001f) {
		tMin = fmax(tMin, axis_t_min.y); tMax = fmin(tMax, axis_t_max.y);
	}
	if (fabs(ray.invDir.z) > 0.0001f) {
	    tMin = fmax(tMin, axis_t_min.z); tMax = fmin(tMax, axis_t_max.z);
	}

	return tMin <= tMax;
}

float3 compute_normal(global Vertex* vertex_buffer,
		      global int* index_buffer,
		      int id,
		      float2 uv)
{
	global Vertex* vx0 = &vertex_buffer[index_buffer[3*id]];
	global Vertex* vx1 = &vertex_buffer[index_buffer[3*id+1]];
	global Vertex* vx2 = &vertex_buffer[index_buffer[3*id+2]];

	float3 n0 = normalize(vx0->normal);
	float3 n1 = normalize(vx1->normal);
	float3 n2 = normalize(vx2->normal);

	float u = uv.s0;
	float v = uv.s1;
	float w = 1.f - (u+v);

	return w * n0 + v * n2 + u * n1;
}

RayHitInfo 
triangle_hit(global Vertex* vertex_buffer,
	     global int* index_buffer,
	     int triangle,
	     Ray ray){

	RayHitInfo info;
	info.hit = false;
	info.shadow_hit = false;
	info.inverse_n = false;

	float3 p = ray.ori.xyz;
 	float3 d = ray.dir.xyz;

	global Vertex* vx0 = &vertex_buffer[index_buffer[3*triangle]];
	global Vertex* vx1 = &vertex_buffer[index_buffer[3*triangle+1]];
	global Vertex* vx2 = &vertex_buffer[index_buffer[3*triangle+2]];

	float3 v0 = vx0->position;
	float3 v1 = vx1->position;
	float3 v2 = vx2->position;
	
	float3 e1 = v1 - v0;
	float3 e2 = v2 - v0;
	
	float3 h = cross(d, e2);
	float  a = dot(e1,h);
	
	if (a > -0.00001f && a < 0.00001f)
		return info;

	float  f = 1.f/a;
	float3 s = p - v0;
	float  u = f * dot(s,h);
	if (u < 0.f || u > 1.f) 
		return info;

	float3 q = cross(s,e1);
	float  v = f * dot(d,q);
	if (v < 0.f || u+v > 1.f)
		return info;

	info.t = f * dot(e2,q);
	bool t_is_within_bounds = (info.t < ray.tMax && info.t > ray.tMin);

	if (t_is_within_bounds) {
		info.hit = true;
		info.id = triangle;
		info.uv.s0 = u;
		info.uv.s1 = v;
	}
	return info;
}

RayHitInfo 
leaf_hit(BVHNode node,
	 global Vertex* vertex_buffer,
	 global int* index_buffer,
	 Ray ray){

	RayHitInfo best_hit;
	best_hit.hit = false;
	best_hit.t = ray.tMax;

	for (int i = node.start_index; i < node.end_index; ++i) {
		int triangle = i;
		RayHitInfo tri_hit = triangle_hit(vertex_buffer,
						index_buffer,
						triangle,
						ray);

		merge_hit_info(&best_hit, &tri_hit);

		if (best_hit.hit)
			ray.tMax = best_hit.t;
	}
	return best_hit;
}

RayHitInfo trace_ray(Ray ray,
		     global Vertex* vertex_buffer,
		     global int* index_buffer,
		     global BVHNode* bvh_nodes)
{
	RayHitInfo best_hit_info;
	best_hit_info.hit = false;
	best_hit_info.shadow_hit = false;
	best_hit_info.inverse_n = false;

	bool going_up = false;

	unsigned int last = 0;
	unsigned int curr = 0;
	
	best_hit_info.t = ray.tMax;

	BVHNode current_node;
	BBox test_bbox;

	unsigned int first_child, second_child;
	bool childrenOrder = true;

	float3 rdir = ray.dir;
	float3 adir = fabs(rdir);
	float max_dir_val = fmax(adir.x, fmax(adir.y,adir.z)) - 0.00001f;
	adir = fdim(adir, (float3)(max_dir_val,max_dir_val,max_dir_val));

	while (true) {
		current_node = bvh_nodes[curr];

		/* Compute node children traversal order */
		float3 lbbox_vals = bvh_nodes[current_node.l_child].bbox.lo;
		float3 rbbox_vals = bvh_nodes[current_node.r_child].bbox.lo;
		float3 choice_vec = dot(rdir,rbbox_vals - lbbox_vals);
		if (adir.x > 0.f) {
			childrenOrder = choice_vec.x > 0.f;
		} else if (adir.y > 0.f) {
			childrenOrder = choice_vec.y > 0.f;
		} else {
			childrenOrder = choice_vec.z > 0.f;
		}
		
		if (childrenOrder) {
			first_child = current_node.l_child;
			second_child = current_node.r_child;
		} else {
			first_child = current_node.r_child;
			second_child = current_node.l_child;
		}
		

		if (going_up) {
			// I'm going up from the root, so break
			if (last == 0) {
				break;

			// I'm going up from my first child, do the second one
			} else if (last == first_child) {
				last = curr;
				curr = second_child;
				going_up = false;
				continue;

			// I'm going up from my second child, go up one more level
			} else if (last == second_child) {
				last = curr;
				curr = current_node.parent;
				going_up = true;
				continue;
			}
		}
			
		test_bbox = current_node.bbox;
		bool hit = bbox_hit(test_bbox, ray);

		// If it hit, and closer to the closest hit up to now, check it
		if (hit) {
			// If it's a leaf, check all primitives in the leaf, then go up
			if (current_node.leaf) {
 				// Check all primitives in leaf
				RayHitInfo leaf_info = leaf_hit(current_node,
							      vertex_buffer,
							      index_buffer,
							      ray);
				merge_hit_info(&best_hit_info, &leaf_info);
				if (best_hit_info.hit)
					ray.tMax = best_hit_info.t;
				last = curr;
				curr = current_node.parent;
				going_up = true;
				continue;

			// If it hit and it isn't a leaf, go to the first child
			} else {
				last = curr;
				curr = first_child;
				going_up = false;
				continue;
			}
		
                // If it didn't hit, go up
		} else {
			last = curr;
			curr = current_node.parent;
			going_up = true;
			continue;
		}
	}
	return best_hit_info;
}


kernel void 
trace(global RayHitInfo* trace_info,
      global RayPlus* rays,
      global Vertex* vertex_buffer,
      global int* index_buffer,
      global BVHNode* bvh_nodes)
      
{
	int index = get_global_id(0);
	int offset = get_global_offset(0);
	Ray ray = rays[index].ray;

	RayHitInfo hit_info = trace_ray(ray, vertex_buffer, index_buffer, bvh_nodes);

	/* Compute normal at hit point */
	if (hit_info.hit) {
		hit_info.n = compute_normal(vertex_buffer, index_buffer, 
					    hit_info.id, hit_info.uv);
		/* If the normal is pointing out, invert it and note it in the flags */
		if (dot(hit_info.n,ray.dir) > 0) {
			hit_info.inverse_n = true;
			hit_info.n *= -1.f;
		}
	}

	/* Save hit info*/
	trace_info[index] = hit_info;
}
