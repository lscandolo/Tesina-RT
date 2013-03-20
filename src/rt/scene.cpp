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


/*-------------------------- Scene Methods ------------------------------*/


Scene::Scene()
{
        m_initialized = false;
        m_aggregate_mesh_built = false;
        m_aggregate_bvh_built = false;
        m_aggregate_kdt_built = false;
        m_bvhs_built = false;
        m_aggregate_bvh_transfered = false;
        m_aggregate_kdt_transfered = false;
        m_bvhs_transfered = false;
        m_accelerator_type = BVH_ACCELERATOR;
}

int32_t 
Scene::initialize()
{
        DeviceInterface& device = *DeviceInterface::instance();
        if (!device.good())
                return -1;

        if (texture_atlas.initialize())
                return -1;

        vert_id = device.new_memory();
        idx_id = device.new_memory();
        mat_map_id = device.new_memory();
        mat_list_id = device.new_memory();
        bvh_id = device.new_memory();
        lights_id = device.new_memory();
        bvh_roots_id = device.new_memory();
        kdt_nodes_id = device.new_memory();
        kdt_leaf_tris_id = device.new_memory();

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
        return m_aggregate_mesh_built && (m_aggregate_bvh_built || m_aggregate_kdt_built);
}

bool
Scene::valid_bvhs()
{
        return m_bvhs_built;
}

bool 
Scene::valid()
{
        DeviceInterface& device = *DeviceInterface::instance();
        return device.good() && m_initialized && 
                (valid_aggregate() || valid_bvhs());
}

bool 
Scene::ready()
{
        DeviceInterface& device = *DeviceInterface::instance();
        if (!device.good() || !m_initialized)
                return false;

        switch (m_accelerator_type) {
        case (KDTREE_ACCELERATOR):
                return m_aggregate_kdt_built && m_aggregate_kdt_transfered;
        case (BVH_ACCELERATOR):
                return (m_aggregate_bvh_built && m_aggregate_bvh_transfered) ||
                       (m_bvhs_built && m_bvhs_transfered);
        default:
                return -1;
        }
}

void
Scene::set_accelerator_type(AcceleratorType type)
{
        if (type == KDTREE_ACCELERATOR || type == BVH_ACCELERATOR)
                m_accelerator_type = type;
}

int32_t 
Scene::transfer_aggregate_mesh_to_device()
{
        if (!m_initialized || !m_aggregate_mesh_built)
                return -1;

	/*---------------------- Move model data to device -----------------*/

        DeviceInterface& device = *DeviceInterface::instance();
        size_t triangle_count = aggregate_mesh.triangleCount();
        size_t vertex_count = aggregate_mesh.vertexCount();

        DeviceMemory& vertex_mem = device.memory(vert_id);
        size_t vertex_mem_size = vertex_count * sizeof(Vertex);
        const void*  vertex_ptr = aggregate_mesh.vertexArray();
        if (!vertex_mem.valid()) {
                if (vertex_mem.initialize(vertex_mem_size, vertex_ptr, READ_ONLY_MEMORY))
                        return -1;
        } else {
                if (vertex_mem.resize(vertex_mem_size))
                        return -1;
                if (vertex_mem.write(vertex_mem_size, vertex_ptr))
                        return -1;
        }

        DeviceMemory& index_mem = device.memory(idx_id);
        size_t index_mem_size = triangle_count * sizeof(Triangle);
        const void*  index_ptr = aggregate_mesh.triangleArray();
        if (!index_mem.valid()) {
                if (index_mem.initialize(index_mem_size, index_ptr, READ_ONLY_MEMORY))
                        return -1;
        } else {
                if (index_mem.resize(index_mem_size))
                        return -1;
                if (index_mem.write(index_mem_size, index_ptr))
                        return -1;
        }

	/*---------------------- Move material data to device ------------*/

        DeviceMemory& mat_list_mem = device.memory(mat_list_id);
        const void* mat_list_ptr = &(material_list[0]);
        size_t mat_list_size = sizeof(material_cl) * material_list.size();
        if (!mat_list_mem.valid()) {
                if (mat_list_mem.initialize(mat_list_size, mat_list_ptr, READ_ONLY_MEMORY))
                        return -1;
        } else {
                if (mat_list_mem.resize(mat_list_size))
                        return -1;
                if (mat_list_mem.write(mat_list_size, mat_list_ptr))
                        return -1;
        }

        DeviceMemory& mat_map_mem = device.memory(mat_map_id);
        const void* mat_map_ptr = &(material_map[0]);
        size_t mat_map_size = sizeof(cl_int) * material_map.size();
        if (!mat_map_mem.valid()) {
                if (mat_map_mem.initialize(mat_map_size, mat_map_ptr, READ_ONLY_MEMORY))
                        return -1;
        } else {
                if (mat_map_mem.resize(mat_map_size))
                        return -1;
                if (mat_map_mem.write(mat_map_size, mat_map_ptr))
                        return -1;
        }

        /*------------ Attempt to move single bvh_root data to device ------------*/
        DeviceMemory& bvh_roots_mem = device.memory(bvh_roots_id);
        if (!bvh_roots_mem.valid()) {

                BVHRoot root;
                root.node = 0;
                root.tr = root.trInv = mat4x4_to_cl_sqmat4(scaleMatrix4x4(1.f));
                bvh_roots.clear();
                bvh_roots.push_back(root);
                const void* bvh_roots_ptr = &root;
                size_t bvh_roots_size = sizeof(root);
                if (bvh_roots_mem.initialize(bvh_roots_size, 
                                             bvh_roots_ptr, 
                                             READ_ONLY_MEMORY))
                        return -1;
        }
        

        return 0;
}

