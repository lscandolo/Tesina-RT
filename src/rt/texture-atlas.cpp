#include <rt/texture-atlas.hpp>

int32_t
TextureAtlas::initialize()
{
        DeviceInterface& device = *DeviceInterface::instance();
        if (!device.good())
                return -1;
        invalid_tex_mem_id = device.new_memory();

        DeviceMemory& invalid_tex_mem = device.memory(invalid_tex_mem_id);
        GLuint invalid_gl_tex_id = create_tex_gl(1,1);
        if (invalid_gl_tex_id < 0)
                return -1;
        if (invalid_tex_mem.initialize_from_gl_texture(invalid_gl_tex_id))
                return -1;

        m_initialized = true;
        return 0;
}

texture_id
TextureAtlas::load_texture(std::string filename)
{
        DeviceInterface& device = *DeviceInterface::instance();
        if (!m_initialized || !device.good())
                return invalid_tex_id;
        
        if (file_map.find(filename) != file_map.end())
                return file_map[filename];

        if (tex_mem_ids.size() >= MAX_TEXTURE_COUNT)
                return invalid_tex_id;

        uint32_t tex_width, tex_height;
        GLuint new_tex_gl_id;
        if (create_tex_gl_from_file(tex_width, tex_height, 
                                    filename.c_str(), &new_tex_gl_id))
                return invalid_tex_id;

        memory_id new_tex_mem_id = device.new_memory();
        DeviceMemory& new_tex_mem = device.memory(new_tex_mem_id);
        // std::cout << "Initializing: " << filename << std::endl;
        // std::cout << "new_tex_gl_id: " << new_tex_gl_id << std::endl;
        if (new_tex_mem.initialize_from_gl_texture(new_tex_gl_id))
                return invalid_tex_id;
        
        texture_id new_tex_id = tex_mem_ids.size();
        // std::cout << "new_tex_id: " << new_tex_id << std::endl;
        tex_mem_ids.push_back(new_tex_mem_id);
        file_map[filename] = new_tex_id;
        return new_tex_id;
}

DeviceMemory&
TextureAtlas::texture_mem(texture_id tex_id)
{
        DeviceInterface& device = *DeviceInterface::instance();
        if (tex_id < 0 || tex_id >= tex_mem_ids.size())
                return device.memory(invalid_tex_mem_id);

        return device.memory(tex_mem_ids[tex_id]);
}

int32_t 
TextureAtlas::acquire_graphic_resources()
{
        DeviceInterface& device = *DeviceInterface::instance();
        if (!m_initialized)
                return -1;

        if (device.acquire_graphic_resource(invalid_tex_mem_id))
                return -1;

        for (size_t i = 0; i < tex_mem_ids.size(); ++i) {
                memory_id id = tex_mem_ids[i];
                if (device.acquire_graphic_resource(id))
                        return -1;
        }
        return 0;
}

int32_t 
TextureAtlas::release_graphic_resources()
{
        DeviceInterface& device = *DeviceInterface::instance();
        if (!m_initialized)
                return -1;

        if (device.release_graphic_resource(invalid_tex_mem_id))
                return -1;

        for (size_t i = 0; i < tex_mem_ids.size(); ++i) {
                memory_id id = tex_mem_ids[i];
                if (device.release_graphic_resource(id))
                        return -1;
        }

        return 0;
}
