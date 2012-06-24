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
 
} SampleTraceInfo;

typedef float3 Color;

typedef struct {

	float3 dir;
	Color  color;
} DirectionalLight;

typedef struct {
	
	Color ambient;
	DirectionalLight directional;

} Lights;

typedef struct 
{
        float t_min,t_max;
        unsigned int next_node;
} kdt_level;


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


bool 
leaf_hit(KDTNode node,
         global unsigned int* leaf_indices,
         global Vertex* vertex_buffer,
         global int* index_buffer,
         Ray ray){

        for (int i = node.tris_start; i < node.tris_end; ++i) {
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
                if (t > ray.tMin)
                        return true;
        }
        return false;
}

float
compute_t_split(KDTNode node, Ray ray) 
{
        if (node.split_axis == 0) {
                return (node.split_coord - ray.ori.x) * ray.invDir.x;
        } else if (node.split_axis == 1) {
                return (node.split_coord - ray.ori.y) * ray.invDir.y;
        } else {
                return (node.split_coord - ray.ori.z) * ray.invDir.z;
        }
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

bool
trace_shadow_ray(Ray ray,
                 global Vertex* vertex_buffer,
                 global int* index_buffer,
                 global KDTNode* kdt_nodes,
                 global unsigned int* leaf_indices,
                 BBox           scene_bbox)
{
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
                return false;
        t_min *= 0.999f;
        t_max *= 1.001f;
        t_min = fmax(ray.tMin,t_min);
        
        while(1) {
                current_node = kdt_nodes[curr];

                if (current_node.leaf) {

                        if (current_node.tris_end > current_node.tris_start) {
                                if (leaf_hit(current_node,
                                             leaf_indices,
                                             vertex_buffer,
                                             index_buffer,
                                             ray))
                                        return true;
                        }
                        if (level > 0 && level < 64) {
                                level--;
                                curr  = levels[level].next_node;
                                t_min = levels[level].t_min;
                                t_max = levels[level].t_max;
                                continue;
                        } else {
                                return false;
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
        
                if (t_split > t_max || t_split <= 0) {
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
shadow_trace_single(global SampleTraceInfo* trace_info,
                    global Sample* samples,
                    global Vertex* vertex_buffer,
                    global int* index_buffer,
                    global KDTNode* kdt_nodes,
                    global unsigned int* leaf_indices,
                    BBox scene_bbox,
                    global Lights* lights)
{
	int index = get_global_id(0);
	SampleTraceInfo info  = trace_info[index];

	if (!info.hit)
		return;

	Ray ray;
	ray.dir = -lights->directional.dir;
	ray.invDir = 1.f/ray.dir;
        ray.ori = info.hit_point;
  	ray.tMin = 0.01f; ray.tMax = 1e37f;

        bool hit = trace_shadow_ray(ray,vertex_buffer,index_buffer,
                                    kdt_nodes, leaf_indices, scene_bbox);
        trace_info[index].shadow_hit = hit;
}


