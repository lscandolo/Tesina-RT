#include <rt/ray-shader.hpp>

int32_t 
RayShader::initialize()

		
{
        DeviceInterface& device = *DeviceInterface::instance();
        if (!device.good())
                return -1;

	/*------------------------ Set up ray shading kernel info ---------------------*/
        p_shade_id = device.new_function();
        DeviceFunction& p_shade_function = device.function(p_shade_id);

        if (p_shade_function.initialize("src/kernel/ray-shader.cl", "shade_primary"))
                return -1;

        p_shade_function.set_dims(1);


        s_shade_id = device.new_function();
        DeviceFunction& s_shade_function = device.function(s_shade_id);

        if (s_shade_function.initialize("src/kernel/ray-shader.cl", "shade_secondary"))
                return -1;

        s_shade_function.set_dims(1);

	m_timing = false;
	return 0;
}

int32_t 
RayShader::shade(RayBundle& rays, HitBundle& hb, Scene& scene, 
		 Cubemap& cm, FrameBuffer& fb, size_t size, bool primary)
{
	if (m_timing)
		m_timer.snap_time();

        DeviceInterface& device = *DeviceInterface::instance();
        function_id shade_id = primary? p_shade_id : s_shade_id;

        DeviceFunction& shade_function = device.function(shade_id);
        
        if (shade_function.set_arg(0, fb.image_mem()))
            return -1;

        if (shade_function.set_arg(1,hb.mem()))
		return -1;

        if (shade_function.set_arg(2,rays.mem()))
		return -1;

        if (shade_function.set_arg(3,scene.material_list_mem()))
		return -1;

        if (shade_function.set_arg(4,scene.material_map_mem()))
		return -1;

        if (shade_function.set_arg(5,cm.positive_x_mem()))
		return -1;

        if (shade_function.set_arg(6,cm.negative_x_mem()))
		return -1;

        if (shade_function.set_arg(7,cm.positive_y_mem()))
		return -1;

        if (shade_function.set_arg(8,cm.negative_y_mem()))
		return -1;

        if (shade_function.set_arg(9,cm.positive_z_mem()))
		return -1;

        if (shade_function.set_arg(10,cm.negative_z_mem()))
		return -1;

        /*Texture_0 to Texture_19*/
        if (shade_function.set_arg(11,scene.texture_atlas.texture_mem(0)))
		return -1;
        if (shade_function.set_arg(12,scene.texture_atlas.texture_mem(1)))
		return -1;
        if (shade_function.set_arg(13,scene.texture_atlas.texture_mem(2)))
		return -1;
        if (shade_function.set_arg(14,scene.texture_atlas.texture_mem(3)))
		return -1;
        if (shade_function.set_arg(15,scene.texture_atlas.texture_mem(4)))
		return -1;
        if (shade_function.set_arg(16,scene.texture_atlas.texture_mem(5)))
		return -1;
        if (shade_function.set_arg(17,scene.texture_atlas.texture_mem(6)))
		return -1;
        if (shade_function.set_arg(18,scene.texture_atlas.texture_mem(7)))
		return -1;
        if (shade_function.set_arg(19,scene.texture_atlas.texture_mem(8)))
		return -1;
        if (shade_function.set_arg(20,scene.texture_atlas.texture_mem(9)))
		return -1;
        if (shade_function.set_arg(21,scene.texture_atlas.texture_mem(10)))
		return -1;
        if (shade_function.set_arg(22,scene.texture_atlas.texture_mem(11)))
		return -1;
        if (shade_function.set_arg(23,scene.texture_atlas.texture_mem(12)))
		return -1;
        if (shade_function.set_arg(24,scene.texture_atlas.texture_mem(13)))
		return -1;
        if (shade_function.set_arg(25,scene.texture_atlas.texture_mem(14)))
		return -1;
        if (shade_function.set_arg(26,scene.texture_atlas.texture_mem(15)))
		return -1;
        if (shade_function.set_arg(27,scene.texture_atlas.texture_mem(16)))
		return -1;
        if (shade_function.set_arg(28,scene.texture_atlas.texture_mem(17)))
		return -1;
        if (shade_function.set_arg(29,scene.texture_atlas.texture_mem(18)))
		return -1;
        if (shade_function.set_arg(30,scene.texture_atlas.texture_mem(19)))
		return -1;
        if (shade_function.set_arg(31,scene.texture_atlas.texture_mem(20)))
		return -1;
        if (shade_function.set_arg(32,scene.texture_atlas.texture_mem(21)))
		return -1;
        if (shade_function.set_arg(33,scene.texture_atlas.texture_mem(22)))
		return -1;
        if (shade_function.set_arg(34,scene.texture_atlas.texture_mem(23)))
		return -1;
        if (shade_function.set_arg(35,scene.texture_atlas.texture_mem(24)))
		return -1;
        if (shade_function.set_arg(36,scene.texture_atlas.texture_mem(25)))
		return -1;

        cl_int use_cubemap = cm.enabled;
        if (shade_function.set_arg(37,sizeof(use_cubemap), &use_cubemap))
		return -1;

        if (shade_function.set_arg(38,scene.lights_mem()))
		return -1;

        size_t group_size = primary? 0:device.max_group_size(0)/2;

        if (shade_function.enqueue_single_dim(size,group_size))
                return -1;
        device.enqueue_barrier();

	if (m_timing)
		m_time_ms = m_timer.msec_since_snap();

	return 0;

}

void
RayShader::timing(bool b)
{
	m_timing = b;
}

double 
RayShader::get_exec_time()
{
	return m_time_ms;
}
