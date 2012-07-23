#include <rt/ray.hpp>

// ray_cl methods

/* Constructor */
ray_cl::ray_cl( const vec3& origin, const vec3& direction) {
	ori = vec3_to_float3(origin);
	dir = vec3_to_float3(direction);
	invDir = vec3_to_float3(inv(direction));
	tMin = 0.f;
	tMax = std::numeric_limits<float>::max();
}


/* Value setter */
void ray_cl::set( const vec3& origin, const vec3& direction)
{
	ori = vec3_to_float3(origin);
	dir = vec3_to_float3(direction);
	invDir = vec3_to_float3(inv(direction));
	tMin = 0.f;
	tMax = std::numeric_limits<float>::max();
}

/// Tests whether a 'T-value' is within the ray's valid interval

bool ray_cl::validT(float t) const 
{ return ( t >= tMin && t < tMax ); };


/* --------------------------------- RayBundle methods ----------------------------*/

RayBundle::RayBundle() 
{
	m_initialized = false;
	m_ray_count = 0;
}

RayBundle::~RayBundle()
{
        DeviceInterface& device = *DeviceInterface::instance();
	if (m_initialized && device.good()) {
		device.delete_memory(ray_id);
	}
}

int32_t 
RayBundle::initialize(const size_t rays)
{
	if (rays <= 0 || m_initialized)
		return -1;

        DeviceInterface& device = *DeviceInterface::instance();
        if (!device.good())
                return -1;

        ray_id = device.new_memory();
        DeviceMemory& ray_mem = device.memory(ray_id);
        if (ray_mem.initialize(rays * sizeof(sample_cl), READ_WRITE_MEMORY))
                return -1;

	m_initialized = true;
	m_ray_count = rays;
	return 0;
}

bool 
RayBundle::valid()
{
        DeviceInterface& device = *DeviceInterface::instance();
	return m_initialized && device.good();
}

int32_t 
RayBundle::count()
{
	return m_ray_count;
}

DeviceMemory&
RayBundle::mem()
{
        DeviceInterface& device = *DeviceInterface::instance();
        return device.memory(ray_id);
}

/* --------------------------------- HitBundle methods ----------------------------*/

 
HitBundle::HitBundle()
{
	m_size = 0;
	m_initialized = false;
}

int32_t 
HitBundle::initialize(const size_t sz)
{
	if (m_initialized)
		return -1;

        DeviceInterface& device = *DeviceInterface::instance();
        if (!device.good())
                return -1;

        hit_id = device.new_memory();
        DeviceMemory& hit_mem = device.memory(hit_id);
        if (hit_mem.initialize(sz * sizeof(sample_trace_info_cl), READ_WRITE_MEMORY))
                return -1;


        m_initialized = true;
        m_size = sz;
	return 0;
}

DeviceMemory& 
HitBundle::mem()
{
        DeviceInterface& device = *DeviceInterface::instance();
	return device.memory(hit_id);
}
