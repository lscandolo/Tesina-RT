#include <rt/cubemap.hpp>

int32_t 
Cubemap::initialize(std::string posx, std::string negx,
		    std::string posy, std::string negy,
		    std::string posz, std::string negz)
{
        DeviceInterface& device = *DeviceInterface::instance();
        if (!device.good() || m_initialized)
                return -1;

	if (create_tex_gl_from_file(tex_width,tex_height,posx.c_str(),&posx_tex) ||
            create_tex_gl_from_file(tex_width,tex_height,negx.c_str(),&negx_tex) ||
            create_tex_gl_from_file(tex_width,tex_height,posy.c_str(),&posy_tex) ||
            create_tex_gl_from_file(tex_width,tex_height,negy.c_str(),&negy_tex) ||
            create_tex_gl_from_file(tex_width,tex_height,posz.c_str(),&posz_tex) ||
            create_tex_gl_from_file(tex_width,tex_height,negz.c_str(),&negz_tex))
		return -1;

	
        posx_id = device.new_memory();
        if (device.memory(posx_id).initialize_from_gl_texture(posx_tex))
                return -1;

        negx_id = device.new_memory();
        if (device.memory(negx_id).initialize_from_gl_texture(negx_tex))
                return -1;

        posy_id = device.new_memory();
        if (device.memory(posy_id).initialize_from_gl_texture(posy_tex))
                return -1;

        negy_id = device.new_memory();
        if (device.memory(negy_id).initialize_from_gl_texture(negy_tex))
                return -1;

        posz_id = device.new_memory();
        if (device.memory(posz_id).initialize_from_gl_texture(posz_tex))
                return -1;

        negz_id = device.new_memory();
        if (device.memory(negz_id).initialize_from_gl_texture(negz_tex))
                return -1;
        m_initialized = true;
        enabled = true;
	return 0;
}

int32_t
Cubemap::acquire_graphic_resources()
{
        if (!m_initialized)
                return -1;

        DeviceInterface& device = *DeviceInterface::instance();
        if (device.acquire_graphic_resource(posx_id) ||
            device.acquire_graphic_resource(posy_id) ||
            device.acquire_graphic_resource(posz_id) ||
            device.acquire_graphic_resource(negx_id) ||
            device.acquire_graphic_resource(negy_id) ||
            device.acquire_graphic_resource(negz_id))
                return -1;

        return 0;
}

int32_t 
Cubemap::release_graphic_resources()
{
        if (!m_initialized)
                return -1;

        DeviceInterface& device = *DeviceInterface::instance();
        if (device.release_graphic_resource(posx_id) ||
            device.release_graphic_resource(posy_id) ||
            device.release_graphic_resource(posz_id) ||
            device.release_graphic_resource(negx_id) ||
            device.release_graphic_resource(negy_id) ||
            device.release_graphic_resource(negz_id))
                return -1;

        return 0;
}
