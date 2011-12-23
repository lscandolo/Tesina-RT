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
	float t;
	unsigned int id;
	float2 uv;
 
} hit_info;

typedef struct {

	float3 n;
	float3 r;
	float  cosL;
	float  spec;

} reflect_info;

void __attribute__((always_inline))
merge_hit_info(hit_info* best_info, hit_info* new_info){
	if ((*new_info).hit) {
		(*best_info).hit = true;
		if ((*new_info).t < (*best_info).t){
			(*best_info).t = (*new_info).t;
			(*best_info).id = (*new_info).id;
			(*best_info).uv = (*new_info).uv;
		}
	}
}

hit_info __attribute__((always_inline))
bbox_hit(BBox bbox,
	 Ray ray)
{
	hit_info info;
	info.hit = false;
	float tMin = ray.tMin;
	float tMax = ray.tMax;

	float3 axis_t_lo, axis_t_hi;

	axis_t_lo = (bbox.lo - ray.ori) * ray.invDir;
	axis_t_hi = (bbox.hi - ray.ori) * ray.invDir;

	float3 axis_t_max = max(axis_t_lo, axis_t_hi);
	float3 axis_t_min = min(axis_t_lo, axis_t_hi);

	if (fabs(ray.invDir.x) > 0.0001f) {
		tMin = max(tMin, axis_t_min.x); tMax = min(tMax, axis_t_max.x); 
	}
	if (fabs(ray.invDir).y > 0.0001f) {
		tMin = max(tMin, axis_t_min.y); tMax = min(tMax, axis_t_max.y);
	}
	if (fabs(ray.invDir.z) > 0.0001f) {
	    tMin = max(tMin, axis_t_min.z); tMax = min(tMax, axis_t_max.z);
	}

	if (tMin < tMax && tMin > ray.tMin && tMin < ray.tMax) {
		info.hit = true;
		info.t   = tMin;
	}

	return info;
}

hit_info 
triangle_hit(global Vertex* vertex_buffer,
	     global int* index_buffer,
	     int triangle,
	     Ray ray){

	hit_info info;
	info.hit = false;

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
	
	if (a > -0.000001f && a < 0.00001f)
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
		info.uv = (float2)(u,v);
	}
	return info;
}

hit_info 
leaf_hit(BVHNode node,
	 global Vertex* vertex_buffer,
	 global int* index_buffer,
	 global tri_id* ordered_triangles,
	 Ray ray){

	hit_info best_hit;
	best_hit.hit = false;
	best_hit.t = ray.tMax;

	for (int i = node.start_index; i < node.end_index; ++i) {
		int triangle = ordered_triangles[i];
		hit_info tri_hit = triangle_hit(vertex_buffer,
						index_buffer,
						triangle,
						ray);

		merge_hit_info(&best_hit, &tri_hit);

		if (best_hit.hit)
			ray.tMax = best_hit.t;
	}
	return best_hit;

}

reflect_info 
triangle_reflect(global Vertex* vertex_buffer,
		 global int* index_buffer,
		 float3 L,
		 hit_info info,
		 Ray ray){

	reflect_info ref;

	global Vertex* vx0 = &vertex_buffer[index_buffer[3*info.id]];
	global Vertex* vx1 = &vertex_buffer[index_buffer[3*info.id+1]];
	global Vertex* vx2 = &vertex_buffer[index_buffer[3*info.id+2]];

	float3 n0 = normalize(vx0->normal);
	float3 n1 = normalize(vx1->normal);
	float3 n2 = normalize(vx2->normal);

	float3 d = ray.dir.xyz;

	float u = info.uv.s0;
 	float v = info.uv.s1;
	float w = 1.f - (u+v);
	
	ref.n = (w * n0 + v * n2 + u * n1);
	ref.r = d - 2.f * ref.n * (dot(d,ref.n));

	ref.cosL = clamp(dot(ref.n,-L),0.f,1.f);
	ref.spec = clamp(dot(ref.r,-L),0.f,1.f);
	ref.spec = pow(ref.spec,8);
		
	return ref;
}

