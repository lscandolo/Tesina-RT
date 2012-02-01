
#include <rt/scene.hpp>

static mesh_id invalid_mesh_id();
// static object_id invalid_object_id();

/*-------------------- Object Methods -------------------------------*/

Object::Object(mesh_id _id) : id(_id){}

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
SceneGeometry::add_object(Object mi)
{
	uint32_t id = objects.size();
	objects.push_back(mi);
	return id;
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
	return mesh_atlas.size() - 1;
}

uint32_t 
Scene::create_aggregate()
{
	uint32_t base_triangle = 0;
	material_list.clear();
	material_map.clear();

	for (uint32_t i = 0; i < geometry.objects.size(); ++i) {
		Object obj = geometry.objects[i];
		if (!obj.is_valid())
			continue;

		mesh_id m_id = obj.get_mesh_id();
		uint32_t base_vertex = geometry_aggregate.vertexCount();
		Mesh& mesh = mesh_atlas[m_id];
		GeometricProperties g = obj.geom;

		ASSERT(m_id < mesh_atlas.size());

		material_list.push_back(obj.mat);
		cl_int map_index = material_list.size() - 1;
		material_map.resize(base_triangle + mesh.triangleCount(), map_index);

		for (uint32_t v = 0; v < mesh.vertexCount(); ++v) {
			Vertex vertex = mesh.vertex(v);
			g.transform(vertex);
			geometry_aggregate.vertices.push_back(vertex);
		}
		for (uint32_t t = 0; t < mesh.triangleCount(); ++t) {
			Triangle tri = mesh.triangle(t);
			tri.v[0] += base_vertex;
			tri.v[1] += base_vertex;
			tri.v[2] += base_vertex;
			geometry_aggregate.triangles.push_back(tri);
		}
		base_triangle += mesh.triangleCount();
		
	}
	return 0;
}

uint32_t 
Scene::create_bvh(){
	if (!bvh.construct_and_map(geometry_aggregate, material_map))
		return 1;
	return 0;
}

/*---------------------------- Scene Info methods ---------------------------*/

bool 
SceneInfo::initialize(Scene& scene, const CLInfo& clinfo)
{

	/*---------------------- Move model data to OpenCL device -----------------*/

	Mesh& scene_mesh = scene.get_aggregate_mesh();
	BVH& scene_bvh   = scene.get_aggregate_bvh ();
	
	int triangles = scene_mesh.triangleCount();
	int vertices = scene_mesh.vertexCount();

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

	/*--------------------- Move bvh to device memory ---------------------*/
	if (create_filled_cl_mem(clinfo,CL_MEM_READ_ONLY,
				 scene_bvh.nodeArraySize() * sizeof(BVHNode),
				 scene_bvh.nodeArray(),
				 &bvh_m))
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
