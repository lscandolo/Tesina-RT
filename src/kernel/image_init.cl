typedef struct 
{
	float3 rgb;
} Color;


kernel void 
init(global Color* image)
{
	int index = get_global_id(0);
	image[index].rgb = (float3)(0.f,0.f,0.f);

}

