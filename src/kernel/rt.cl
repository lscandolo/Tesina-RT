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
	unsigned char split_axis;
	bool   leaf;

} BVHNode;

typedef struct {
	bool hit;
	float t;
} hit_info;

hit_info bbox_hit(BBox bbox,
		  Ray ray)
{
	hit_info info;
	info.hit = false;
	float tMin = ray.tMin;
	float tMax = ray.tMax;

	float axis_t_min;
	float axis_t_max;

	for (int i = 0; i < 3; ++i) {
		if (ray.dir[i] == 0.f)
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

hit_info triangle_hit(global Vertex* vertex_buffer,
		      global int* index_buffer,
		      int triangle,
		      Ray ray){

	hit_info info;

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
	
	float f = 1.f/a;
	float3 s = p - v0;
	float u = f * dot(s,h);
	float3 q = cross(s,e1);
	float v = f * dot(d,q);
	float w = 1.f-(u+v);

	info.t = f * dot(e2,q);
	info.hit = false;
	bool t_is_within_bounds = (info.t < ray.tMax && info.t > ray.tMin);

	if (!(a > -0.000001f && a < 0.00001f) &&
	    !(u < 0.f || u > 1.f) &&
	    !(v < 0.f || u+v > 1.f) &&
	    t_is_within_bounds){
		info.hit = true;
	}

	return info;
}

hit_info leaf_hit(BVHNode node,
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
			best_hit.t = min(best_hit.t, tri_hit.t);
		}
	}
	return best_hit;

}


kernel void trace(write_only image2d_t img,
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

	int ray_index = x + y * (width+1);
	Ray ray = ray_buffer[ray_index];
	ray.ori[2] -= 10.f + div*10;
	/* ray.ori[2] -= 5.f; */

	bool ray_hit = false;
	float ray_t = ray.tMax;

	bool going_up = false;

	unsigned int last = 0;
	unsigned int curr = 0;
	hit_info best_hit_info;
	best_hit_info.hit = false;
	best_hit_info.t = ray.tMax;
	int tries = 0;
	while (true) {
		if (going_up) {
			// I'm going up and reached the top, break
			if (last == 0) {
				break;

			// I'm going up from my left child, do the right one
			}else if (last == bvh_nodes[curr].l_child) {
				last = curr;
				curr = bvh_nodes[curr].r_child;
				going_up = false;
				continue;

			// I'm going up from my right child, go up one more level
			} else {
				last = curr;
				curr = bvh_nodes[curr].parent;
				going_up = true;
				continue;
			}
		}
			
		hit_info info = bbox_hit(bvh_nodes[curr].bbox, ray);
		// If it hit, and closer to the closest hit up to now, check it
		if (info.hit && 
		    (ray.tMax > info.t)) {
			// If it's a leaf, check all primitives in the leaf, then go up
			if (bvh_nodes[curr].leaf) {
				// Check all primitives in leaf
				tries++;
				hit_info leaf_info = leaf_hit(bvh_nodes[curr],
							      vertex_buffer,
							      index_buffer,
							      ordered_triangles,
							      ray);
				if (leaf_info.hit){
					best_hit_info.hit = true;
					best_hit_info.t = 
						min(best_hit_info.t, leaf_info.t);
					ray.tMax = best_hit_info.t;
				}
				last = curr;
				curr = bvh_nodes[curr].parent;
				going_up = true;
				continue;
			// If it hit and it isn't a leaf, go to the left node
			} else {
				last = curr;
				curr = bvh_nodes[curr].l_child;
				going_up = false;
				continue;
			}
			// If it didn't hit, go up
		} else {
				last = curr;
				curr = bvh_nodes[curr].parent;
				going_up = true;
				continue;
		}
	}
	ray_hit = best_hit_info.hit;
	ray_t = best_hit_info.t;

	/* for (int i = 0; i < 333; ++i){ */

	/* 	hit_info info = bbox_hit(bvh_nodes[i].bbox, ray); */
	/* 	if (info.hit){ */
	/* 		ray_hit = true; */
	/* 		ray_t = min(ray_t, info.t); */
	/* 	} */

	/* } */

	/* for (int i = 0; i < 576; ++i){ */

	/* 	hit_info info = triangle_hit(vertex_buffer, index_buffer, i, ray); */
	/* 	if (info.hit){ */
	/* 		ray_hit = true; */
	/* 		ray_t = min(ray_t, info.t); */
	/* 	} */

	/* } */

	int2 image_size = (int2)(get_image_width(img), get_image_height(img));

	float x_part = (float)x/(float)width; 
	float y_part = (float)y/(float)height; 

	int2 coords = (int2)( ( (image_size.x-1) * x ) / width,
			      ( (image_size.y-1) * y ) / height);

	float4 valf;

	if (ray_hit) {
		valf = (float4)(ray_t/1000.f,
				ray_t/100.f,
				ray_t/10.f,
				1.0f);
	} else {
		valf = (float4)(0.f,
				0.f,
				0.f,
				1.0f);
	}

	write_imagef(img, coords, valf);

}
