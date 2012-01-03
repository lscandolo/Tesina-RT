#ifndef RT_GEOM_HPP
#define RT_GEOM_HPP

#include <rt/vector.hpp>
#include <rt/mesh.hpp>

class GeometricProperties {

public:
	GeometricProperties()
		{
			pos = makeVector(0.f,0.f,0.f);
			rpy = makeVector(0.f,0.f,0.f);
			scale = 1.f;
		}

	void transform(Vertex& v);

	vec3 pos;
	vec3 rpy; /* Would be cleaner with a quaternion here...*/
	float scale;
};

#endif /* RT_GEOM_HPP */
