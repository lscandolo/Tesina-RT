#include <iostream> //!!


#include <rt/scene.hpp>
#include <rt/vector.hpp>
#include <rt/cl_aux.hpp>

static mesh_id invalid_mesh_id();
// static object_id invalid_object_id();

/*-------------------- Object Methods -------------------------------*/

Object::Object(mesh_id _id) : id(_id){slack = vec3_zero;}

const mesh_id 
Object::get_mesh_id()
{
	return id;
}

void
Object::set_mesh_id(mesh_id new_id)
{
	id = new_id;
}

Object 
Object::clone()
{
	Object mi(id);
	mi.geom = geom;
	mi.mat = mat;
	return mi;
}

/*------------------------- SceneGeometry Methods ---------------------*/

object_id 
SceneGeometry::add_object(mesh_id mid)
{
	uint32_t oid = uint32_t(objects.size());
	objects.push_back(Object(mid));
	return oid;
}
	
void 
SceneGeometry::remove_object(object_id id)
{
	if (id < objects.size())
		objects[id].set_mesh_id(invalid_mesh_id());
}

Object& 
SceneGeometry::object(object_id id)
{
	ASSERT(is_valid_object_id(id) && id < objects.size());
	return objects[id];
}

/*-------------------------- Scene Methods ------------------------------*/

mesh_id
Scene::load_obj_file(std::string filename)
{
	ModelOBJ obj;
	if (!obj.import(filename.c_str())){
		return invalid_mesh_id();
	}

	Mesh mesh;
	obj.toMesh(&mesh);
	mesh_atlas.push_back(mesh);
	return uint32_t(mesh_atlas.size() - 1);
}

uint32_t 
Scene::create_aggregate()
{
	uint32_t base_triangle = 0;
	material_list.clear();
	material_map.clear();

	for (uint32_t i = 0; i < geometry.objects.size(); ++i) {
		Object& obj = geometry.objects[i];

		if (!obj.is_valid())
			continue;

		mesh_id m_id = obj.get_mesh_id();

		ASSERT(m_id >= 0 && m_id < mesh_atlas.size());

		size_t base_vertex = geometry_aggregate.vertexCount();
		Mesh& mesh = mesh_atlas[m_id];
		GeometricProperties g = obj.geom;

		material_list.push_back(obj.mat);
		cl_int map_index = cl_int(material_list.size() - 1);
		material_map.resize(base_triangle + mesh.triangleCount(), map_index);
		
		for (uint32_t v = 0; v < mesh.vertexCount(); ++v) {
			Vertex vertex = mesh.vertex(v);
			g.transform(vertex);
			geometry_aggregate.vertices.push_back(vertex);
		}
		for (uint32_t t = 0; t < mesh.triangleCount(); ++t) {
			Triangle tri = mesh.triangle(t);
			tri.v[0] += index_t(base_vertex);
			tri.v[1] += index_t(base_vertex);
			tri.v[2] += index_t(base_vertex);
			geometry_aggregate.triangles.push_back(tri);
			geometry_aggregate.slacks.push_back(obj.slack);
		}
		base_triangle += uint32_t(mesh.triangleCount());
		
	}
	return 0;
}

uint32_t 
Scene::create_bvh(){
	if (!bvh.construct_and_map(geometry_aggregate, material_map))
		return 1;
	return 0;
}

bool 
Scene::reorderTriangles(const std::vector<uint32_t>& new_order) {
	ASSERT(new_order.size() == geometry_aggregate.triangles.size());
	ASSERT(new_order.size() == material_map.size());

	geometry_aggregate.reorderTriangles(new_order);

	std::vector<cl_int> old_order = material_map;
	for (uint32_t i = 0; i < material_map.size(); ++i) {
		material_map[i] = old_order[new_order[i]];
	}

	std::vector<vec3>& slacks = geometry_aggregate.slacks;
	std::vector<vec3> old_slacks = slacks;
	for (uint32_t i = 0; i < slacks.size(); ++i) {
		slacks[i] = old_slacks[new_order[i]];
	}

    return true;	
}


