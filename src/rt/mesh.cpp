#include <rt/mesh.hpp>
#include <rt/assert.hpp>


/*-------------------- Mesh Methods -------------------------------*/

/* Constructs a basic mesh around arrays of vertex and index data. */
Mesh::Mesh()
{}

/* Returns the number of triangles in the mesh */
size_t Mesh::triangleCount() const 
{return triangles.size(); }

/* Returns the number of vertices in the mesh */
size_t Mesh::vertexCount() const 
{return vertices.size(); }
	
/* Accessor for the vertex array */
Vertex& Mesh::vertex(vtx_id i) 
{return vertices[i];}

/* Accessor for the vertex array (copy) */
Vertex Mesh::vertex(vtx_id i) const 
{return vertices[i];}

/* Accessor for a triangle */
Triangle& Mesh::triangle(tri_id tri) 
{ return triangles[tri]; }

/* Accessor for a triangle */
Triangle Mesh::triangle(tri_id tri) const 
{ return triangles[tri]; }

/* Accessor for a triangle vertex */
vtx_id Mesh::triangleVertexIndex(tri_id tri, uint32_t which) const 
{ return triangles[tri].v[which]; }

/* Accesor for the triangle array */
const Triangle* Mesh::triangleArray() const 
{return &triangles[0];}

/* Accesor for the vertex array */
const Vertex* Mesh::vertexArray() const 
{return &vertices[0];}

/* Reorder mesh triangles accorging to order array */
void Mesh::reorderTriangles(const std::vector<uint32_t>& order){
	ASSERT(triangles.size() == order.size());
	std::vector<Triangle> old_triangles = triangles;
	for (uint32_t i = 0; i < triangles.size(); ++i) {
		triangles[i] = old_triangles[order[i]];
	}
}

/* Set a global slack for all triangles */
void Mesh::set_global_slack(vec3 slack) 
{
        slacks = std::vector<vec3>(triangles.size(),slack);
}

/* Clear all data */
void Mesh::destroy()
{
        vertices.clear();
        triangles.clear();
        slacks.clear();
        original_materials.clear();
}
