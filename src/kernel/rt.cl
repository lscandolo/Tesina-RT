typedef struct vec3_t
{
	float v[3];
} vec3;

typedef struct 
{
	float ori[3];
	float dir[3];
	float invDir[3];
	float tMin;
	float tMax;
} Ray;

typedef struct 
{
        float position[3];
        float texCoord[2];
        float normal[3];
        float tangent[4];
        float bitangent[3];
} Vertex;


typedef unsigned int tri_id;

typedef struct {
	float hi[3];
	float lo[3];
	float centroid[3];
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

} hit_info;

hit_info __attribute__((always_inline))
bbox_hit(BBox bbox,
	 Ray ray)
{
	hit_info info;
	info.hit = false;
	float tMin = ray.tMin;
	float tMax = ray.tMax;

	float axis_t_min;
	float axis_t_max;

	for (int i = 0; i < 3; ++i) {
		if (ray.dir[i] > -0.00001f && ray.dir[i] < 0.00001f)
			continue;
		float inv_dir = ray.invDir[i];
		if (ray.dir[i] > 0) {
			axis_t_min = (bbox.lo[i] - ray.ori[i]) * inv_dir;
			axis_t_max = (bbox.hi[i] - ray.ori[i]) * inv_dir;
		} else {
			axis_t_max = (bbox.lo[i] - ray.ori[i]) * inv_dir;
			axis_t_min = (bbox.hi[i] - ray.ori[i]) * inv_dir;
		}
		
		tMin = max(tMin, axis_t_min);
		tMax = min(tMax, axis_t_max);

	}
	if (tMin < tMax) {
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

	float3 p = (float3)(ray.ori[0], ray.ori[1], ray.ori[2]);
	float3 d = (float3)(ray.dir[0], ray.dir[1], ray.dir[2]);

	global Vertex* vx0 = &vertex_buffer[index_buffer[3*triangle]];
	global Vertex* vx1 = &vertex_buffer[index_buffer[3*triangle+1]];
	global Vertex* vx2 = &vertex_buffer[index_buffer[3*triangle+2]];

	float3 v0 = (float3)(vx0->position[0], vx0->position[1], vx0->position[2]);
	float3 v1 = (float3)(vx1->position[0], vx1->position[1], vx1->position[2]);
	float3 v2 = (float3)(vx2->position[0], vx2->position[1], vx2->position[2]);
	
	float3 e1 = v1 - v0;
	float3 e2 = v2 - v0;
	
	float3 h = cross(d, e2);
	float a = dot(e1,h);
	
	if (a > -0.000001f && a < 0.00001f)
		return info;

	float f = 1.f/a;
	float3 s = p - v0;
	float u = f * dot(s,h);
	if (u < 0.f || u > 1.f) 
		return info;

	float3 q = cross(s,e1);
	float v = f * dot(d,q);
	if (v < 0.f || u+v > 1.f)
		return info;

	float w = 1.f-(u+v);
	info.t = f * dot(e2,q);
	bool t_is_within_bounds = (info.t < ray.tMax && info.t > ray.tMin);

	if (t_is_within_bounds) {
		info.hit = true;
		info.id = triangle;
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
		if (tri_hit.hit) {
			best_hit.hit = true;
			if (tri_hit.t < best_hit.t){
				best_hit.t = tri_hit.t;
				best_hit.id = triangle;
			}
			ray.tMax = best_hit.t;
		}
	}
	return best_hit;

}

typedef struct {

	float3 n;
	float3 r;
	float  cosL;
	float  spec;

} hit_reflect_info;

hit_reflect_info 
triangle_reflect(global Vertex* vertex_buffer,
		 global int* index_buffer,
		 int triangle,
		 Ray ray){

	hit_reflect_info ref;

	float3 L = normalize((float3)(0.577f,0.577f,0.577f));

	float3 p = (float3)(ray.ori[0], ray.ori[1], ray.ori[2]);
	float3 d = (float3)(ray.dir[0], ray.dir[1], ray.dir[2]);

	float3 v1,v2,v0, n0, n1, n2;

	global Vertex* vx0 = &vertex_buffer[index_buffer[3*triangle]];
	global Vertex* vx1 = &vertex_buffer[index_buffer[3*triangle+1]];
	global Vertex* vx2 = &vertex_buffer[index_buffer[3*triangle+2]];

	global float* vert0 = vx0->position;
	global float* vert1 = vx1->position;
	global float* vert2 = vx2->position;

	global float* norm0 = vx0->normal;
	global float* norm1 = vx1->normal;
	global float* norm2 = vx2->normal;

	v0 = (float3)(vert0[0], vert0[1], vert0[2]);
	v1 = (float3)(vert1[0], vert1[1], vert1[2]);
	v2 = (float3)(vert2[0], vert2[1], vert2[2]);

	n0 = normalize((float3)(norm0[0], norm0[1], norm0[2]));
	n1 = normalize((float3)(norm1[0], norm1[1], norm1[2]));
	n2 = normalize((float3)(norm2[0], norm2[1], norm2[2]));

	float3 e1 = v1 - v0;
	float3 e2 = v2 - v0;

	float3 h = cross(d, e2);
	float  a = dot(e1,h);

	float  f = 1.f/a;
	float3 s = p - v0;
	float  u = f * dot(s,h);
	float3 q = cross(s,e1);
	float  v = f * dot(d,q);
	float  w = 1.f-(u+v);

	float t = f * dot(e2,q);

	float3 n = (w * n0 + v * n2 + u * n1);
	float3 r = d - 2.f * n * (dot(d,n));

	ref.n = n;
	ref.r = r;
	ref.cosL = clamp(dot(n,-L),0.f,1.f);
	ref.spec = clamp(dot(r,-L),0.f,1.f);
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

	int ray_index = x + y * (width+1);

	private Ray ray = ray_buffer[ray_index];
	ray.ori[2] -= 10.f + div*5;
	ray.ori[1] += 3.f;

	/* ray.ori[2] -= 15.f; */

	bool ray_hit = false;
	float ray_t = ray.tMax;

	bool going_up = false;

	unsigned int last = 0;
	unsigned int curr = 0;
	hit_info best_hit_info;
	best_hit_info.hit = false;
	best_hit_info.t = ray.tMax;

	private int tries = 0;

	/* private int test_idx = 0; */
	/* private int test_nodes[32]; */

	private BVHNode current_node;
	private BBox test_bbox;

	bool red_flag = false;
	
	while (true) {
		current_node = bvh_nodes[curr];

		if (going_up) {
			// I'm going up from the root, so break
			if (last == 0) {
				break;

				// I'm going up from my left child, do the right one
			} else if (last == current_node.l_child) {
				last = curr;
				curr = current_node.r_child;
				going_up = false;
				continue;

			// I'm going up from my right child, go up one more level
			/* } else { */
			} else if (last == current_node.r_child) {
				last = curr;
				curr = current_node.parent;
				going_up = true;
				continue;
			}

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
				if (leaf_info.hit) {
					best_hit_info.hit = true;
					if (leaf_info.t < best_hit_info.t) {
						best_hit_info.t = leaf_info.t;
						best_hit_info.id = leaf_info.id;
					}
					ray.tMax = best_hit_info.t;
				}
				last = curr;
				curr = current_node.parent;
				going_up = true;
				continue;

			// If it hit and it isn't a leaf, go to the left child
			} else {
				last = curr;
				curr = current_node.l_child;
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

	/* hit_info leaf_info; */
	/* leaf_info.hit = false; */
	/* for (int i = 0; i < test_idx; ++i){ */
	/* 	leaf_info = leaf_hit(bvh_nodes[test_nodes[i]], */
	/* 			     vertex_buffer, */
	/* 			     index_buffer, */
	/* 			     ordered_triangles, */
	/* 			     ray); */
		
	/* 	if (leaf_info.hit) { */
	/* 		best_hit_info.hit = true; */
	/* 		best_hit_info.t = */
	/* 			min(best_hit_info.t, leaf_info.t); */
	/* 		ray.tMax = best_hit_info.t; */
	/* 	} */
	/* } */

	ray_hit = best_hit_info.hit;
	ray_t = best_hit_info.t;

	/* Image writing computations */
	int2 image_size = (int2)(get_image_width(img), get_image_height(img));

	float x_part = (float)x/(float)width; 
	float y_part = (float)y/(float)height; 

	int2 coords = (int2)( ( (image_size.x-1) * x ) / width,
			      ( (image_size.y-1) * y ) / height);


	/* Value for the pixel corresponding to this ray */
	float4 valf;
	if (ray_hit) {
		hit_reflect_info ref;
		ref = triangle_reflect(vertex_buffer,
				       index_buffer,
				       best_hit_info.id,
				       ray);
		valf = (float4)(ray_t * 0.001f * ref.cosL,
				ray_t * 0.01f * ref.cosL + 1.0f *ref.spec,
				ray_t * 0.1f * ref.cosL + 1.0f *ref.spec,1.0f);
		/* valf = (float4)(ray_t * 0.01f, */
		/* 		ray_t * 0.1f, */
		/* 		ray_t * 1.f, */
		/* 		1.0f); */
	} else {
		valf = (float4)(0.f,
				0.f,
				0.f,
				1.0f);
	}

	/* if (test_idx > 1) */
	if (tries > 31)
		valf = (float4)(1.f,
				0.f,
				0.f,
				1.0f);


	if (red_flag)
		valf = (float4)(1.f,
				0.f,
				0.f,
				1.0f);

	write_imagef(img, coords, valf);

}
