typedef struct 
{
        float3 position;
        float3 normal;
        float4 tangent;
        float3 bitangent;
        float2 texCoord;
} Vertex;

typedef unsigned int tri_id;

kernel void 
mangle(global Vertex* vertex_buffer,
       float arg,
       float last_arg,
       float height)

{
	int index = get_global_id(0);

	vertex_buffer[index].position.y = height * sin(0.33f*arg+vertex_buffer[index].position.x) 
		* cos(0.57f*arg+vertex_buffer[index].position.z);

        float xfactor = (0.33f + fmod(0.01238431 * index,0.23467)) * arg;
        float zfactor = (0.57f + fmod(0.02173 * index,0.3237247)) * arg;

	vertex_buffer[index].normal.x = cos(xfactor + vertex_buffer[index].position.x);
	vertex_buffer[index].normal.z = -sin(zfactor + vertex_buffer[index].position.z);

	return;

}
