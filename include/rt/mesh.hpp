#ifndef RT_MESH_HPP
#define RT_MESH_HPP

#include <rt/vector.hpp>
#include <stdint.h>
#include <vector>

struct Vertex
{
        float position[3];
        float texCoord[2];
        float normal[3];
        float tangent[4];
        float bitangent[3];
};

class Material{}; // Get own class for this guy!!

typedef uint32_t index_t;
typedef uint32_t tri_id;
typedef uint32_t vtx_id;

class Mesh
{

private:
	std::vector<Vertex> vertices;
	std::vector<index_t> indices;

	uint32_t triangle_count;
	uint32_t  vertex_count;

public:
	Mesh();

        /* Returns the number of triangles in the mesh */
        inline uint32_t triangleCount() const;
	
        /* Accessor for the vertex array */
        inline const Vertex& vertex(vtx_id i) const;

        /* Accessor for the index array */
        inline index_t vertexIndex(tri_id tri, uint32_t which) const;

        // inline const index_t* indexArray() const;

};

#endif /* RT_MESH_HPP */
