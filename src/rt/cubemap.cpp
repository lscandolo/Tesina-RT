#include <rt/cubemap.hpp>

bool 
Cubemap::initialize(std::string posx, std::string negx,
		    std::string posy, std::string negy,
		    std::string posz, std::string negz,
		    const CLInfo& clinfo)
{

	posx_tex = create_tex_gl_from_jpeg(tex_width,tex_height,posx.c_str());
	negx_tex = create_tex_gl_from_jpeg(tex_width,tex_height,negx.c_str());
	posy_tex = create_tex_gl_from_jpeg(tex_width,tex_height,posy.c_str());
	negy_tex = create_tex_gl_from_jpeg(tex_width,tex_height,negy.c_str());
	posz_tex = create_tex_gl_from_jpeg(tex_width,tex_height,posz.c_str());
	negz_tex = create_tex_gl_from_jpeg(tex_width,tex_height,negz.c_str());
	

	if (posx_tex < 0 || negx_tex < 0 || 
	    posy_tex < 0 || negy_tex < 0 || 
	    posz_tex < 0 || negz_tex < 0) {
		return false;
	}
	
	if (create_cl_mem_from_gl_tex(clinfo, posx_tex, &posx_mem))
		return false;
	if (create_cl_mem_from_gl_tex(clinfo, negx_tex, &negx_mem))
		return false;
	if (create_cl_mem_from_gl_tex(clinfo, posy_tex, &posy_mem))
		return false;
	if (create_cl_mem_from_gl_tex(clinfo, negy_tex, &negy_mem))
		return false;
	if (create_cl_mem_from_gl_tex(clinfo, posz_tex, &posz_mem))
		return false;
	if (create_cl_mem_from_gl_tex(clinfo, negz_tex, &negz_mem))
		return false;
	



	return true;
}