kernel void 
trace(write_only image2d_t img,
      global Ray* ray_buffer,
      global Vertex* vertex_buffer,
      global int* index_buffer,
      global BVHNode* bvh_nodes,
      global tri_id* ordered_triangles,
      read_only int div)

{
	int x = get_global_id(0);
	int y = get_global_id(1);
	int width = get_global_size(0)-1;
	int height = get_global_size(1)-1;

	int group_x = get_local_size(0);
	int group_y = get_local_size(1);

	int index = x + y * (width+1);

	private Ray ray = ray_buffer[index];

	bool ray_hit = false;
	float ray_t = ray.tMax;

	bool going_up = false;

	unsigned int last = 0;
	unsigned int curr = 0;
	hit_info best_hit_info;
	best_hit_info.hit = false;
	best_hit_info.t = ray.tMax;

	private BVHNode current_node;
	private BBox test_bbox;

	bool red_flag = false;
	

	unsigned int first_child, second_child;
	bool childrenOrder = true;

	float3 rdir = ray.dir;
	float3 adir = fabs(rdir);
	float max_dir_val = max(adir.x, max(adir.y,adir.z)) - 0.00001f;
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

				// I'm going up from my left child, do the right one
			} else if (last == first_child) {
				last = curr;
				curr = second_child;
				going_up = false;
				continue;

			// I'm going up from my right child, go up one more level
			} else if (last == second_child) {
				last = curr;
				curr = current_node.parent;
				going_up = true;
				continue;
			}

			/* This means the node structure is broken */
			else {
				red_flag = true;
				break;
			}
				
		}
			
		test_bbox = current_node.bbox;
		hit_info info = bbox_hit(test_bbox, ray);

		// If it hit, and closer to the closest hit up to now, check it
		if (info.hit && info.t < ray.tMax && info.t > ray.tMin) {
			// If it's a leaf, check all primitives in the leaf, then go up
			if (current_node.leaf) {
 				// Check all primitives in leaf
				/* test_nodes[test_idx++] = curr; */
				hit_info leaf_info = leaf_hit(current_node,
							      vertex_buffer,
							      index_buffer,
							      ordered_triangles,
							      ray);
				merge_hit_info(&best_hit_info, &leaf_info);
				if (best_hit_info.hit)
					ray.tMax = best_hit_info.t;
				last = curr;
				curr = current_node.parent;
				going_up = true;
				continue;

			// If it hit and it isn't a leaf, go to the left child
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

	ray_hit = best_hit_info.hit;
	ray_t = best_hit_info.t;

	/* Image writing computations */
	int2 image_size = (int2)(get_image_width(img), get_image_height(img));

	float x_part = (float)x/(float)width; 
	float y_part = (float)y/(float)height; 

	int2 coords = (int2)( ( (image_size.x-1) * x ) / width,
			      ( (image_size.y-1) * y ) / height);


	/* Value for the pixel corresponding to this ray */
	float3 L = normalize((float3)(0.1f  * (div-8.f) + 0.1f,
				      0.1f  * (div-8.f),
				      0.2f));
	float4 valf;

	/* If it hit compute the reflection information */
	if (ray_hit) {
		reflect_info ref;
		ref = triangle_reflect(vertex_buffer,
				       index_buffer,
				       L,
				       best_hit_info,
				       ray);

		valf = (float4)(ray_t * 0.001f * ref.cosL,
				ray_t * 0.01f * ref.cosL + 1.0f *ref.spec,
				ray_t * 0.1f * ref.cosL + 1.0f *ref.spec,1.0f);

	/* Else just paint it black */
	} else {
		valf = (float4)(0.f,
				0.f,
				0.f,
				1.0f);
	}

	if (red_flag)
		valf = (float4)(1.f,
				0.f,
				0.f,
				1.0f);

	write_imagef(img, coords, valf);

}