int32_t 
Scene::transfer_aggregate_bvh_to_device()
{
        if (!m_initialized || !m_aggregate_bvh_built)
                return -1;

	/*--------------------- Move bvh to device memory ---------------------*/

        DeviceInterface& device = *DeviceInterface::instance();
        DeviceMemory& bvh_mem = device.memory(bvh_id);
        const void* bvh_ptr = aggregate_bvh.nodeArray();
        size_t bvh_size = aggregate_bvh.nodeArraySize() * sizeof(BVHNode);

        if (bvh_mem.initialize(bvh_size, bvh_ptr, READ_ONLY_MEMORY))
            return -1;

        /*------------ Attempt to move single bvh_root data to device ------------*/
        
        DeviceMemory& bvh_roots_mem = device.memory(bvh_roots_id);
        if (!bvh_roots_mem.valid()) {

                BVHRoot root;
                root.node = 0;
                root.tr = root.trInv = mat4x4_to_cl_sqmat4(scaleMatrix4x4(1.f));
                bvh_roots.clear();
                bvh_roots.push_back(root);
                const void* bvh_roots_ptr = &root;
                size_t bvh_roots_size = sizeof(root);
                if (bvh_roots_mem.initialize(bvh_roots_size, 
                                             bvh_roots_ptr, 
                                             READ_ONLY_MEMORY))
                        return -1;
        }
        
        m_aggregate_bvh_transfered = true;

        return 0;
}

int32_t 
Scene::transfer_aggregate_kdtree_to_device()
{
        if (!m_initialized || !m_aggregate_kdt_built)
                return -1;

	/*--------------------- Move kdt to device memory ---------------------*/

        DeviceInterface& device = *DeviceInterface::instance();
        DeviceMemory& kdt_nodes_mem = device.memory(kdt_nodes_id);
        DeviceMemory& kdt_leaf_tris_mem = device.memory(kdt_leaf_tris_id);

        const void* kdt_nodes_ptr = aggregate_kdtree.node_array();
        size_t kdt_nodes_size = aggregate_kdtree.node_array_size() * sizeof(KDTNode);

        const void* kdt_leaf_tris_ptr = aggregate_kdtree.leaf_tris_array();
        size_t kdt_leaf_tris_size = aggregate_kdtree.leaf_tris_array_size()*sizeof(cl_uint);

        if (kdt_nodes_mem.initialize(kdt_nodes_size, 
                                     kdt_nodes_ptr, 
                                     READ_ONLY_MEMORY))
            return -1;

        if (kdt_leaf_tris_mem.initialize(kdt_leaf_tris_size, 
                                         kdt_leaf_tris_ptr, 
                                         READ_ONLY_MEMORY))
            return -1;

        m_aggregate_kdt_transfered = true;

        return 0;
}

std::vector<mesh_id>
Scene::load_obj_file(std::string filename) 
{
        std::vector<mesh_id> mesh_ids;
	ModelOBJ obj;
	if (!obj.import(filename.c_str())){
		return mesh_ids;
	}

        std::vector<Mesh> meshes;
        // std::vector<material_cl> materials;

        obj.get_meshes(meshes);

        for (uint32_t i = 0; i < meshes.size(); ++i) {
                mesh_atlas.push_back(meshes[i]);
                uint32_t mesh_id = mesh_atlas.size() - 1;
                mesh_ids.push_back(mesh_id);
        }
        return mesh_ids;
}

