#include <rt/mesh.hpp>

/* Constructs a basic mesh around arrays of vertex and index data. */
inline Mesh::Mesh()
{triangle_count = vertex_count = 0;}

/* Returns the number of triangles in the mesh */
inline uint32_t Mesh::triangleCount() const 
{return triangle_count; }
	
/* Accessor for the vertex array */
inline const Vertex& Mesh::vertex(vtx_id i) const 
{return vertices[i];}

/* Accessor for the index array */
inline index_t Mesh::vertexIndex(tri_id tri, uint32_t which) const 
{ return indices[3*tri + which]; }

// inline const index_t* Mesh::indexArray() const 
// {return indices;}


