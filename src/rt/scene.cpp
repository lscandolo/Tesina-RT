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
	object_id oid = object_id(objects.size());
	objects.push_back(Object(mid));
	return oid;
}
	
void 
SceneGeometry::remove_object(object_id id)
{
	if (id < (object_id)objects.size())
		objects[id].set_mesh_id(invalid_mesh_id());
}

Object& 
SceneGeometry::object(object_id id)
{
	ASSERT(is_valid_object_id(id) && id < objects.size());
	return objects[id];
}

/*-------------------------- Scene Methods ------------------------------*/

Scene::Scene()
{
        m_initialized = false;
        m_aggregate_mesh_built = false;
        m_aggregate_bvh_built = false;
        m_bvhs_built = false;
}

int32_t 
Scene::initialize(CLInfo& clinfo)
{
        if (device.initialize(clinfo))
                return -1;

        vert_id = device.new_memory();
        tri_id = device.new_memory();
        mat_map_id = device.new_memory();
        mat_list_id = device.new_memory();
        bvh_id = device.new_memory();
        lights_id = device.new_memory();
        bvh_roots_id = device.new_memory();

	/*-------------- Move initial light info to device memory---------------*/
        DeviceMemory& light_mem = device.memory(lights_id);
        if (light_mem.initialize(sizeof(lights_cl), READ_ONLY_MEMORY))
                return -1;

        m_initialized = true;
        return 0;
}

bool 
Scene::valid_aggregate()
{
        return m_aggregate_mesh_built && m_aggregate_bvh_built;
}

bool
Scene::valid_bvhs()
{
        return m_bvhs_built;
}

bool 
Scene::valid()
{
        return device.good() && m_initialized && 
                (valid_aggregate() || valid_bvhs());
}

int32_t 
Scene::transfer_aggregate_mesh_to_device()
{
        if (!m_initialized || !m_aggregate_mesh_built)
                return -1;

	/*---------------------- Move model data to device -----------------*/

        size_t triangle_count = aggregate_mesh.triangleCount();
        size_t vertex_count = aggregate_mesh.vertexCount();

        DeviceMemory& vertex_mem = device.memory(vert_id);
        size_t vertex_mem_size = vertex_count * sizeof(Vertex);
        const void*  vertex_ptr = aggregate_mesh.vertexArray();
        if (vertex_mem.initialize(vertex_mem_size, vertex_ptr, READ_ONLY_MEMORY))
                    return -1;

        DeviceMemory& triangle_mem = device.memory(tri_id);
        size_t triangle_mem_size = triangle_count * sizeof(Triangle);
        const void*  triangle_ptr = aggregate_mesh.triangleArray();
        if (triangle_mem.initialize(triangle_mem_size, triangle_ptr, READ_ONLY_MEMORY))
                    return -1;

	/*---------------------- Move material data to device ------------*/

        DeviceMemory& mat_list_mem = device.memory(mat_list_id);
        const void* mat_list_ptr = &(material_list[0]);
        size_t mat_list_size = sizeof(material_cl) * material_list.size();
        if (mat_list_mem.initialize(mat_list_size, mat_list_ptr, READ_ONLY_MEMORY))
                return -1;

        DeviceMemory& mat_map_mem = device.memory(mat_map_id);
        const void* mat_map_ptr = &(material_map[0]);
        size_t mat_map_size = sizeof(cl_int) * material_map.size();
        if (mat_map_mem.initialize(mat_map_size, mat_map_ptr, READ_ONLY_MEMORY))
                return -1;

        /********************** Single bvh root ****************************/
        
        BVHRoot root;
        root.node = 0;
        root.tr = root.trInv = mat4x4_to_cl_sqmat4(scaleMatrix4x4(1.f));
        bvh_roots.clear();
        bvh_roots.push_back(root);
        DeviceMemory& bvh_roots_mem = device.memory(bvh_roots_id);
        const void* bvh_roots_ptr = &root;
        size_t bvh_roots_size = sizeof(root);
        if (bvh_roots_mem.initialize(bvh_roots_size, bvh_roots_ptr, READ_ONLY_MEMORY))
                return -1;
        
        return 0;

}

