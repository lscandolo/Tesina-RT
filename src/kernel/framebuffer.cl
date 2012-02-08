typedef struct 
{
	float3 rgb;
} Color;

typedef struct 
{
	int rgb;
} ColorInt;


kernel void 
init(global ColorInt* image)
{
	int index = get_global_id(0);
	image[index].rgb = 0;
}

kernel void 
copy(global ColorInt* image,
     write_only image2d_t tex)
{
	int width = get_image_width(tex);
	int heihgt = get_image_height(tex);
	int index = get_global_id(0);

	int2 xy = (int2)(index%width,index/width);

	int rgb = image[index].rgb;
	
	const float inv_255 = 1.f/255.f;

	float r = (rgb >> 16) * inv_255;
	float g = ((rgb & 0xff00) >> 8) * inv_255;
	float b = (rgb & 0xff) * inv_255;

	float4 texval = (float4)(r,g,b,1.f);

	write_imagef(tex, xy, texval);
}

