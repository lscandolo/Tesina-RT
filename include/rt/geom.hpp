#ifndef RT_GEOM_HPP
#define RT_GEOM_HPP

#include <rt/vector.hpp>
#include <rt/matrix.hpp>
#include <rt/mesh.hpp>

class GeometricProperties {

public:
	GeometricProperties()
		{
			pos = makeVector(0.f,0.f,0.f);
			rpy = makeVector(0.f,0.f,0.f);
			scale = makeVector(1.f,1.f,1.f);
			updateTransform();
		}

	void setPos(const vec3& new_pos);
	void setRpy(const vec3& new_rpy);
	void setScale(float new_scale);
	void setScale(const vec3& new_scale);

	void transform(Vertex& v);

private:

	void updateTransform();

	vec3 pos;
	vec3 rpy; /* Would be cleaner with a quaternion here...*/
	vec3 scale;

	mat4x4 M;
};

#endif /* RT_GEOM_HPP */