int32_t 
Scene::transfer_aggregate_bvh_to_device()
{
        if (!m_initialized || !m_aggregate_bvh_built)
                return -1;

	/*--------------------- Move bvh to device memory ---------------------*/

        DeviceMemory& bvh_mem = device.memory(bvh_id);
        const void* bvh_ptr = aggregate_bvh.nodeArray();
        size_t bvh_size = aggregate_bvh.nodeArraySize() * sizeof(BVHNode);

        if (bvh_mem.initialize(bvh_size, bvh_ptr, READ_ONLY_MEMORY))
            return -1;

        /********************** !!STUB multi-BVH info ****************************/
        
        BVHRoot root;
        root.node = 0;
        root.tr = root.trInv = mat4x4_to_cl_sqmat4(scaleMatrix4x4(1.f));
        bvh_roots.clear();
        bvh_roots.push_back(root);
        DeviceMemory& bvh_roots_mem = device.memory(bvh_roots_id);
        const void* bvh_roots_ptr = &root;
        size_t bvh_roots_size = sizeof(root);
        if (bvh_roots_mem.initialize(bvh_roots_size, bvh_roots_ptr, READ_ONLY_MEMORY))
                return -1;
        
        return 0;
}


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

int32_t 
Scene::create_aggregate_mesh()
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

		size_t base_vertex = aggregate_mesh.vertexCount();
		Mesh& mesh = mesh_atlas[m_id];
		GeometricProperties g = obj.geom;

		material_list.push_back(obj.mat);
		cl_int map_index = cl_int(material_list.size() - 1);
		material_map.resize(base_triangle + mesh.triangleCount(), map_index);
		
		for (uint32_t v = 0; v < mesh.vertexCount(); ++v) {
			Vertex vertex = mesh.vertex(v);
			g.transform(vertex);
			aggregate_mesh.vertices.push_back(vertex);
		}
		for (uint32_t t = 0; t < mesh.triangleCount(); ++t) {
			Triangle tri = mesh.triangle(t);
            tri.v[0] += vtx_id(base_vertex);
			tri.v[1] += vtx_id(base_vertex);
			tri.v[2] += vtx_id(base_vertex);
			aggregate_mesh.triangles.push_back(tri);
			aggregate_mesh.slacks.push_back(obj.slack);
		}
		base_triangle += uint32_t(mesh.triangleCount());
		
	}
        
        m_aggregate_mesh_built = true;
	return 0;
}

int32_t 
Scene::create_aggregate_bvh()
{
        if (!aggregate_bvh.construct_and_map(aggregate_mesh, material_map))
                return -1;
        m_aggregate_bvh_built = true;
        return 0;
}

bool 
Scene::reorderTriangles(const std::vector<uint32_t>& new_order) {
	ASSERT(new_order.size() == aggregate_mesh.triangles.size());
	ASSERT(new_order.size() == material_map.size());

	aggregate_mesh.reorderTriangles(new_order);

	std::vector<cl_int> old_order = material_map;
	for (uint32_t i = 0; i < material_map.size(); ++i) {
		material_map[i] = old_order[new_order[i]];
	}

	std::vector<vec3>& slacks = aggregate_mesh.slacks;
	std::vector<vec3> old_slacks = slacks;
	for (uint32_t i = 0; i < slacks.size(); ++i) {
		slacks[i] = old_slacks[new_order[i]];
	}

    return true;	
}

uint32_t 
Scene::update_bvh_roots()
{
        uint32_t root_idx = 0;

	/*--------------------- Update roots from object info ---------------------*/
        for (uint32_t obj_idx = 0; obj_idx < geometry.objects.size(); ++obj_idx) {
                Object& obj = geometry.objects[obj_idx];
                if (!obj.is_valid())
                        continue;
                
                BVHRoot& bvh_root = bvh_roots[root_idx];
                const mat4x4& tr = obj.geom.getTransformMatrix();
                const mat4x4& trInv = obj.geom.getTransformMatrixInv();
                bvh_root.tr = mat4x4_to_cl_sqmat4(tr);
                bvh_root.trInv = mat4x4_to_cl_sqmat4(trInv);

                ++root_idx;
        }
	/*--------------------- Move bvh roots to device memory ---------------------*/

        DeviceMemory& bvh_roots_mem = device.memory(bvh_roots_id);
        size_t bvh_roots_size = bvh_roots.size() * sizeof(BVHRoot);
        const void* bvh_roots_ptr = &(bvh_roots[0]);
	if (bvh_roots.size()) {
                if (bvh_roots_mem.initialize(bvh_roots_size, bvh_roots_ptr, 
                                             READ_ONLY_MEMORY))
			return -1;
        }
        
        
        return 0;
}

