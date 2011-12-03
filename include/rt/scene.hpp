#ifndef RT_SCENE_HPP
#define RT_SCENE_HPP

#include <vector>

class Scene {

	std::vector<MeshInstance>;
	std::vector<Light>;
};

class GeometricProperties {

	Vector Pos;
	Vector Rot;
	float  scale;
};

class ObjectInstance {

	Object& reference;
	GeometricProperties geom;
};


class Object {

	ModelReference model; /*Should reference a model in a model atlas*/
	GeometricProperties geom;
	Material& mat;
};

class Model {

	Mesh mesh;

};



#endif /* RT_SCENE_HPP */
