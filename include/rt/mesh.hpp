#ifndef RT_MESH_HPP
#define RT_MESH_HPP

#include <stdint.h>
#include <vector>

#include <rt/vector.hpp>
#include <rt/cl_aux.hpp>
#include <rt/assert.hpp>

typedef uint32_t index_t;
typedef uint32_t tri_id;
typedef uint32_t vtx_id;

struct Triangle
{
	index_t v[3];
};

RT_ALIGN(16)
struct Vertex
{
        cl_float3 position;
        cl_float3 normal;
        cl_float4 tangent;
        cl_float3 bitangent;
        cl_float2 texCoord;
};

class Mesh
{

public:
	std::vector<Vertex> vertices;
	std::vector<Triangle> triangles;
	std::vector<vec3> slacks;

public:
	Mesh();

        /* Returns the number of triangles in the mesh */
       size_t triangleCount() const;
	
        /* Returns the number of vertices in the mesh */
	   size_t vertexCount() const;

        /* Accessor for the vertex array */
        Vertex& vertex(vtx_id i) ;

        /* Accessor for the vertex array (copy) */
        Vertex vertex(vtx_id i) const;

        /* Accessor for a triangle */
	Triangle& triangle(tri_id tri) ;

        /* Accessor for a triangle (copy) */
	Triangle triangle(tri_id tri) const;

        /* Accessor for the index array */
        index_t triangleVertexIndex(tri_id tri, uint32_t which) const;

        /* Accesor for the triangle array */
	const Triangle* triangleArray() const;

        /* Accesor for the vertex array */
	const Vertex* vertexArray() const; 

        /* Reorder mesh triangles accorging to order array */
	void reorderTriangles(const std::vector<uint32_t>& order); 

};

#endif /* RT_MESH_HPP */
