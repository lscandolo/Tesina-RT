typedef struct 
{
	float3 rgb;
} Color;

typedef struct 
{
	int rgb;
} ColorInt;


/* kernel void  */
/* init(global Color* image) */
/* { */
/* 	int index = get_global_id(0); */
/* 	image[index].rgb = (float3)(0.f,0.f,0.f); */

/* } */

kernel void 
init(global ColorInt* image)
{
	int index = get_global_id(0);
	image[index].rgb = 0;
}

