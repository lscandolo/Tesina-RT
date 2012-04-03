#include <rt/ray-shader.hpp>

bool 
RayShader::initialize(CLInfo& clinfo)

		
{
        if (device.initialize(clinfo))
                return false;

	/*------------------------ Set up ray shading kernel info ---------------------*/
        shade_id = device.new_function();
        DeviceFunction& shade_function = device.function(shade_id);

        if (shade_function.initialize("src/kernel/ray-shader.cl", "shade"))
                return false;

        shade_function.set_dims(1);

	timing = false;
	return true;
}

bool 
RayShader::shade(RayBundle& rays, HitBundle& hb, Scene& scene, 
		 Cubemap& cm, FrameBuffer& fb, int32_t size)
{
	if (timing)
		timer.snap_time();

        DeviceFunction& shade_function = device.function(shade_id);
        
        if (shade_function.set_arg(0, fb.image_mem()))
            return false;

        if (shade_function.set_arg(1,sizeof(cl_mem),&hb.mem()))
		return false;

        if (shade_function.set_arg(2,sizeof(cl_mem),&rays.mem()))
		return false;

        if (shade_function.set_arg(3,scene.material_list_mem()))
		return false;

        if (shade_function.set_arg(4,scene.material_map_mem()))
		return false;

        if (shade_function.set_arg(5,cm.positive_x_mem()))
		return false;

        if (shade_function.set_arg(6,cm.negative_x_mem()))
		return false;

        if (shade_function.set_arg(7,cm.positive_y_mem()))
		return false;

        if (shade_function.set_arg(8,cm.negative_y_mem()))
		return false;

        if (shade_function.set_arg(9,cm.positive_z_mem()))
		return false;

        if (shade_function.set_arg(10,cm.negative_z_mem()))
		return false;

        if (shade_function.set_arg(11,scene.lights_mem()))
		return false;

        size_t shade_work_size[] = {size, 0, 0};
        shade_function.set_global_size(shade_work_size);

	if (shade_function.execute())
		return false;

	if (timing)
		time_ms = timer.msec_since_snap();

	return true;
}

void
RayShader::enable_timing(bool b)
{
	timing = b;
}

double 
RayShader::get_exec_time()
{
	return time_ms;
}
