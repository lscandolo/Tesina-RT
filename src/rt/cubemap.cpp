#include <rt/cubemap.hpp>

int32_t 
Cubemap::initialize(std::string posx, std::string negx,
		    std::string posy, std::string negy,
		    std::string posz, std::string negz,
		    const CLInfo& clinfo)
{

        if (device.initialize(clinfo))
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

	return 0;
}