int32_t 
Scene::create_bvhs()
{
        if (!m_initialized)
                return -1;

        std::vector<Object>::iterator obj;
        int32_t node_offset = 0;
        int32_t tri_offset = 0;

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

                        const mat4x4& tr = obj->geom.getTransformMatrix();
                        const mat4x4& trInv = obj->geom.getTransformMatrixInv();
                        bvh_root.tr = mat4x4_to_cl_sqmat4(tr);
                        bvh_root.trInv = mat4x4_to_cl_sqmat4(trInv);
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
                const mat4x4& tr = obj->geom.getTransformMatrix();
                const mat4x4& trInv = obj->geom.getTransformMatrixInv();
                bvh_root.tr = mat4x4_to_cl_sqmat4(tr);
                bvh_root.trInv = mat4x4_to_cl_sqmat4(trInv);
                bvh_roots.push_back(bvh_root);

                node_offset += bvh.nodeArraySize();
                tri_offset  += mesh_atlas[obj->id].triangleCount();
        }
        /* At this point the bvh nodes are properly offset and the triangles 
           they point to as well.
           We still need to offset the vertex indices in the triangles of the meshes 
           when moving the data to the OpenCL device */
        
        m_bvhs_built = true;
        return 0;
        
}

int32_t
Scene::transfer_meshes_to_device()
{
	if (!m_initialized)
		return -1;

        typedef std::vector<mesh_id>::iterator index_iterator_t;

	/*---------------------- Move model data to OpenCL device -----------------*/

        std::vector<Vertex> vertices;
        std::vector<Triangle> triangles;

        int32_t vtx_count = 0;
        int32_t tri_count = 0;

        /* First put them adjacent in order to move them 
         TODO: Copy them individually */
        for (index_iterator_t id = bvh_order.begin(); id < bvh_order.end(); id++) { 

                Mesh& mesh = mesh_atlas[*id];

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

        DeviceMemory& vert_mem = device.memory(vert_id);
        size_t vert_size = vertices.size() * sizeof(Vertex);
        const void* vert_ptr = &(vertices[0]);
        if (vert_mem.initialize(vert_size, vert_ptr, READ_ONLY_MEMORY))
		return -1;

        DeviceMemory& triangle_mem = device.memory(tri_id);
        size_t triangle_size = triangles.size() * sizeof(Triangle);
        const void* triangle_ptr = &(triangles[0]);
        if (triangle_mem.initialize(triangle_size, triangle_ptr, READ_ONLY_MEMORY))
		return -1;

	/*---------------------- Move material data to device ------------*/

        DeviceMemory& mat_list_mem = device.memory(mat_list_id);
        const void* mat_list_ptr = &(material_list[0]);
        size_t mat_list_size = sizeof(material_cl) * material_list.size();
        if (mat_list_mem.initialize(mat_list_size, mat_list_ptr, READ_ONLY_MEMORY))
                return -1;

        DeviceMemory& mat_map_mem = device.memory(mat_map_id);
        const void* mat_map_ptr = &(material_map[0]);
        size_t mat_map_size = sizeof(cl_int) * material_map.size();
        if (mat_map_mem.initialize(mat_map_size, mat_map_ptr, READ_ONLY_MEMORY))
                return -1;

        return 0;

}

int32_t
Scene::transfer_bvhs_to_device()
{
	if (!m_initialized || !m_bvhs_built)
		return -1;

        typedef std::vector<mesh_id>::iterator index_iterator_t;

	/*--------------------- Move bvh nodes to device memory ---------------------*/
        /* First put them adjacent in order to move them 
         TODO: Copy them individually */
        std::vector<BVHNode> bvh_nodes;
        for(index_iterator_t id = bvh_order.begin(); id < bvh_order.end(); id++) {
                BVH& bvh = bvhs[*id];
                bvh_nodes.insert(bvh_nodes.end(), bvh.m_nodes.begin(), bvh.m_nodes.end());
        }
        
        DeviceMemory& bvh_mem = device.memory(bvh_id);
        size_t bvh_size = bvh_nodes.size() * sizeof(BVHNode);
        const void* bvh_ptr = &(bvh_nodes[0]);
	if (bvh_nodes.size()) {
                if (bvh_mem.initialize(bvh_size, bvh_ptr, READ_ONLY_MEMORY))
			return -1;
        }

	/*--------------------- Move bvh roots to device memory ---------------------*/

        DeviceMemory& bvh_roots_mem = device.memory(bvh_roots_id);
        size_t bvh_roots_size = bvh_roots.size() * sizeof(BVHRoot);
        const void* bvh_roots_ptr = &(bvh_roots[0]);
	if (bvh_roots.size()) {
                if (bvh_roots_mem.initialize(bvh_roots_size, bvh_roots_ptr, 
                                             READ_ONLY_MEMORY))
			return -1;
        }
        return 0;
}

Mesh&
Scene::get_mesh(mesh_id mid)
{
        ASSERT(mid < mesh_atlas.size() && mid >= 0);
        return mesh_atlas[mid];
}

BVH&
Scene::get_object_bvh(object_id oid)
{
        ASSERT(bvhs.find(oid) != bvhs.end());
        return bvhs[oid];
}

int32_t 
Scene::update_mesh_vertices(mesh_id mid)
{
	if (!m_initialized || bvhs.find(mid) == bvhs.end())
            return -1;

    typedef std::vector<mesh_id>::iterator mesh_iterator_t;

	/*---------------------- Move model data to OpenCL device -----------------*/

    std::vector<Vertex> vertices;
    std::vector<Triangle> triangles;

    int32_t vtx_count = 0;
    int32_t tri_count = 0;

    size_t vertex_offset = 0;

    for (mesh_iterator_t it = bvh_order.begin(); it < bvh_order.end(); it++) { 

            if (*it != mid) {
                    Mesh& it_mesh = mesh_atlas[*it];
                    vertex_offset += it_mesh.vertexCount();
            }
            else {
                    Mesh& mesh = mesh_atlas[mid];
                    if (vertex_mem().write(mesh.vertexCount() *sizeof(Vertex),
                                         mesh.vertexArray(),
                                         vertex_offset))
                                         return -1;
                    return 0;
            }
    }
    return -1;
}   

int32_t
Scene::set_dir_light(const directional_light_cl& dl)
{
        if (!m_initialized)
                return -1;

	lights.directional = dl;

	vec3 vdl = float3_to_vec3(dl.dir);
	vdl.normalize();
	lights.directional.dir = vec3_to_float3(vdl);

        DeviceMemory& light_mem = device.memory(lights_id);

        return light_mem.write(sizeof(lights_cl), &lights);
}

int32_t
Scene::set_ambient_light(const color_cl& c)
{
        if (!m_initialized)
                return -1;

	lights.ambient = c;

        DeviceMemory& light_mem = device.memory(lights_id);

        return light_mem.write(sizeof(lights_cl), &lights);
}

DeviceMemory& 
Scene::vertex_mem()
{
        return device.memory(vert_id);
}

DeviceMemory& 
Scene::triangle_mem()
{
        return device.memory(tri_id);
}

DeviceMemory& 
Scene::material_list_mem()
{
        return device.memory(mat_list_id);
}

DeviceMemory& 
Scene::material_map_mem()
{
        return device.memory(mat_map_id);
}

DeviceMemory&
Scene::bvh_nodes_mem()
{
        return device.memory(bvh_id);
}

DeviceMemory&
Scene::bvh_roots_mem()
{
        return device.memory(bvh_roots_id);
}

DeviceMemory&
Scene::lights_mem()
{
        return device.memory(lights_id);
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
