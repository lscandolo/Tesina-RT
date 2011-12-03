#define STEPS 256.f

typedef struct vec3_t
{
	float x,y,z;
} vec3;

typedef struct Ray_t
{
	vec3 ori;
	vec3 dir;
	vec3 invDir;
	float tMax, tMin;
} Ray;

__kernel void trace(__write_only image2d_t img,
		   __global Ray* ray_buffer,
		   __read_only int div)

{
	int x = get_global_id(0);
	int y = get_global_id(1);
	int width = get_global_size(0)-1;
	int height = get_global_size(1)-1;

	int index = x + y * (width+1);

	float3 v1,v2,v0;
	v0 = (float3)(-1.f, 1.f, 5.f+ (div/100.f));
	v1 = (float3)( 1.f, 1.f, 5.f+ (div/100.f));
	v2 = (float3)( 0.f,-1.f, 5.f+ (div/100.f));

	Ray ray = ray_buffer[index];

	float3 p = (float3)(ray.ori.x,ray.ori.y,ray.ori.z);
	float3 d = (float3)(ray.dir.x,ray.dir.y,ray.dir.z);

	float3 e1 = v1 - v0;
	float3 e2 = v2 - v0;

	float3 h = cross(d, e2);
	float a = dot(e1,h);

	bool hit = false;

	float t = 0.f;

	if (a > -0.000001f && a < 0.00001f) {
		hit = false; 
	} else {
		float f = 1.f/a;
		float3 s = p - v0;
		float u = f * dot(s,h);
		if (u < 0.f || u > 1.f) {
			hit = false;
		} else {
			float3 q = cross(s,e1);
			float v = f * dot(d,q);
			if (v < 0.f || u+v > 1.f) {
				hit = false;
			} else {
				t = f * dot(e2,q);
				hit = (t < ray.tMax && t > ray.tMin);
			}
		}
	}


	int2 image_size = (int2)(get_image_width(img), get_image_height(img));

	float x_part = (float)x/(float)width;
	float y_part = (float)y/(float)height;

	int2 coords = (int2)( ( (image_size.x-1) * x ) / width,
			      ( (image_size.y-1) * y ) / height);


	float val;
	if (hit)
		val = t;
	else
		val = 0.f;

	float4 valf = (float4)(val/1000.f,val/100.f,val/10.f,1.0f);

	write_imagef(img, coords, valf);

}


/* vec3 a,b,c; */
/* a.x = -1; b.x = 1; c.x = 0; */
/* a.y = -1; b.y = -1; c.y = 1; */
/* a.z = 2; b.z = 2; c.z = 2; */

/* float3 ac = a - c; */
/* float3 bc = b - c; */

// float4 valf = (float4)((div*x)/(w* STEPS), 0.0f, (div*y)/(h* STEPS), 1.0f);

// float4 valf = (float4)(0.2f,0.3f,0.6f,1.0f) * ((float)div/STEPS);