uint32_t
Scene::process()
{

        std::vector<Object>::iterator obj;
        int32_t node_offset = 0;
        int32_t tri_offset = 0;

        // int32_t total_tris = 0;
        // int32_t total_verts = 0;
        // int32_t total_nodes = 0;

        /* Create bvh for each mesh that is referenced by a valid object*/
        /* Save the resulting structures in this->bvhs and this->mmaps (for material maps)*/
        for (obj = geometry.objects.begin(); obj < geometry.objects.end(); obj++) {
                /*If object is invalid, ignore*/
                if (!obj->is_valid())
                        continue;

                /*If object mesh is already processed, just add its root*/
                if (bvhs.find(obj->id) != bvhs.end()) {
                        BVHRoot bvh_root;
                        bvh_root.node = bvhs[obj->id].start_node;
                        bvh_root.tr   = obj->geom.getTransformMatrix();
                        bvh_roots.push_back(bvh_root);
                        continue;
                }

                
                BVH bvh;
                Mesh& mesh = mesh_atlas[obj->id];

		material_list.push_back(obj->mat);
		cl_int map_index = cl_int(material_list.size() - 1);
		material_map.resize(tri_offset + mesh.triangleCount(), map_index);

                if (!bvh.construct(mesh, node_offset, tri_offset))
                        return 1;

                bvh_order.push_back(obj->id);
                bvhs[obj->id] = bvh;
                mmaps[obj->id] = material_map;

                BVHRoot bvh_root;
                bvh_root.node = node_offset;
                bvh_root.tr   = obj->geom.getTransformMatrix();
                bvh_roots.push_back(bvh_root);

                // total_tris  += mesh_atlas[obj->id].triangleCount();
                // total_verts += mesh_atlas[obj->id].vertexCount();
                // total_nodes += bvh.nodeArraySize();


                node_offset += bvh.nodeArraySize();
                tri_offset  += mesh_atlas[obj->id].triangleCount();
        }
        /* At this point the bvh nodes are properly offset and the triangles 
           they point to as well.
           We still need to offset the vertex indices in the triangles of the meshes 
           when moving the data to the OpenCL device */
        

        return 0;
}





/*---------------------------- Scene Info methods ---------------------------*/

bool 
SceneInfo::initialize(Scene& scene, const CLInfo& cli)
{

	if (!cli.initialized)
		return false;
	clinfo = cli;

	/*---------------------- Move model data to OpenCL device -----------------*/

	Mesh& scene_mesh = scene.get_aggregate_mesh();
	BVH& scene_bvh   = scene.get_aggregate_bvh ();
	
	size_t triangles = scene_mesh.triangleCount();
	size_t vertices = scene_mesh.vertexCount();

	if (create_filled_cl_mem(clinfo,CL_MEM_READ_ONLY,
				 vertices * sizeof(Vertex),
				 scene_mesh.vertexArray(),
				 &vert_m))
		return false;


	if (create_filled_cl_mem(clinfo,CL_MEM_READ_ONLY,
				 triangles * 3 * sizeof(uint32_t),
				 scene_mesh.triangleArray(),
				 &index_m))
		return false;

	/*---------------------- Move material data to OpenCL device ------------*/

	std::vector<material_cl>& mat_list = scene.get_material_list();
	std::vector<cl_int>& mat_map = scene.get_material_map();
	
	void* mat_list_ptr = &(mat_list[0]);
	void* mat_map_ptr  = &(mat_map[0]);

	size_t mat_list_size = sizeof(material_cl) * mat_list.size();
	size_t mat_map_size  = sizeof(cl_int)      * mat_map.size();

	if (create_filled_cl_mem(clinfo,CL_MEM_READ_ONLY,
				 mat_list_size,
				 mat_list_ptr,
				 &mat_list_m))
		return false;

	if (create_filled_cl_mem(clinfo,CL_MEM_READ_ONLY,
				 mat_map_size,
				 mat_map_ptr,
				 &mat_map_m))
		return false;

	/*--------------------- move bvh to device memory ---------------------*/
	if (scene_bvh.nodeArraySize()) {
		if (create_filled_cl_mem(clinfo,CL_MEM_READ_ONLY,
					 scene_bvh.nodeArraySize() * sizeof(BVHNode),
					 scene_bvh.nodeArray(),
					 &bvh_m))
			return false;
	}
	/*-------------- Move initial light info to device memory---------------*/
	
	if (create_empty_cl_mem(clinfo,CL_MEM_READ_ONLY,
				 sizeof(lights_cl),
				 &lights_m))
		return false;


        /********************** !!STUB multi-BVH info ****************************/
        
        BVHRoot bvh_root;
        bvh_root.node = 0;
        if (create_filled_cl_mem(clinfo,CL_MEM_READ_ONLY,
                                sizeof(BVHRoot),
                                 &bvh_root,
                                 &bvh_roots_m))
                return false;
        
        bvh_roots_cant = 1;
        /***********************************************************************/
        

	return true;

}

