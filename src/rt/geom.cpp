#include <rt/geom.hpp>
#include <rt/math.hpp>
#include <rt/cl_aux.hpp>

void 
GeometricProperties::setPos(const vec3& new_pos)
{
	pos = new_pos;
	updateTransform();
}

void 
GeometricProperties::setRpy(const vec3& new_rpy)
{
	rpy = new_rpy;
	updateTransform();
}

void 
GeometricProperties::setScale(float new_scale)
{
	scale = makeVector(new_scale,new_scale,new_scale);
	updateTransform();
}

void 
GeometricProperties::setScale(const vec3& new_scale)
{
	scale = new_scale;
	updateTransform();
}

void 
GeometricProperties::updateTransform()
{
	mat4x4 posM = translationMatrix4x4(pos);
	mat4x4 rpyM = rotationMatrix4x4(rpy);
	mat4x4 scaleM = scaleMatrix4x4(scale);
	M = scaleM * posM * rpyM;
}

void 
GeometricProperties::transform(Vertex& v)
{
	vec4 vec4_pos       = float3_to_vec4(v.position, 1.f);
	vec4 vec4_normal    = float3_to_vec4(v.normal, 0.f);
	vec4 vec4_tangent   = float4_to_vec4(v.tangent);
	vec4 vec4_bitangent = float3_to_vec4(v.bitangent, 0.f);

	vec4_pos       = M * vec4_pos;
	vec4_normal    = M * vec4_normal;
	vec4_tangent   = M * vec4_tangent;
	vec4_bitangent = M * vec4_bitangent;

	vec4_normal[3] = 1.f;
	vec4_bitangent[3] = 1.f;

	v.position  = vec3_to_float3(homogenize(vec4_pos));
	v.normal    = vec3_to_float3(homogenize(vec4_normal));
	v.tangent   = vec4_to_float4(vec4_tangent);
	v.bitangent = vec3_to_float3(homogenize(vec4_bitangent));

}

