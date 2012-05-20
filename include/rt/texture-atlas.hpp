#ifndef TEXTURE_ATLAS_HPP
#define TEXTURE_ATLAS_HPP

// #include <stdint.h>

#include <string>
#include <vector>
#include <map>

#include <gpu/interface.hpp>
#include <cl-gl/opengl-init.hpp>

typedef int32_t texture_id;

class TextureAtlas 
{

public:
        int32_t       initialize(CLInfo& clinfo);
        texture_id    load_texture(std::string filename);
        DeviceMemory& texture_mem(texture_id);
        
private:
        DeviceInterface device;
        memory_id invalid_tex_mem_id;

        static const texture_id invalid_tex_id = -1;
        static const size_t MAX_TEXTURE_COUNT = 20;

        bool m_initialized;

        std::map<std::string,texture_id> file_map;
        std::vector<memory_id> tex_mem_ids;
};


#endif /* TEXTURE_ATLAS_HPP */
