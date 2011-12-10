#ifndef RT_SCENE_HPP
#define RT_SCENE_HPP

#include <vector>
#include <stdint.h>

#include <rt/mesh.hpp>
#include <rt/light.hpp>
#include <rt/geom.hpp>

class Scene {

	std::vector<MeshInstance> mesh_atlas;
	std::vector<Light> lights;
};

class Object {

private:
	typedef int32_t MeshReference;
public:

	MeshReference model; /*Should reference a mesh in a model atlas*/
	GeometricProperties geom;
	Material& mat;
};

class ObjectInstance {

	ObjectInstance(Object& instance) : reference(instance) {}

	Object& reference;
	GeometricProperties geom;
};

class Model {
	Mesh mesh;
};



#endif /* RT_SCENE_HPP */
