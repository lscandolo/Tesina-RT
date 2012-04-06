#ifndef RT_RAY_SHADER_HPP
#define RT_RAY_SHADER_HPP

#include <cl-gl/opencl-init.hpp>
#include <gpu/interface.hpp>
#include <rt/timing.hpp>
#include <rt/ray.hpp>
#include <rt/scene.hpp>
#include <rt/cubemap.hpp>
#include <rt/framebuffer.hpp>

class RayShader {

public:
	
	int32_t initialize(CLInfo& clinfo);
	int32_t shade(RayBundle& rays, HitBundle& hb, Scene& scene, 
		   Cubemap& cm, FrameBuffer& fb, size_t size);

	void timing(bool b);
	double get_exec_time();

private:

        DeviceInterface device;
        function_id shade_id;

	bool         m_timing;
	rt_time_t    m_timer;
	double       m_time_ms;
};

#endif /* RT_RAY-SHADER_HPP */
