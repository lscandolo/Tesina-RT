typedef struct 
{
        float4 row[4];
} sqmat4;

typedef struct {
        int node;
        sqmat4 tr;
        sqmat4 trInv;
} BVHRoot;

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
        float3 hit_point;
  
} RayHitInfo;


typedef struct {

	float3 n;
	float3 r;
	float  cosL;
	float  spec;
	bool   inv_n;

} RayReflectInfo;

typedef float3 Color;

typedef struct {

	float3 dir;
	Color  color;
} DirectionalLight;

typedef struct {
	
	Color ambient;
	DirectionalLight directional;

} Lights;

float3 __attribute__((always_inline)) 
multiply_vector(float3 v, sqmat4 M)
{
        float4 x = (float4)(v,0.f);
        float4 r;

        r.x = dot(M.row[0],x);
        r.y = dot(M.row[1],x);
        r.z = dot(M.row[2],x);
        r.w = dot(M.row[3],x);

        if (fabs(r.w) > 0.00001f)
                r = r / r.w;
        return r.xyz;
}

float3 __attribute__((always_inline)) 
multiply_point(float3 v, sqmat4 M)
{
        float4 x = (float4)(v,1.f);
        float4 r;

        r.x = dot(M.row[0],x);
        r.y = dot(M.row[1],x);
        r.z = dot(M.row[2],x);
        r.w = dot(M.row[3],x);

        if (fabs(r.w) > 0.00001f)
                r = r / r.w;
        return r.xyz;
}

Ray transform_ray(Ray ray, sqmat4 tr)
{
        ray.dir = multiply_vector(ray.dir, tr);
        ray.ori = multiply_point(ray.ori, tr);
        ray.invDir = 1.f / ray.dir;
        return ray;
}

RayHitInfo transform_hit_info(RayHitInfo hit_info, sqmat4 tr)
{
        hit_info.n = multiply_vector(hit_info.n, tr);
        return hit_info;
}

bool __attribute__((always_inline))
bbox_hit(BBox bbox,
	 Ray ray)
{
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
	if (fabs(ray.invDir.y) > 0.0001f) {
		tMin = max(tMin, axis_t_min.y); tMax = min(tMax, axis_t_max.y);
	}
	if (fabs(ray.invDir.z) > 0.0001f) {
	    tMin = max(tMin, axis_t_min.z); tMax = min(tMax, axis_t_max.z);
	}

	return (tMin <= tMax);
}

bool 
triangle_hit(global Vertex* vertex_buffer,
	     global int* index_buffer,
	     int triangle,
	     Ray ray){

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
	
        if (a > -1e-26f && a < 1e-26f)
	/* if (a > -0.000001f && a < 0.00001f) */
		return false;

	float  f = 1.f/a;
	float3 s = p - v0;
	float  u = f * dot(s,h);
	if (u < 0.f || u > 1.f) 
		return false;

	float3 q = cross(s,e1);
	float  v = f * dot(d,q);
	if (v < 0.f || u+v > 1.f)
		return false;

	float t = f * dot(e2,q);
	return (t < ray.tMax && t > ray.tMin);
}

bool 
leaf_hit_any(BVHNode node,
	     global Vertex* vertex_buffer,
	     global int* index_buffer,
	     Ray ray){

	for (int i = node.start_index; i < node.end_index; ++i) {
		int triangle = i;
		if (triangle_hit(vertex_buffer,index_buffer,triangle,ray))
			return true;
	}
	return false;

}

RayReflectInfo 
triangle_reflect(global Vertex* vertex_buffer,
		 global int* index_buffer,
		 float3 L,
		 RayHitInfo info,
		 Ray ray){

	RayReflectInfo ref;

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

	/* If the normal points out, we need to invert it (and day we did) */
	if (dot(ref.n,d) > 0) { 
		ref.inv_n = true;
		ref.n = -ref.n;
	} else {
		ref.inv_n = false;
	}
	

	ref.r = d - 2.f * ref.n * (dot(d,ref.n));

	ref.cosL = clamp(dot(ref.n,-L),0.f,1.f);
	ref.cosL = pow(ref.cosL,2);

	ref.spec = clamp(dot(ref.r,-L),0.f,1.f);
	ref.spec = pow(ref.spec,8);
	ref.spec *= ref.cosL;
		
	return ref;
}

