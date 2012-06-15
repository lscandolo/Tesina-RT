#include <rt/bbox.hpp>

BBox::BBox()
{
	float M = std::numeric_limits<float>::max();
	float m = std::numeric_limits<float>::min();
	
	hi = makeFloat3(m,m,m);
	lo = makeFloat3(M,M,M);
}

BBox::BBox(const Vertex& a, const Vertex& b, const Vertex& c)
{
	hi = max(a.position, max(b.position,c.position));
	lo = min(a.position, min(b.position,c.position));
}

void 
BBox::set(const Vertex& a, const Vertex& b, const Vertex& c)
{
	hi = max(a.position, max(b.position,c.position));
	lo = min(a.position, min(b.position,c.position));
}

void 
BBox::merge(const BBox& b)
{
	hi = max(hi, b.hi);
	lo = min(lo, b.lo);
}

void 
BBox::add_slack(const vec3& sl)
{
	hi.s[0] += sl[0];
	hi.s[1] += sl[1];
	hi.s[2] += sl[2];

	lo.s[0] -= sl[0];
	lo.s[1] -= sl[1];
	lo.s[2] -= sl[2];
}

uint8_t 
BBox::largestAxis() const
{
	float dx = hi.s[0] - lo.s[0];
	float dy = hi.s[1] - lo.s[1];
	float dz = hi.s[2] - lo.s[2];
	// if (dx >= dy && dx >= dz)
        //         return dy > dz ? 2 : 1;
	// if (dy >= dx && dy >= dz)
        //         return dx > dz ? 2 : 0;
	// if (dz >= dx && dz >= dy)
        //         return dx > dy ? 1 : 0;
	if (dx > dy && dx > dz)
		return 0;
	else if (dy > dz)
		return 1;
	else
		return 2;
}

vec3 
BBox::centroid() const
{
	vec3 hi_vec = float3_to_vec3(hi);
	vec3 lo_vec = float3_to_vec3(lo);
	return (hi_vec+lo_vec) * 0.5f;
	// return vec3_to_float3((hi_vec+lo_vec) * 0.5f);
}

float 
BBox::surfaceArea() const
{
	vec3 hi_vec = float3_to_vec3(hi);
	vec3 lo_vec = float3_to_vec3(lo);
	vec3 diff = max(hi_vec - lo_vec, makeVector(0.f,0.f,0.f));
	return (diff[0] * (diff[1] + diff[2]) + diff[1] * diff[2]) * 2.f;
	
}
