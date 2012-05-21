#ifndef RT_SCENE_HPP
#define RT_SCENE_HPP

#include <vector>
#include <string>
#include <stdint.h>

#include <cl-gl/opencl-init.hpp>
#include <gpu/interface.hpp>
#include <rt/mesh.hpp>
#include <rt/material.hpp>
#include <rt/texture-atlas.hpp>
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

        Mesh&    get_mesh(mesh_id mid);
        BVH&     get_object_bvh(object_id oid);
        int32_t  update_mesh_vertices(mesh_id mid);
        uint32_t update_bvh_roots();
        
        size_t   root_count();

        /* Ligthing methods */
        int32_t set_dir_light(const directional_light_cl& dl);
        int32_t set_ambient_light(const color_cl& color);

        /* File load methods */
        std::vector<mesh_id> load_obj_file(std::string filename);
        mesh_id load_obj_file_as_aggregate(std::string filename);
        void    load_obj_file_and_make_objs(std::string filename);

        /*Object methods*/
        size_t   object_count(){return bvh_roots.size();}
        object_id add_object(mesh_id mid);
        std::vector<object_id> add_objects(std::vector<mesh_id> mids);
        void remove_object(object_id id);
        Object& object(object_id id);

        /*GPU resources*/
        int32_t acquire_graphic_resources();
        int32_t release_graphic_resources();

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

        size_t triangle_count();
        size_t vertex_count();


        TextureAtlas texture_atlas;
private:

        Mesh aggregate_mesh;
        BVH  aggregate_bvh;

        std::vector<Object> objects;
        std::vector<Mesh> mesh_atlas;

        std::vector<material_cl> material_list;
        std::vector<cl_int>   material_map;

        std::map<mesh_id, BVH> bvhs;
        std::map<mesh_id, std::vector<cl_int> > mmaps;
        std::vector<mesh_id> bvh_order; /*This is to keep track of the order in which the 
                                        bvhs were created, since the node references 
                                        are offset, they need to be ordered the same way
                                        when we move them to device mem*/
        std::vector<BVHRoot> bvh_roots;

private:
        bool m_initialized;
        bool m_aggregate_mesh_built;
        bool m_aggregate_bvh_built;
        bool m_bvhs_built;

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
