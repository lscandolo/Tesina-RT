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
        union {
                unsigned int l_child;
                unsigned int start_index;
        };
        union {
                unsigned int r_child;
                unsigned int end_index;
        };
	unsigned int parent;
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


float3 compute_normal(global Vertex* vertex_buffer,
                      global int* index_buffer,
                      int id,
                      float2 uv);
float2 compute_texCoord(global Vertex* vertex_buffer,
                        global int* index_buffer,
                        int id,
                        float2 uv);

float3 __attribute__((always_inline)) 
multiply_vector(float3 v, sqmat4 M)
{
        float4 x = (float4)(v,0.f);
        float4 r;

        r.x = dot(M.row[0],x);
        r.y = dot(M.row[1],x);
        r.z = dot(M.row[2],x);
        r.w = dot(M.row[3],x);

        if (fabs(r.w) > 1e-26f)
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

        if (fabs(r.w) > 1e-26f)
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

void __attribute__((always_inline))
transform_hit_info(Ray ray, 
                   Ray tr_ray, 
                   RayHitInfo* hit_info, 
                   sqmat4 tr,
                   global Vertex* vertex_buffer,
                   global int* index_buffer)
{

        if (hit_info->hit) {
                hit_info->hit_point = tr_ray.ori + tr_ray.dir * hit_info->t;
                hit_info->hit_point = multiply_point(hit_info->hit_point, tr);
                hit_info->t = distance(hit_info->hit_point, ray.ori); 
        }
     
}

void __attribute__((always_inline))
complete_transformed_hit_info(Ray ray, 
                              sqmat4 tr,
                              RayHitInfo* hit_info, 
                              global Vertex* vertex_buffer,
                              global int* index_buffer)
{

               
        hit_info->n = compute_normal(vertex_buffer, 
                                     index_buffer,
                                     hit_info->id, 
                                     hit_info->uv);
        hit_info->n = multiply_vector(hit_info->n, tr);
        /* If the normal is pointing out, 
           invert it and note it in the flags */
        if (dot(hit_info->n,ray.dir) > 0) { 
                hit_info->inverse_n = true;
                hit_info->n *= -1.f;
        }

        hit_info->uv = compute_texCoord(vertex_buffer, 
                                        index_buffer,
                                        hit_info->id, 
                                        hit_info->uv);
}

void __attribute__((always_inline))
complete_hit_info(Ray ray, 
                  RayHitInfo* hit_info, 
                  global Vertex* vertex_buffer,
                  global int* index_buffer)
{

        hit_info->hit_point = ray.ori + ray.dir * hit_info->t;

        int id = 3 * hit_info->id;

        global Vertex* vx0 = &vertex_buffer[index_buffer[id]];
        global Vertex* vx1 = &vertex_buffer[index_buffer[id+1]];
        global Vertex* vx2 = &vertex_buffer[index_buffer[id+2]];

        float u = hit_info->uv.s0;
        float v = hit_info->uv.s1;
        float w = 1.f - (u+v);

        float3 n0 = normalize(vx0->normal);
        float3 n1 = normalize(vx1->normal);
        float3 n2 = normalize(vx2->normal);

        float2 t0 = vx0->texCoord;
        float2 t1 = vx1->texCoord;
        float2 t2 = vx2->texCoord;

        hit_info->n = normalize(w * n0 + v * n2 + u * n1);
        hit_info->uv = w * t0 + v * t2 + u * t1;
        
        /* If the normal is pointing out, 
           invert it and note it in the flags */
        if (dot(hit_info->n,ray.dir) > 0) { 
                hit_info->inverse_n = true;
                hit_info->n *= -1.f;
        }

}

void __attribute__((always_inline))
merge_hit_info(RayHitInfo* best_info, RayHitInfo* new_info){
        if (!best_info->hit ||
            (new_info->hit && new_info->t < best_info->t)) {
                    //best_info->t  = new_info->t;
                    //best_info->id = new_info->id;
                    //best_info->uv = new_info->uv;
                    //best_info->hit_point = new_info->hit_point;
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

        if (fabs(ray.invDir.x) > 1e-6f) {
                tMin = fmax(tMin, axis_t_min.x); tMax = fmin(tMax, axis_t_max.x);
        }
        if (fabs(ray.invDir.y) > 1e-6f) {
                tMin = fmax(tMin, axis_t_min.y); tMax = fmin(tMax, axis_t_max.y);
        }
        if (fabs(ray.invDir.z) > 1e-6f) {
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

float2 compute_texCoord(global Vertex* vertex_buffer,
                        global int* index_buffer,
                        int id,
                        float2 uv)
{
        global Vertex* vx0 = &vertex_buffer[index_buffer[3*id]];
        global Vertex* vx1 = &vertex_buffer[index_buffer[3*id+1]];
        global Vertex* vx2 = &vertex_buffer[index_buffer[3*id+2]];

        float2 n0 = vx0->texCoord;
        float2 n1 = vx1->texCoord;
        float2 n2 = vx2->texCoord;

        float u = uv.s0;
        float v = uv.s1;
        float w = 1.f - (u+v);

        return w * n0 + v * n2 + u * n1;
}

/* RayHitInfo  */
/* triangle_hit(global Vertex* vertex_buffer, */
/*              global int* index_buffer, */
/*              int triangle, */
/*              Ray ray){ */

/*         RayHitInfo info; */
/*         info.hit = false; */
/*         info.shadow_hit = false; */
/*         info.inverse_n = false; */

/*         float3 p = ray.ori.xyz; */
/*         float3 d = ray.dir.xyz; */

/*         global Vertex* vx0 = &vertex_buffer[index_buffer[3*triangle]]; */
/*         global Vertex* vx1 = &vertex_buffer[index_buffer[3*triangle+1]]; */
/*         global Vertex* vx2 = &vertex_buffer[index_buffer[3*triangle+2]]; */

/*         float3 v0 = vx0->position; */
/*         float3 v1 = vx1->position; */
/*         float3 v2 = vx2->position; */
        
/*         float3 e1 = v1 - v0; */
/*         float3 e2 = v2 - v0; */
        
/*         float3 h = cross(d, e2); */
/*         float  a = dot(e1,h); */
        
/*         if (a > -1e-26f && a < 1e-26f) */
/* 	/\* if (a > -0.000001f && a < 0.00001f) *\/ */
/*                 return info; */

/*         float  f = 1.f/a; */
/*         float3 s = p - v0; */
/*         float  u = f * dot(s,h); */
/*         if (u < 0.f || u > 1.f)  */
/*                 return info; */

/*         float3 q = cross(s,e1); */
/*         float  v = f * dot(d,q); */
/*         if (v < 0.f || u+v > 1.f) */
/*                 return info; */

/*         info.t = f * dot(e2,q); */
/*         bool t_is_within_bounds = (info.t < ray.tMax && info.t > ray.tMin); */

/*         if (t_is_within_bounds) { */
/*                 info.hit = true; */
/*                 info.id = triangle; */
/*                 info.uv.s0 = u; */
/*                 info.uv.s1 = v; */
/*         } */
/*         return info; */
/* } */

RayHitInfo 
leaf_hit(BVHNode node,
         global Vertex* vertex_buffer,
         global int* index_buffer,
         Ray ray){

        RayHitInfo info;
        info.hit = false;
        info.t = ray.tMax;

        for (int i = node.start_index; i < node.end_index; ++i) {
                int triangle = i;

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
                        continue;
                
                float  f = 1.f/a;
                float3 s = p - v0;
                float  u = f * dot(s,h);
                if (u < 0.f || u > 1.f) 
                        continue;
                
                float3 q = cross(s,e1);
                float  v = f * dot(d,q);
                if (v < 0.f || u+v > 1.f)
                        continue;
                
                float t = f * dot(e2,q);
                bool t_is_within_bounds = (t <= info.t && t >= ray.tMin);

                if (t_is_within_bounds) {
                        info.t = t;
                        info.hit = true;
                        info.id = triangle;
                        info.uv.s0 = u;
                        info.uv.s1 = v;
                }
        }

        if (info.hit)
                ray.tMax = info.t;

        return info;
}

RayHitInfo trace_ray(Ray ray,
                     global Vertex* vertex_buffer,
                     global int* index_buffer,
                     global BVHNode* bvh_nodes,
                     int bvh_root)
{
        RayHitInfo best_hit_info;
        best_hit_info.hit = false;
        best_hit_info.shadow_hit = false;
        best_hit_info.inverse_n = false;
        best_hit_info.t = ray.tMax;

        bool going_up = false;

        unsigned int last = bvh_root;
        unsigned int curr = bvh_root;
        
        BVHNode current_node;

        unsigned int first_child, second_child;
        bool childrenOrder = true;

        float3 rdir = ray.dir;
        float3 adir = fabs(rdir);
        float max_dir_val = fmax(adir.x, fmax(adir.y,adir.z)) - 0.00001f;
        adir = fdim(adir, (float3)(max_dir_val,max_dir_val,max_dir_val));

        /* int depth = 0; */
        /* int maxdepth = 0; */
        while (true) {
                /* maxdepth = max(depth,maxdepth); */
                current_node = bvh_nodes[curr];

                /* Compute node children traversal order if its an interior node*/
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
                                break;

                                // I'm going up from my first child, do the second one
                        } else if (last == first_child) {
                                last = curr;
                                curr = second_child;
                                going_up = false;
                                /* depth++; */

                        // I'm going up from my second child, go up one more level
                        } else /*if (last == second_child)*/ {
                                last = curr;
                                curr = current_node.parent;
                                going_up = true;
                                /* depth--; */
                        }
                } else {
                        // If it hit, and closer to the closest hit up to now, check it
                        if (bbox_hit(current_node.bbox, ray)) {
                                // If it's a leaf, check all primitives in the leaf,
                                // then go up
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

                                        // If it hit and it isn't a leaf,
                                        // go to the first child
                                } else {
                                        last = curr;
                                        curr = first_child;
                                        going_up = false;
                                        /* depth++; */
                                }
                
                                // If it didn't hit, go up
                        } else {
                                last = curr;
                                curr = current_node.parent;
                                going_up = true;
                                /* depth--; */
                        }
                }
        }
        /* best_hit_info.n.s0 = maxdepth; */
        return best_hit_info;
}


kernel void 
trace_multi(global RayHitInfo* trace_info,
            global RayPlus* rays,
            global Vertex* vertex_buffer,
            global int* index_buffer,
            global BVHNode* bvh_nodes,
            global BVHRoot* roots,
            int root_count)
{
        int index = get_global_id(0);
        Ray ray = rays[index].ray;

        RayHitInfo best_info;
        best_info.hit = false;
        int best_root = 0;

        for (int i = 0; i < root_count; ++i) {

                Ray tr_ray = transform_ray(ray, roots[i].trInv);
                RayHitInfo root_info = trace_ray(tr_ray,vertex_buffer,index_buffer,
                                                 bvh_nodes, roots[i].node);
                /*Compute real t and hit point to compare which hit is closest*/
                transform_hit_info(ray,
                                   tr_ray,
                                   &root_info,
                                   roots[i].tr,
                                   vertex_buffer,
                                   index_buffer);

                if (!best_info.hit ||
                    (root_info.hit && root_info.t < best_info.t)) {
                        best_info = root_info;
                        best_root = i;
                }
        }

        /*Compute normal and texCoord at hit point*/
        if (best_info.hit)
                complete_transformed_hit_info(ray, roots[best_root].tr,&best_info,
                                              vertex_buffer, index_buffer);
        /* Save hit info*/
        trace_info[index] = best_info;
}

kernel void 
trace_single(global RayHitInfo* trace_info,
             global RayPlus* rays,
             global Vertex* vertex_buffer,
             global int* index_buffer,
             global BVHNode* bvh_nodes,
             global BVHRoot* roots,
             int root_count)
{
        int index = get_global_id(0);
        Ray ray = rays[index].ray;
        RayHitInfo best_info;

        best_info = trace_ray(ray,vertex_buffer,index_buffer,
                              bvh_nodes, 0);

        if (best_info.hit) 
                complete_hit_info(ray,&best_info, vertex_buffer, index_buffer);


        trace_info[index] = best_info;
}
