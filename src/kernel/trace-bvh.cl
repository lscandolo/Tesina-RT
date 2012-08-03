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
} Sample;

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
 
} SampleTraceInfo;

typedef struct {
        int id;
        float  t;
        float u;
        float v;
} RayHit;

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
transform_hit_info(const Ray ray, 
                   const Ray tr_ray, 
                   RayHit* hit_info, 
                   const sqmat4 tr)
{

        if (hit_info->id >= 0) {
                float3 hit_point = tr_ray.ori + tr_ray.dir * hit_info->t;
                hit_point = multiply_point(hit_point, tr);
                hit_info->t = distance(hit_point, ray.ori) ; 
        }
     
}

void __attribute__((always_inline))
complete_transformed_hit_info(const Ray ray, 
                              const sqmat4 tr,
                              const RayHit hit_info, 
                              global SampleTraceInfo* trace_info, 
                              global Vertex* vertex_buffer,
                              global int* index_buffer)
{
        int index = get_global_id(0);
        trace_info += index;

        SampleTraceInfo ray_hit_info;
        ray_hit_info.hit = true;
        ray_hit_info.t = hit_info.t;
        ray_hit_info.id = hit_info.id;
        ray_hit_info.hit_point = ray.ori + ray.dir * hit_info.t;

        int id = 3 * hit_info.id;

        global Vertex* vx0 = &vertex_buffer[index_buffer[id]];
        global Vertex* vx1 = &vertex_buffer[index_buffer[id+1]];
        global Vertex* vx2 = &vertex_buffer[index_buffer[id+2]];

        float u = hit_info.u;
        float v = hit_info.v;
        float w = 1.f - (u+v);

        float3 n0 = normalize(vx0->normal);
        float3 n1 = normalize(vx1->normal);
        float3 n2 = normalize(vx2->normal);

        float2 t0 = vx0->texCoord;
        float2 t1 = vx1->texCoord;
        float2 t2 = vx2->texCoord;

        float3 n = normalize(w * n0 + v * n2 + u * n1);

        ray_hit_info.n = multiply_vector(n, tr);

        /* If the normal is pointing out, 
           invert it and note it in the flags */
        if (dot(ray_hit_info.n,ray.dir) > 0) {
                ray_hit_info.inverse_n = true;
                ray_hit_info.n *= -1.f;
        } else {
                ray_hit_info.inverse_n = false;
        }

        ray_hit_info.uv = w * t0 + v * t2 + u * t1;
        *trace_info = ray_hit_info;
}

void __attribute__((always_inline))
complete_trace_info(const Ray ray, 
                    const RayHit hit_info,
                    global SampleTraceInfo* trace_info, 
                    global Vertex* vertex_buffer,
                    global int* index_buffer)
{
        int index = get_global_id(0);
        trace_info += index;

        if (hit_info.id < 0) {
                trace_info->hit = false;
                return ;
        }

        SampleTraceInfo ray_hit_info;

        ray_hit_info.hit = true;
        ray_hit_info.t = hit_info.t;
        ray_hit_info.id = hit_info.id;
        ray_hit_info.hit_point = ray.ori + ray.dir * hit_info.t;

        int id = 3 * hit_info.id;

        global Vertex* vx0 = &vertex_buffer[index_buffer[id]];
        global Vertex* vx1 = &vertex_buffer[index_buffer[id+1]];
        global Vertex* vx2 = &vertex_buffer[index_buffer[id+2]];

        float u = hit_info.u;
        float v = hit_info.v;
        float w = 1.f - (u+v);

        float3 n0 = normalize(vx0->normal);
        float3 n1 = normalize(vx1->normal);
        float3 n2 = normalize(vx2->normal);

        float2 t0 = vx0->texCoord;
        float2 t1 = vx1->texCoord;
        float2 t2 = vx2->texCoord;

        float3 n = normalize(w * n0 + v * n2 + u * n1);
        
        /* If the normal is pointing out, 
           invert it and note it in the flags */
        if (dot(n,ray.dir) > 0) { 
                ray_hit_info.inverse_n = true;
                ray_hit_info.n = -n;
        } else {
                ray_hit_info.inverse_n = false;
                ray_hit_info.n = n;
        }
        
        ray_hit_info.uv = w * t0 + v * t2 + u * t1;
        *trace_info = ray_hit_info;

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

void
leaf_hit(RayHit* best_info,
         BVHNode node,
         global Vertex* vertex_buffer,
         global int* index_buffer,
         Ray ray){

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
                bool t_is_within_bounds = (t <= best_info->t && t >= ray.tMin);

                if (t_is_within_bounds) {
                        best_info->t = t;
                        best_info->id = triangle;
                        best_info->u = u;
                        best_info->v = v;
                }
        }
}

