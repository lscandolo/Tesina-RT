#ifndef RT_CUBEMAP_HPP
#define RT_CUBEMAP_HPP

#include <string>
#include <cl-gl/opencl-init.hpp>
#include <cl-gl/opengl-init.hpp>
#include <gpu/interface.hpp>

class Cubemap {

public:

	bool initialize(std::string posx, std::string negx,
			std::string posy, std::string negy,
			std::string posz, std::string negz,
			const CLInfo& clinfo);

	DeviceMemory& positive_x_mem(){return device.memory(posx_id);}
	DeviceMemory& negative_x_mem(){return device.memory(negx_id);};
	DeviceMemory& positive_y_mem(){return device.memory(posy_id);};
	DeviceMemory& negative_y_mem(){return device.memory(negy_id);};
        DeviceMemory& positive_z_mem(){return device.memory(posz_id);};
        DeviceMemory& negative_z_mem(){return device.memory(negz_id);};

	// cl_mem& positive_x_mem(){return posx_mem;}
	// cl_mem& negative_x_mem(){return negx_mem;};
	// cl_mem& positive_y_mem(){return posy_mem;};
	// cl_mem& negative_y_mem(){return negy_mem;};
	// cl_mem& positive_z_mem(){return posz_mem;};
	// cl_mem& negative_z_mem(){return negz_mem;};

private:

	uint32_t tex_width,tex_height;

	GLuint posx_tex,negx_tex;
	GLuint posy_tex,negy_tex;
	GLuint posz_tex,negz_tex;

        DeviceInterface device;
	memory_id posx_id,negx_id;
	memory_id posy_id,negy_id;
	memory_id posz_id,negz_id;

	// cl_mem posx_mem,negx_mem;
	// cl_mem posy_mem,negy_mem;
	// cl_mem posz_mem,negz_mem;

};

#endif /* RT_CUBEMAP_HPP */
