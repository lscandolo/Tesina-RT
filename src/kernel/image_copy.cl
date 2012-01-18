typedef struct 
{
	float3 rgb;
} Color;


kernel void 
copy(global Color* image,
       write_only image2d_t tex)
{
	int width = get_image_width(tex);
	int heihgt = get_image_height(tex);
	int index = get_global_id(0);

	int2 xy = (int2)(index%width,index/width);

	float4 texval = (float4)(image[index].rgb,1.f);

	write_imagef(tex, xy, texval);
	
}

