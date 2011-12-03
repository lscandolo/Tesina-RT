__kernel void Grad(__write_only image2d_t bmp, __read_only int div)

{

int x = get_global_id(0);
int y = get_global_id(1);
int w = get_global_size(0)-1;
int h = get_global_size(1)-1;

int2 coords = (int2)(x,y);

float red = (float)x/(float)w;
float blue = (float)y/(float)h;

float4 valf = (float4)((div*red)/1000.f, 0.0f, (div*blue)/1000.f, 1.0f);
write_imagef(bmp, coords, valf);

}