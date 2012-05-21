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

<<<<<<< Updated upstream
void __attribute__((always_inline))
transform_hit_info(Ray ray, 
                   Ray tr_ray, 
                   RayHitInfo* hit_info, 
=======
//RayHitInfo __attribute__((always_inline))
void
transform_hit_info(RayHitInfo* hit_info, 
                   Ray ray,
                   Ray tr_ray, 
>>>>>>> Stashed changes
                   sqmat4 tr,
                   global Vertex* vertex_buffer,
                   global int* index_buffer)
{

        if (hit_info->hit) {
<<<<<<< Updated upstream
                
=======
                
                hit_info->n = compute_normal(vertex_buffer, 
                                            index_buffer,
                                            hit_info->id, 
                                            hit_info->uv);
                hit_info->n =
                        normalize(multiply_vector(hit_info->n,tr));
                
                /* If the normal is pointing out, 
                   invert it and note it in the flags */
                if (dot(hit_info->n,ray.dir) > 0) {
                        hit_info->inverse_n = true;
                        hit_info->n *= -1.f;
                }
>>>>>>> Stashed changes
                hit_info->hit_point = tr_ray.ori + tr_ray.dir * hit_info->t;
                hit_info->hit_point = multiply_point(hit_info->hit_point, tr);
                hit_info->t = distance(hit_info->hit_point, ray.ori); 
        }
<<<<<<< Updated upstream
     
}

void __attribute__((always_inline))
complete_hit_info(Ray ray, 
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
=======
>>>>>>> Stashed changes
}

void __attribute__((always_inline))
merge_hit_info(RayHitInfo* best_info, RayHitInfo* new_info){
        if (!best_info->hit ||
            (new_info->hit && new_info->t < best_info->t)) {
                    best_info->t  = new_info->t;
                    best_info->id = new_info->id;
                    best_info->uv = new_info->uv;
                    best_info->hit_point = new_info->hit_point;
                    //*best_info = *new_info;
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

        if (fabs(ray.invDir.x) > 1e-26f) {
                tMin = fmax(tMin, axis_t_min.x); tMax = fmin(tMax, axis_t_max.x);
        }
        if (fabs(ray.invDir.y) > 1e-26f) {
                tMin = fmax(tMin, axis_t_min.y); tMax = fmin(tMax, axis_t_max.y);
        }
        if (fabs(ray.invDir.z) > 1e-26f) {
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
        
        if (a > -1e-26f && a < 1e-26f)
	/* if (a > -0.000001f && a < 0.00001f) */
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

RayHitInfo 
try_leaf_hit(RayHitInfo* best_info,
             BVHNode node,
             global Vertex* vertex_buffer,
             global int* index_buffer,
             Ray ray){

        for (int i = node.start_index; i < node.end_index; ++i) {
                int triangle = i;
                RayHitInfo tri_hit = triangle_hit(vertex_buffer,
                                                index_buffer,
                                                triangle,
                                                ray);

                merge_hit_info(best_info, &tri_hit);

                if (best_info->hit)
                        ray.tMax = best_info->t;
        }
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
<<<<<<< Updated upstream
                } else {
                        test_bbox = current_node.bbox;
                        bool hit = bbox_hit(test_bbox, ray);

                        // If it hit, and closer to the closest hit up to now, check it
                        if (hit) {
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
=======
                }
                        
                bool hit = bbox_hit(current_node.bbox, ray);

                // If it hit, and closer to the closest hit up to now, check it
                if (hit) {
                        // If it's a leaf, check all primitives in the leaf, then go up
                        if (current_node.leaf) {
                                // Check all primitives in leaf
                                //RayHitInfo leaf_info = try_leaf_hit(&best_hit_info,
                                //                                current_node,
                                //                                vertex_buffer,
                                //                                index_buffer,
                                //                                ray);

                                RayHitInfo leaf_info = leaf_hit(current_node,
                                                              vertex_buffer,
                                                              index_buffer,
                                                              ray);
                                merge_hit_info(&best_hit_info, &leaf_info);
                                if (best_hit_info.hit)
                                        ray.tMax = best_hit_info.t;
>>>>>>> Stashed changes
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
trace(global RayHitInfo* trace_info,
      global RayPlus* rays,
      global Vertex* vertex_buffer,
      global int* index_buffer,
      global BVHNode* bvh_nodes,
      global BVHRoot* roots,
      int root_count)
      
{
        int index = get_global_id(0);
        int offset = get_global_offset(0);
        Ray ray = rays[index].ray;

        RayHitInfo best_info;
        best_info.hit = false;
        int best_root = 0;

        for (int i = 0; i < root_count; ++i) {

                Ray tr_ray = transform_ray(ray, roots[i].trInv);
                RayHitInfo root_info = trace_ray(tr_ray,vertex_buffer,index_buffer,
                                                     bvh_nodes, roots[i].node);
<<<<<<< Updated upstream
                /* float depth = root_hit_info.n.s0; */
                /*Compute real t and hit point to compare which hit is closest*/
                transform_hit_info(ray,
                                   tr_ray,
                                   &root_info,
                                   roots[i].tr,
                                   vertex_buffer,
                                   index_buffer);
                /* root_hit_info.uv.s0 = depth; */

                if (!best_info.hit ||
                    (root_info.hit && root_info.t < best_info.t)) {
                        best_info = root_info;
                        best_root = i;
                }
=======

                //transform_hit_info(&root_hit_info,
                //                   ray,
                //                   tr_ray,
                //                   roots[i].tr,
                //                   vertex_buffer,
                //                   index_buffer);

                //merge_hit_info (&hit_info, &root_hit_info);
>>>>>>> Stashed changes
        }

        /*Compute normal and texCoord at hit point*/
        if (best_info.hit)
                complete_hit_info(ray, roots[best_root].tr,&best_info, 
                                  vertex_buffer, index_buffer);
        /* Save hit info*/
        trace_info[index] = best_info;
}
