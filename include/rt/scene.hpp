#ifndef RT_SCENE_HPP
#define RT_SCENE_HPP

#include <vector>
#include <string>
#include <stdint.h>

#include <cl-gl/opencl-init.hpp>
#include <gpu/interface.hpp>
#include <rt/mesh.hpp>
#include <rt/material.hpp>
#include <rt/bvh.hpp>
#include <rt/multi-bvh.hpp>
#include <rt/light.hpp>
#include <rt/geom.hpp>
#include <rt/obj-loader.hpp>

/*Once MeshInstances have been handed to the scene geometry, it is 
 the user's responsibility to make sure the mesh pointers remain valid */


typedef int32_t mesh_id;
typedef int32_t object_id;

bool is_valid_mesh_id(mesh_id id);
bool is_valid_object_id(object_id id);

struct Object {

	Object(mesh_id id);

	const mesh_id get_mesh_id();
	void set_mesh_id(mesh_id id);
	Object clone();

	bool is_valid(){return is_valid_mesh_id(id);}

	mesh_id id;
	GeometricProperties geom;
	material_cl mat;
	vec3 slack;
};

struct SceneGeometry {

	object_id add_object(mesh_id mid);
	void remove_object(object_id id);
	Object& object(object_id id);

	std::vector<Object> objects;
};


class Scene {

public:
        
        Scene();
        int32_t initialize(CLInfo& clinfo);
        bool    valid();
        bool    valid_aggregate();
        bool    valid_bvhs();

	int32_t create_aggregate_mesh();
	int32_t create_aggregate_bvh();

	Mesh&   get_aggregate_mesh(){return aggregate_mesh;}
	BVH&    get_aggregate_bvh (){return aggregate_bvh;}

        int32_t transfer_aggregate_mesh_to_device();
        int32_t transfer_aggregate_bvh_to_device();

        int32_t create_bvhs();
        int32_t transfer_meshes_to_device();
        int32_t transfer_bvhs_to_device();
        
	int32_t set_dir_light(const directional_light_cl& dl);
	int32_t set_ambient_light(const color_cl& color);

	mesh_id load_obj_file(std::string filename);

        DeviceMemory& vertex_mem();
        DeviceMemory& triangle_mem();
        DeviceMemory& material_list_mem();
        DeviceMemory& material_map_mem();
        DeviceMemory& bvh_nodes_mem();
        DeviceMemory& bvh_roots_mem();
        DeviceMemory& lights_mem();

	std::vector<material_cl>& get_material_list (){return material_list;}
	std::vector<cl_int>&      get_material_map (){return material_map;}

	bool reorderTriangles(const std::vector<uint32_t>& new_order);

	SceneGeometry geometry;


        /*!! Create Multi-BVH */
        uint32_t create_multiple_bvhs();
        uint32_t update_bvh_roots();

	std::vector<Mesh> mesh_atlas;
private:

	Mesh aggregate_mesh;
	BVH  aggregate_bvh;
	std::vector<material_cl> material_list;
	std::vector<cl_int>   material_map;

public:
        std::map<index_t, BVH> bvhs;
        std::map<index_t, std::vector<cl_int> > mmaps;
        std::vector<index_t> bvh_order; /*This is to keep track of the order in which the 
                                          bvhs were created, since the node references 
                                          are offset, they need to be ordered the same way
                                          when we move them to device mem*/
        std::vector<BVHRoot> bvh_roots;

private:
        bool m_initialized;
        bool m_aggregate_mesh_built;
        bool m_aggregate_bvh_built;
        bool m_bvhs_built;

        bool m_multi_bvh;

	lights_cl lights;

        DeviceInterface device;
	memory_id vert_id;
	memory_id tri_id;
	memory_id mat_map_id;
	memory_id mat_list_id;
	memory_id bvh_id;
	memory_id lights_id;
        memory_id bvh_roots_id;
};



#endif /* RT_SCENE_HPP */
