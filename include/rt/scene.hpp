#ifndef RT_SCENE_HPP
#define RT_SCENE_HPP

#include <vector>
#include <string>
#include <stdint.h>

#include <rt/mesh.hpp>
#include <rt/material.hpp>
#include <rt/bvh.hpp>
#include <rt/light.hpp>
#include <rt/geom.hpp>
#include <rt/obj-loader.hpp>


/*Once MeshInstances have been handed to the scene geometry, it is 
 the user's responsibility to make sure the mesh pointers remain valid */


typedef int32_t mesh_id;
typedef int32_t object_id;

bool is_valid_mesh_id(mesh_id id);
bool is_valid_object_id(object_id id);

static mesh_id invalid_mesh_id();
static object_id invalid_object_id();

struct Object {

	Object(mesh_id id);

	const mesh_id get_mesh_id();
	void set_mesh_id(mesh_id id);
	Object clone();

	bool is_valid(){return is_valid_mesh_id(id);}

	mesh_id id;
	GeometricProperties geom;
	material_cl mat;
};

struct SceneGeometry {

	object_id add_object(Object mi);
	void remove_object(object_id id);
	Object& object(object_id id);

	std::vector<Object> objects;
};


class Scene {

public:
	mesh_id load_obj_file(std::string filename);

	uint32_t create_aggregate();
	uint32_t create_bvh();

	Mesh& get_aggregate_mesh(){return geometry_aggregate;}
	BVH&  get_aggregate_bvh (){return bvh;}
	std::vector<material_cl>& get_material_list (){return material_list;}
	std::vector<cl_int>& get_material_map (){return material_map;}

	SceneGeometry geometry;

private:
	std::vector<Mesh> mesh_atlas;
	std::vector<Light> lights;

	Mesh geometry_aggregate;
	BVH bvh;
	std::vector<material_cl> material_list;
	std::vector<cl_int>   material_map;
};

#endif /* RT_SCENE_HPP */