bool trace_shadow_ray(Ray ray,
		      global Vertex* vertex_buffer,
		      global int* index_buffer,
		      global BVHNode* bvh_nodes,
                      int bvh_root)
{
	bool going_up = false;
	unsigned int last = bvh_root;
	unsigned int curr = bvh_root;

	private BVHNode current_node;

	unsigned int first_child, second_child;
	bool childrenOrder = true;

	float3 rdir = ray.dir;
	float3 adir = fabs(rdir);
	float max_dir_val = max(adir.x, max(adir.y,adir.z)) - 0.00001f;
	adir = fdim(adir, (float3)(max_dir_val,max_dir_val,max_dir_val));

	while (true) { 
		current_node = bvh_nodes[curr];

		/* Compute node children traversal order if it's not a leaf*/
		if (!current_node.leaf) {
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
		}
		

		if (going_up) {
			// I'm going up from the root, so break
			if (last == bvh_root) {
				return false;
				
                                // I'm going up from my first child, do the right one
			} else if (last == first_child) {
				last = curr;
				curr = second_child;
				going_up = false;

                                // I'm going up from my second child, go up one more level
			} else /* if (last == second_child) */ {
				last = curr;
				curr = current_node.parent;
				going_up = true;
			}

		}
		else {	
                        // If it hit, and closer to the closest hit up to now, check it
                        if (bbox_hit(current_node.bbox,ray)) {
                                // If it's a leaf, check all primitives in the leaf, 
                                // then go up
                                if (current_node.leaf) {
                                        // Check all primitives in leaf
                                        if(leaf_hit_any(current_node,
                                                        vertex_buffer,
                                                        index_buffer,
                                                        ray)) 
                                                return true;

                                        last = curr;
                                        curr = current_node.parent;
                                        going_up = true;
                                        // If it hit and it isn't a leaf, 
                                        //go to the first child
                                } else {
                                        last = curr;
                                        curr = first_child;
                                        going_up = false;
                                }
		
                                // If it didn't hit, go up
                        } else {
                                last = curr;
                                curr = current_node.parent;
                                going_up = true;
                        }
                }
        }
	return false;
}


kernel void 
shadow_trace_multi(global RayHitInfo* trace_info,
                   global RayPlus* rays,
                   global Vertex* vertex_buffer,
                   global int* index_buffer,
                   int    root_count,
                   global BVHRoot* roots,
                   global BVHNode* bvh_nodes,
                   global Lights* lights)
{
	int index = get_global_id(0);

	/* Ray original_ray = rays[index].ray; */

	RayHitInfo info  = trace_info[index];

	if (!info.hit){
		return;
	}

	Ray ray;
	ray.dir = -lights->directional.dir;
	ray.invDir = 1.f/ray.dir;
        ray.ori = info.hit_point;
	/* ray.ori = original_ray.ori + original_ray.dir * info.t; */
  	ray.tMin = 0.0001f; ray.tMax = 1e37f;

        trace_info[index].shadow_hit = false;
        for (int i = 0; i < root_count; ++i) {

                Ray tr_ray = transform_ray(ray, roots[i].trInv);
                bool hit = trace_shadow_ray(tr_ray, 
                                            vertex_buffer, 
                                            index_buffer, 
                                            bvh_nodes,
                                            roots[i].node);

                if (hit) {
                        trace_info[index].shadow_hit = true;
                        return;
                }
        }
        return;
}

kernel void 
shadow_trace_single(global RayHitInfo* trace_info,
                    global RayPlus* rays,
                    global Vertex* vertex_buffer,
                    global int* index_buffer,
                    int    root_count,
                    global BVHRoot* roots,
                    global BVHNode* bvh_nodes,
                    global Lights* lights)
{
	int index = get_global_id(0);

	/* Ray original_ray = rays[index].ray; */

	RayHitInfo info  = trace_info[index];

	if (!info.hit){
		return;
	}

	Ray ray;
	ray.dir = -lights->directional.dir;
	ray.invDir = 1.f/ray.dir;
        ray.ori = info.hit_point;
	/* ray.ori = original_ray.ori + original_ray.dir * info.t; */
  	ray.tMin = 0.0001f; ray.tMax = 1e37f;

        trace_info[index].shadow_hit = trace_shadow_ray(ray, 
                                                        vertex_buffer, 
                                                        index_buffer, 
                                                        bvh_nodes,
                                                        0);
        return;
}