void
Scene::load_obj_file_and_make_objs(std::string filename) 
{
	ModelOBJ obj;
	if (!obj.import(filename.c_str())){
		return;
	}

        std::vector<Mesh> meshes;
        // std::vector<material_cl> materials;

        obj.get_meshes(meshes);

        for (uint32_t i = 0; i < meshes.size(); ++i) {
                mesh_atlas.push_back(meshes[i]);
                uint32_t mesh_id = mesh_atlas.size() - 1;
                object_id obj_id = add_object(mesh_id);
                Object& obj = object(obj_id);
                MeshMaterial& mesh_mat = meshes[i].original_materials[0];
                obj.mat = mesh_mat.material;
                obj.mat.texture = texture_atlas.load_texture(mesh_mat.texture_filename);
        }
}

mesh_id
Scene::load_obj_file_as_aggregate(std::string filename)
{
	ModelOBJ obj;
	if (!obj.import(filename.c_str())){
		return invalid_mesh_id();
	}

	Mesh mesh;
	obj.get_aggregate_mesh(&mesh);
	mesh_atlas.push_back(mesh);
	return uint32_t(mesh_atlas.size() - 1);
}

/*------------------------- Scene (Geometry) Methods ---------------------*/

object_id 
Scene::add_object(mesh_id mid)
{
	object_id oid = object_id(objects.size());
	objects.push_back(Object(mid));
        if (mid >= 0 && mid < (int32_t)mesh_atlas.size()) {
                material_cl& mat = objects[oid].mat;
                MeshMaterial& mesh_mat = mesh_atlas[mid].original_materials[0];
                mat = mesh_mat.material;
                mat.texture = texture_atlas.load_texture(mesh_mat.texture_filename);
        }
	return oid;
}

std::vector<object_id>
Scene::add_objects(std::vector<mesh_id> mids)
{
        std::vector<object_id> object_ids;
        for (size_t i = 0; i < mids.size(); ++i)  
                object_ids.push_back(add_object(mids[i]));
        return object_ids;
}
	
void 
Scene::remove_object(object_id id)
{
	if (id < (object_id)objects.size())
		objects[id].set_mesh_id(invalid_mesh_id());
}

Object& 
Scene::object(object_id id)
{
	ASSERT(is_valid_object_id(id) && id < objects.size());
	return objects[id];
}