#define MAX_LEVELS 30
RayHit trace_ray(Ray ray,
                 global Vertex* vertex_buffer,
                 global int* index_buffer,
                 global BVHNode* bvh_nodes,
                 int bvh_root)
{
        RayHit best_hit;
        best_hit.id = -1;
        best_hit.t =  ray.tMax;

        unsigned int levels[MAX_LEVELS];
        unsigned int level = 0;

        unsigned int curr = bvh_root;
        
        private BVHNode current_node;

        unsigned int first_child, second_child;
        bool children_order = true;

        float3 rdir = ray.dir;
        float3 adir = fabs(rdir);
        float max_dir_val = fmax(adir.x, fmax(adir.y,adir.z)) - 0.00001f;
        adir = fdim(adir, (float3)(max_dir_val,max_dir_val,max_dir_val));

        while (true) {
                current_node = bvh_nodes[curr];

                if (!bbox_hit(current_node.bbox, ray)) {

                        if (level > 0 && level < MAX_LEVELS) {
                                level--;
                                curr  = levels[level];
                                continue;
                        }
                        else {
                                return best_hit;
                        }
                }

                if (current_node.leaf) {
                        leaf_hit(&best_hit,
                                 current_node,
                                 vertex_buffer,
                                 index_buffer,
                                 ray);

                        if (best_hit.id >= 0)
                                ray.tMax = best_hit.t;

                        if (level > 0 && level < MAX_LEVELS) {
                                level--;
                                curr  = levels[level];
                                continue;
                        }
                        else {
                                return best_hit;
                        }
                } else {

                        /* Compute node children traversal order if its an interior node*/
                        float3 lbbox_vals = bvh_nodes[current_node.l_child].bbox.lo;
                        float3 rbbox_vals = bvh_nodes[current_node.r_child].bbox.lo;
                        float3 choice_vec = dot(rdir,rbbox_vals - lbbox_vals);
                        if (adir.x > 0.f) {
                                children_order = choice_vec.x > 0.f;
                        } else if (adir.y > 0.f) {
                                children_order = choice_vec.y > 0.f;
                        } else {
                                children_order = choice_vec.z > 0.f;
                        }
                        
                        if (children_order) {
                                first_child = current_node.l_child;
                                second_child = current_node.r_child;
                        } else {
                                first_child = current_node.r_child;
                                second_child = current_node.l_child;
                        }
                }

                levels[level] = second_child;
                level++;
                curr = first_child;
        }
        return best_hit;
}


kernel void 
trace_multi(global SampleTraceInfo* trace_info,
            global Sample* samples,
            global Vertex* vertex_buffer,
            global int* index_buffer,
            global BVHNode* bvh_nodes,
            global BVHRoot* roots,
            int root_count)
{
        int index = get_global_id(0);
        Ray ray = samples[index].ray;

        RayHit best_hit;
        best_hit.id = -1;
        int best_root = -1;
        
        for (int i = 0; i < root_count; ++i) {

                Ray tr_ray = transform_ray(ray, roots[i].trInv);
                RayHit root_hit = trace_ray(tr_ray,vertex_buffer,index_buffer,
                                            bvh_nodes, roots[i].node);

                /*Compute real t to compare which hit is closest*/
                transform_hit_info(ray,
                                   tr_ray,
                                   &root_hit,
                                   roots[i].tr);

                if (root_hit.id >= 0 &&
                    (best_hit.id < 0 || root_hit.t < best_hit.t)) {
                        best_hit = root_hit;
                        best_root = i;
                }
        }

        /*Compute normal and texCoord at hit point*/
        if (best_hit.id >= 0)
                complete_transformed_hit_info(ray, roots[best_root].tr, best_hit,
                                              trace_info, vertex_buffer, index_buffer);
        else
                /* Save hit info*/
                trace_info[index].hit = false;
                
}

kernel void 
trace_single(global SampleTraceInfo* trace_info,
             global Sample* samples,
             global Vertex* vertex_buffer,
             global int* index_buffer,
             global BVHNode* bvh_nodes)
{
        int index = get_global_id(0);
        Ray ray = samples[index].ray;
        RayHit best_hit;

        best_hit = trace_ray(ray,vertex_buffer,index_buffer,
                             bvh_nodes, 0);

        complete_trace_info(ray, best_hit, trace_info, vertex_buffer, index_buffer);
        
}