bool 
SceneInfo::initialize_multi(Scene& scene, const CLInfo& cli)
{

	if (!cli.initialized)
		return false;
	clinfo = cli;

	/*--------------------- move bvhs to device memory ---------------------*/
        std::vector<BVHNode> bvh_nodes;
        for(std::vector<index_t>::iterator id = scene.bvh_order.begin();
            id < scene.bvh_order.end();
            id++) { 
                BVH& bvh = scene.bvhs[*id];
                bvh_nodes.insert(bvh_nodes.end(), bvh.m_nodes.begin(), bvh.m_nodes.end());
        }
        
	if (bvh_nodes.size()) {
		if (create_filled_cl_mem(clinfo,CL_MEM_READ_ONLY,
					 bvh_nodes.size() * sizeof(BVHNode),
					 &(bvh_nodes[0]),
					 &bvh_m))
			return false;
        }

	if (scene.bvh_roots.size()) {
		if (create_filled_cl_mem(clinfo,CL_MEM_READ_ONLY,
					 scene.bvh_roots.size() * sizeof(BVHRoot),
					 &(scene.bvh_roots[0]),
					 &bvh_roots_m))
			return false;
        }
        bvh_roots_cant = scene.bvh_roots.size();

	/*---------------------- Move model data to OpenCL device -----------------*/

        std::vector<Vertex> vertices;
        std::vector<Triangle> triangles;
        std::vector<vec3> slacks;

        int32_t vtx_count = 0;
        int32_t tri_count = 0;

        for (std::vector<index_t>::iterator id = scene.bvh_order.begin(); 
             id < scene.bvh_order.end(); 
             id++) { 

                Mesh& mesh = scene.mesh_atlas[*id];

                for (size_t i = 0; i < mesh.vertexCount(); ++i) {
                        vertices.push_back(mesh.vertex(i));
                }

                for (size_t i = 0; i < mesh.triangleCount(); ++i) {
                        Triangle t = mesh.triangle(i);
                        t.v[0] += vtx_count;
                        t.v[1] += vtx_count;
                        t.v[2] += vtx_count;
                        triangles.push_back(t);
                }


                vtx_count += mesh.vertexCount();
                tri_count += mesh.triangleCount();
        }

	if (create_filled_cl_mem(clinfo,CL_MEM_READ_ONLY,
				 vertices.size() * sizeof(Vertex),
				 &(vertices[0]),
				 &vert_m))
		return false;


	if (create_filled_cl_mem(clinfo,CL_MEM_READ_ONLY,
				 triangles.size() * sizeof(Triangle),
				 &(triangles[0]),
				 &index_m))
		return false;

	/*---------------------- Move material data to OpenCL device ------------*/

	std::vector<material_cl>& mat_list = scene.get_material_list();
	std::vector<cl_int>& mat_map = scene.get_material_map();
	
	void* mat_list_ptr = &(mat_list[0]);
	void* mat_map_ptr  = &(mat_map[0]);

	size_t mat_list_size = sizeof(material_cl) * mat_list.size();
	size_t mat_map_size  = sizeof(cl_int)      * mat_map.size();

	if (create_filled_cl_mem(clinfo,CL_MEM_READ_ONLY,
				 mat_list_size,
				 mat_list_ptr,
				 &mat_list_m))
		return false;

	if (create_filled_cl_mem(clinfo,CL_MEM_READ_ONLY,
				 mat_map_size,
				 mat_map_ptr,
				 &mat_map_m))
		return false;


	/*-------------- Move initial light info to device memory---------------*/
	
        if (create_empty_cl_mem(clinfo,CL_MEM_READ_ONLY,
				 sizeof(lights_cl),
				 &lights_m))
		return false;


	return true;

}

size_t 
SceneInfo::size()
{
	return cl_mem_size(vert_m) + cl_mem_size(index_m) + 
	       cl_mem_size(mat_map_m) + cl_mem_size(mat_list_m) + 
	       cl_mem_size(bvh_m);
}

bool 
SceneInfo::set_dir_light(const directional_light_cl& dl)
{
	cl_int err;
	lights.directional = dl;
	vec3 vdl = float3_to_vec3(dl.dir);
	vdl.normalize();
	lights.directional.dir = vec3_to_float3(vdl);

	err =  clEnqueueWriteBuffer(clinfo.command_queue,
				    lights_m, 
				    true, 
				    0,
				    sizeof(lights_cl),
				    &lights,
				    0,
				    NULL,
				    NULL);
	if (error_cl(err, "Directional light set"))
		return false;

	return true;
}

bool 
SceneInfo::set_ambient_light(const color_cl& c)
{
	cl_int err;
	lights.ambient = c;
	err =  clEnqueueWriteBuffer(clinfo.command_queue,
				    lights_m, 
				    true, 
				    0,
				    sizeof(lights_cl),
				    &lights,
				    0,
				    NULL,
				    NULL);
	if (error_cl(err, "Ambient light set"))
		return false;
	

	return true;
}

/*---------------------------- Misc functions ---------------------------*/

static mesh_id invalid_mesh_id(){return -1;}
// static object_id invalid_object_id(){return -1;}

bool is_valid_mesh_id(mesh_id id)
{
	return id>=0;
}

bool is_valid_object_id(object_id id)
{
	return id>=0;
}
