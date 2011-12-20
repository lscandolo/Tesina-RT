#ifndef RT_MESH_HPP
#define RT_MESH_HPP

#include <stdint.h>
#include <vector>

#include <rt/vector.hpp>
#include <rt/geom.hpp>
#include <rt/cl_aux.hpp>

typedef uint32_t index_t;
typedef uint32_t tri_id;
typedef uint32_t vtx_id;

struct Triangle
{
	index_t v[3];
};

struct Vertex
{
        cl_float3 position;
        cl_float3 normal;
        cl_float4 tangent;
        cl_float3 bitangent;
        cl_float2 texCoord;
};

// struct Triangle
// {
// 	index_t v[3];
// };

// struct Vertex
// {
//         float position[3];
//         float texCoord[2];
//         float normal[3];
//         float tangent[4];
//         float bitangent[3];
// };

class Material{}; // Get own file for this guy!!

class Mesh
{

public:
	std::vector<Vertex> vertices;
	std::vector<Triangle> triangles;

public:
	Mesh();

        /* Returns the number of triangles in the mesh */
        uint32_t triangleCount() const;
	
        /* Returns the number of vertices in the mesh */
	uint32_t vertexCount() const;

        /* Accessor for the vertex array */
        const Vertex& vertex(vtx_id i) const;

        /* Accessor for a triangle */
	const Triangle& triangle(tri_id tri) const;

        /* Accessor for the index array */
        index_t triangleVertexIndex(tri_id tri, uint32_t which) const;

        /* Accesor for the triangle array */
	const Triangle* triangleArray() const;

        /* Accesor for the vertex array */
	const Vertex* vertexArray() const; 
};

class MeshInstance
{
private: 
	Mesh* mesh;
	GeometricProperties geom;
};

#endif /* RT_MESH_HPP */