int32_t 
Scene::create_aggregate_mesh()
{
	uint32_t base_triangle = 0;
	material_list.clear();
	material_map.clear();
    aggregate_mesh = Mesh();

	for (uint32_t i = 0; i < objects.size(); ++i) {
		Object& obj = objects[i];

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
Scene::create_aggregate_accelerator()
{
        switch (m_accelerator_type) {
        case (KDTREE_ACCELERATOR):
                return create_aggregate_kdtree();
        case (BVH_ACCELERATOR):
                return create_aggregate_bvh();
        default:
                return -1;
        }
}

int32_t 
Scene::create_aggregate_bvh()
{
        if (aggregate_bvh.construct_and_map(aggregate_mesh, material_map))
                return -1;
        m_aggregate_bvh_built = true;
        return 0;
}

int32_t 
Scene::create_aggregate_kdtree()
{
        if (aggregate_kdtree.construct(aggregate_mesh, &aggregate_bbox))
                return -1;
        m_aggregate_kdt_built = true;
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
        if (!m_bvhs_built)
                return -1;
        uint32_t root_idx = 0;
        DeviceInterface& device = *DeviceInterface::instance();

	/*--------------------- Update roots from object info ---------------------*/
        for (uint32_t obj_idx = 0; obj_idx < objects.size(); ++obj_idx) {
                Object& obj = objects[obj_idx];
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
        if (!bvh_roots_mem.valid()) return -1;
        size_t bvh_roots_size = bvh_roots.size() * sizeof(BVHRoot);
        const void* bvh_roots_ptr = &(bvh_roots[0]);
	if (bvh_roots.size()) {
                if (bvh_roots_mem.write(bvh_roots_size, bvh_roots_ptr,0))
                        return -1;
        }
        return 0;
}

size_t
Scene::root_count(){
        if (m_aggregate_mesh_built)
                return 1;
        else
                return object_count();
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
        for (obj = objects.begin(); obj < objects.end(); obj++) {
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

                if (bvh.construct(mesh, node_offset, tri_offset)) {
                        return -1;
                }

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
        DeviceInterface& device = *DeviceInterface::instance();

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

        DeviceMemory& triangle_mem = device.memory(idx_id);
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
Scene::transfer_aggregate_accelerator_to_device()
{
        switch (m_accelerator_type) {
        case (KDTREE_ACCELERATOR):
                return transfer_aggregate_kdtree_to_device();
        case (BVH_ACCELERATOR):
                return transfer_aggregate_bvh_to_device();
        default:
                return -1;
        }
}

int32_t
Scene::transfer_bvhs_to_device()
{
	if (!m_initialized || !m_bvhs_built)
		return -1;

        typedef std::vector<mesh_id>::iterator index_iterator_t;
        DeviceInterface& device = *DeviceInterface::instance();

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
    m_bvhs_transfered = true;
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

//int32_t 
//Scene::update_object_vertices(mesh_id mid)
//{
//	if (!m_initialized || !m_aggregate_mesh_built)
//                return -1;
//
//	uint32_t base_triangle = 0;
//	material_list.clear();
//	material_map.clear();
//    aggregate_mesh = Mesh();
//
//	for (uint32_t i = 0; i < objects.size(); ++i) {
//		Object& obj = objects[i];
//
//		if (!obj.is_valid())
//			continue;
//
//		mesh_id m_id = obj.get_mesh_id();
//
//		ASSERT(m_id >= 0 && m_id < mesh_atlas.size());
//
//		size_t base_vertex = aggregate_mesh.vertexCount();
//		Mesh& mesh = mesh_atlas[m_id];
//		GeometricProperties g = obj.geom;
//
//		material_list.push_back(obj.mat);
//		cl_int map_index = cl_int(material_list.size() - 1);
//		material_map.resize(base_triangle + mesh.triangleCount(), map_index);
//		
//		for (uint32_t v = 0; v < mesh.vertexCount(); ++v) {
//			Vertex vertex = mesh.vertex(v);
//			g.transform(vertex);
//			aggregate_mesh.vertices.push_back(vertex);
//		}
//		for (uint32_t t = 0; t < mesh.triangleCount(); ++t) {
//			Triangle tri = mesh.triangle(t);
//                        tri.v[0] += vtx_id(base_vertex);
//			tri.v[1] += vtx_id(base_vertex);
//			tri.v[2] += vtx_id(base_vertex);
//			aggregate_mesh.triangles.push_back(tri);
//			aggregate_mesh.slacks.push_back(obj.slack);
//		}
//		base_triangle += uint32_t(mesh.triangleCount());
//                
//	}
//        
//        m_aggregate_mesh_built = true;
//	return 0;
//
//}

int32_t
Scene::update_mesh_vertices(mesh_id mid)
{

        if (m_bvhs_built) {

                if (!m_initialized || bvhs.find(mid) == bvhs.end())
                        return -1;

                typedef std::vector<mesh_id>::iterator mesh_iterator_t;

                /*---------------------- Move model data to OpenCL device -----------------*/

                std::vector<Vertex> vertices;
                std::vector<Triangle> triangles;

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
        } else if (m_aggregate_mesh_built) {

                DeviceInterface& device = *DeviceInterface::instance();
                uint32_t base_vertex = 0;

                for (uint32_t i = 0; i < objects.size(); ++i) {
                        Object& obj = objects[i];
                        
                        if (!obj.is_valid())
                                continue;

                        mesh_id m_id = obj.get_mesh_id();
                        ASSERT(m_id >= 0 && m_id < mesh_atlas.size());
                        Mesh& mesh = mesh_atlas[m_id];
                        if (m_id == mid) {
                                Mesh& mesh = mesh_atlas[m_id];
                                GeometricProperties g = obj.geom;
                                size_t vertex_count = mesh.vertexCount();
                                for (uint32_t v = 0; v < vertex_count; ++v) {
                                        Vertex vertex = mesh.vertex(v);
                                        g.transform(vertex);
                                        aggregate_mesh.vertices[base_vertex+v] = vertex;
                                }
                                DeviceMemory& vertex_mem = device.memory(vert_id);
                                size_t write_size = vertex_count * sizeof(Vertex);
                                size_t write_offset = base_vertex * sizeof(Vertex);
                                const void*  vertex_ptr = aggregate_mesh.vertexArray()+base_vertex;
                                if (vertex_mem.write(write_size, vertex_ptr, write_offset))
                                        return -1;
                        }
                        base_vertex += uint32_t(mesh.vertexCount());
                }
                return 0;
        } else {
                return -1;
        }
}   

int32_t 
Scene::update_aggregate_mesh_vertices()
{
	if (!m_initialized || !m_aggregate_mesh_built)
                return -1;

	/*---------------------- Move model data to OpenCL device -----------------*/

        DeviceInterface& device = *DeviceInterface::instance();
        size_t vertex_count = aggregate_mesh.vertexCount();
        DeviceMemory& vertex_mem = device.memory(vert_id);
        size_t vertex_mem_size = vertex_count * sizeof(Vertex);
        const void*  vertex_ptr = aggregate_mesh.vertexArray();
        if (vertex_mem.write(vertex_mem_size, vertex_ptr, 0))
                    return -1;
        return 0;

}   

int32_t
Scene::set_dir_light(const directional_light_cl& dl)
{
        if (!m_initialized)
                return -1;

        lights.light.type = DIR_L;
	lights.light.directional = dl;

	vec3 vdl = float3_to_vec3(dl.dir);
	vdl.normalize();
	lights.light.directional.dir = vec3_to_float3(vdl);

        DeviceInterface& device = *DeviceInterface::instance();
        DeviceMemory& light_mem = device.memory(lights_id);

        return light_mem.write(sizeof(lights_cl), &lights);
}

int32_t
Scene::set_spot_light(const spot_light_cl& sp)
{
        if (!m_initialized)
                return -1;

        lights.light.type = SPOT_L;
	lights.light.spot = sp;

	vec3 vsp = float3_to_vec3(sp.dir);
	vsp.normalize();
	lights.light.spot.dir = vec3_to_float3(vsp);

        DeviceInterface& device = *DeviceInterface::instance();
        DeviceMemory& light_mem = device.memory(lights_id);

        return light_mem.write(sizeof(lights_cl), &lights);
}

int32_t
Scene::set_ambient_light(const color_cl& c)
{
        if (!m_initialized)
                return -1;

	lights.ambient = c;

        DeviceInterface& device = *DeviceInterface::instance();
        DeviceMemory& light_mem = device.memory(lights_id);

        return light_mem.write(sizeof(lights_cl), &lights);
}

int32_t
Scene::acquire_graphic_resources()
{
        if (!m_initialized)
                return -1;

        if (texture_atlas.acquire_graphic_resources() ||
            cubemap.acquire_graphic_resources())
                return -1;

        return 0;
}

int32_t
Scene::release_graphic_resources()
{
        if (!m_initialized)
                return -1;

        if (texture_atlas.release_graphic_resources() || 
            cubemap.release_graphic_resources())
                return -1;
        
        return 0;
}

DeviceMemory& 
Scene::vertex_mem()
{
        return DeviceInterface::instance()->memory(vert_id);
}

DeviceMemory& 
Scene::index_mem()
{
        return DeviceInterface::instance()->memory(idx_id);
}

DeviceMemory& 
Scene::material_list_mem()
{
        return DeviceInterface::instance()->memory(mat_list_id);
}

DeviceMemory& 
Scene::material_map_mem()
{
        return DeviceInterface::instance()->memory(mat_map_id);
}

DeviceMemory&
Scene::bvh_nodes_mem()
{
        return DeviceInterface::instance()->memory(bvh_id);
}

DeviceMemory&
Scene::bvh_roots_mem()
{
        return DeviceInterface::instance()->memory(bvh_roots_id);
}

DeviceMemory&
Scene::kdtree_nodes_mem()
{
        return DeviceInterface::instance()->memory(kdt_nodes_id);
}

DeviceMemory&
Scene::kdtree_leaf_tris_mem()
{
        return DeviceInterface::instance()->memory(kdt_leaf_tris_id);
}

DeviceMemory&
Scene::lights_mem()
{
        return DeviceInterface::instance()->memory(lights_id);
}


size_t 
Scene::triangle_count()
{
        
        if (!m_initialized)
                return 0;

        size_t vtx_count = 0;
        std::vector<Object>::iterator obj;
        for (obj = objects.begin(); obj != objects.end(); obj++) {
                if (!obj->is_valid())
                        continue;
                Mesh& mesh = mesh_atlas[obj->id];
                vtx_count += mesh.triangleCount();
        }
        return vtx_count;

}

size_t 
Scene::vertex_count()
{
        if (!m_initialized)
                return 0;

        size_t vtx_count = 0;
        std::vector<Object>::iterator obj;
        for (obj = objects.begin(); obj != objects.end(); obj++) {
                if (!obj->is_valid())
                        continue;
                Mesh& mesh = mesh_atlas[obj->id];
                vtx_count += mesh.vertexCount();
        }
        return vtx_count;
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

