#include <rt/geom.hpp>
#include <rt/cl_aux.hpp>

static vec3 rotate(vec3 v, vec3 rpy)
{
	return v;
}

void 
GeometricProperties::transform(Vertex& v)
{
	vec3 vec3_pos = float3_to_vec3(v.position);
	
	/*This is a placeholder, a more efficient method is needed*/

	vec3_pos *= scale;
	vec3_pos = rotate(vec3_pos, rpy);
	vec3_pos += pos;

	
	v.position = vec3_to_float3(vec3_pos);

}

