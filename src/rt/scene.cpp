#include <rt/scene.hpp>

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
	for (uint32_t i = 0; i < geometry.objects.size(); ++i) {
		Object obj = geometry.objects[i];
		if (!obj.is_valid())
			continue;
		mesh_id m_id = obj.get_mesh_id();
		uint32_t base_vertex = geometry_aggregate.vertexCount();
		Mesh& mesh = mesh_atlas[m_id];
		GeometricProperties g = obj.geom;

		ASSERT(m_id < mesh_atlas.size());

		for (uint32_t v = 0; v < mesh.vertexCount(); ++v) {
			Vertex vertex = mesh.vertex(v);
			g.transform(vertex);
			/*!! Must finish vertex transform method!! */
			geometry_aggregate.vertices.push_back(vertex);
		}
		for (uint32_t t = 0; t < mesh.triangleCount(); ++t) {
			Triangle tri = mesh.triangle(t);
			tri.v[0] += base_vertex;
			tri.v[1] += base_vertex;
			tri.v[2] += base_vertex;
			geometry_aggregate.triangles.push_back(tri);
		}
	}
	return 0;
}

uint32_t 
Scene::create_bvh(){
	if (!bvh.construct(geometry_aggregate))
		return 1;
	return 0;
}

/*---------------------------- Misc functions ---------------------------*/

static mesh_id invalid_mesh_id(){return -1;}
static object_id invalid_object_id(){return -1;}


bool is_valid_mesh_id(mesh_id id)
{
	return id>=0;
}

bool is_valid_object_id(object_id id)
{
	return id>=0;
}
