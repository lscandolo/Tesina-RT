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
        float4 hi;
        float4 lo;
} BBox;

typedef struct {

        bool  leaf;
        char  split_axis;
        float split_coord;
        union {
                unsigned int  tris_start;
                unsigned int  l_child;
        };
        union {
                unsigned int tris_end;
                unsigned int r_child;
        };

} KDTNode;

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

typedef struct 
{
        float t_min,t_max;
        unsigned int next_node;
} kdt_level;

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

kdt_level /* __attribute__((always_inline)) */
bbox_hit(BBox bbox,
         Ray ray)
{
        float tMin = ray.tMin;
        float tMax = ray.tMax;

        float3 axis_t_lo, axis_t_hi;

        axis_t_lo = (bbox.lo.xyz - ray.ori) * ray.invDir;
        axis_t_hi = (bbox.hi.xyz - ray.ori) * ray.invDir;

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

        kdt_level lev;
        lev.t_min = tMin;
        lev.t_max = tMax;
        return lev;
}

RayHitInfo 
leaf_hit(KDTNode node,
         global unsigned int* leaf_indices,
         global Vertex* vertex_buffer,
         global int* index_buffer,
         Ray ray, float t_min, float t_max){

        RayHitInfo hit_info;
        hit_info.hit = false;
        hit_info.inverse_n = false;
        t_min *= 0.9999f;
        t_max *= 1.0001f;
        hit_info.t = t_max;

        for (unsigned int i = node.tris_start; i < node.tris_end; ++i) {
                unsigned int triangle = leaf_indices[i];

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

                bool t_is_within_bounds = (t <= t_max && t >= t_min && t <= hit_info.t);
                /* bool t_is_within_bounds = t > 0 && t < 1e36f; */
                if (t_is_within_bounds) {
                        hit_info.hit = true;
                        hit_info.t   = t;
                        hit_info.id = triangle;
                        hit_info.uv.s0 = u;
                        hit_info.uv.s1 = v;
                }
        }
        return hit_info;
}

float
compute_t_split(KDTNode node, Ray ray) 
{
        float3 split = (float3)(node.split_coord,node.split_coord,node.split_coord);
        split = (split - ray.ori) * ray.invDir;
        if (node.split_axis == 0) {
                return split.x;
        } else if (node.split_axis == 1) {
                return split.y;
        } else {
                return split.z;
        }

        /* if (node.split_axis == 0) { */
        /*         return (node.split_coord - ray.ori.x) * ray.invDir.x; */
        /* } else if (node.split_axis == 1) { */
        /*         return (node.split_coord - ray.ori.y) * ray.invDir.y; */
        /* } else { */
        /*         return (node.split_coord - ray.ori.z) * ray.invDir.z; */
        /* } */
}

bool 
compute_below_first(Ray ray, KDTNode current_node, float t_split)
{
        if (current_node.split_axis == 0) {
                return (ray.ori.x < current_node.split_coord || 
                        (ray.ori.x == current_node.split_coord && ray.dir.x >= 0));

        } else if (current_node.split_axis == 1) {
                return (ray.ori.y < current_node.split_coord || 
                        (ray.ori.y == current_node.split_coord && ray.dir.y >= 0));

        } else {
                return (ray.ori.z < current_node.split_coord || 
                        (ray.ori.z == current_node.split_coord && ray.dir.z >= 0));
        }
}

RayHitInfo trace_ray(Ray ray,
                     global Vertex* vertex_buffer,
                     global int* index_buffer,
                     global KDTNode* kdt_nodes,
                     global unsigned int* leaf_indices,
                     BBox           scene_bbox)
{
        RayHitInfo hit_info;
        hit_info.hit = false;
        hit_info.shadow_hit = false;
        hit_info.inverse_n = false;
        hit_info.t = ray.tMax;

        kdt_level levels[64];
        unsigned int level = 0;

        unsigned int curr = 0;
        KDTNode current_node;
        unsigned int first_child, second_child;

        float t_min, t_max;
        kdt_level lev = bbox_hit(scene_bbox, ray);
        t_min = lev.t_min;
        t_max = lev.t_max;
        if (t_max < t_min || t_min < ray.tMin)
                return hit_info;
        t_min *= 0.999f;
        t_max *= 1.001f;
        t_min = fmax(ray.tMin,t_min);
        
        while(1) {
                current_node = kdt_nodes[curr];

                if (current_node.leaf) {

                        if (current_node.tris_end > current_node.tris_start) {
                                hit_info = leaf_hit(current_node,
                                                    leaf_indices,
                                                    vertex_buffer,
                                                    index_buffer,
                                                    ray, t_min, t_max);
                                if (hit_info.hit)
                                        return hit_info;
                        }

                        if (level > 0 && level < 64) {
                                level--;
                                curr  = levels[level].next_node;
                                t_min = levels[level].t_min;
                                t_max = levels[level].t_max;
                                continue;
                        } else {
                                return hit_info;
                        }
                }

                float t_split = compute_t_split(current_node, ray);
                bool below_first = compute_below_first(ray, current_node, t_split);
                if (below_first) {
                        first_child = current_node.l_child;
                        second_child = current_node.r_child;
                } else {
                        first_child = current_node.r_child;
                        second_child = current_node.l_child;
                }
        
                if (t_split > t_max || t_split < 0) {
                        curr = first_child;
                }
                else if (t_split < t_min) {
                        curr = second_child;
                }
                else {
                        levels[level].next_node = second_child;
                        levels[level].t_min = t_split;
                        levels[level].t_max = t_max;
                        level++;

                        curr = first_child;
                        t_max = t_split;
                }
                
        }

}

kernel void 
trace_single(global RayHitInfo* trace_info,
             global RayPlus* rays,
             global Vertex* vertex_buffer,
             global int* index_buffer,
             global KDTNode* kdt_nodes,
             global unsigned int* leaf_indices,
                    BBox scene_bbox)
{
        int index = get_global_id(0);
        Ray ray = rays[index].ray;
        RayHitInfo hit_info = trace_ray(ray,vertex_buffer,index_buffer,
                                        kdt_nodes, leaf_indices, scene_bbox);
        
        if (hit_info.hit)
                complete_hit_info(ray, &hit_info, vertex_buffer, index_buffer);

        trace_info[index] = hit_info;
}
