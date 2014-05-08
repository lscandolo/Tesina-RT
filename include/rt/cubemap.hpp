#pragma once
#ifndef RT_CUBEMAP_HPP
#define RT_CUBEMAP_HPP

#include <string>
#include <cl-gl/opencl-init.hpp>
#include <cl-gl/opengl-init.hpp>
#include <gpu/interface.hpp>

class Cubemap {

public:

        Cubemap()  {m_initialized = false;}
        int32_t initialize(std::string posx, std::string negx,
                           std::string posy, std::string negy,
                           std::string posz, std::string negz);
        int32_t destroy();


        int32_t acquire_graphic_resources();
        int32_t release_graphic_resources();

	DeviceMemory& positive_x_mem()
                {return DeviceInterface::instance()->memory(posx_id);}
	DeviceMemory& negative_x_mem()
                {return DeviceInterface::instance()->memory(negx_id);};
	DeviceMemory& positive_y_mem()
                {return DeviceInterface::instance()->memory(posy_id);};
	DeviceMemory& negative_y_mem()
                {return DeviceInterface::instance()->memory(negy_id);};
        DeviceMemory& positive_z_mem()
                {return DeviceInterface::instance()->memory(posz_id);};
        DeviceMemory& negative_z_mem()
                {return DeviceInterface::instance()->memory(negz_id);};

        bool enabled;

        private:

        bool m_initialized;
        uint32_t tex_width,tex_height;

	GLuint posx_tex,negx_tex;
	GLuint posy_tex,negy_tex;
	GLuint posz_tex,negz_tex;

	memory_id posx_id,negx_id;
	memory_id posy_id,negy_id;
	memory_id posz_id,negz_id;

};

#endif /* RT_CUBEMAP_HPP */
