#include <rt/tester.hpp>
#include <rt/test-params-dragon.hpp>
#include <rt/test-params.hpp>

Tester* Tester::instance = 0;

Tester* 
Tester::getSingletonPtr()
{
        if (!instance)
                instance = new Tester();
        return instance;
}

Tester::Tester()
{
        window_size.width = window_size.height = 1;

        for (int i = 0; i < 5; ++i)
                scenes.push_back(i);

        window_sizes.push_back(resolution_t(512,512));
        window_sizes.push_back(resolution_t(1024,1024));

        // rays_per_pixel.push_back(1);
        // rays_per_pixel.push_back(4);

        for (int i = 0; i < 3; ++i)
                view_positions.push_back(i);

        tile_to_cores_ratios.push_back(64);
        tile_to_cores_ratios.push_back(128);
        tile_to_cores_ratios.push_back(256);
        
        sec_ray_use_disc.push_back(false);
        sec_ray_use_disc.push_back(true);

        sec_ray_use_atomics.push_back(false);
        sec_ray_use_atomics.push_back(true);
        
        prim_ray_use_zcurve.push_back(false);
        prim_ray_use_zcurve.push_back(true);

        prim_ray_quad_sizes.push_back(16);
        prim_ray_quad_sizes.push_back(32);
        prim_ray_quad_sizes.push_back(64);
        prim_ray_quad_sizes.push_back(128);

        bvh_max_depths.push_back(24);
        bvh_max_depths.push_back(32);
        bvh_max_depths.push_back(48);
        
        bvh_min_leaf_sizes.push_back(1);
        bvh_min_leaf_sizes.push_back(4);
        bvh_min_leaf_sizes.push_back(8);

        bvh_refit_only.push_back(false);
        bvh_refit_only.push_back(true);
        
}

int 
Tester::initialize()
{
        // Initialize renderer
        renderer.configure_from_ini_file("rt.ini");
        renderer.config.bvh_depth = 32;
        renderer.config.tile_to_cores_ratio = 128;
        renderer.config.use_lbvh = true;
        renderer.config.bvh_refit_only = false;
        renderer.config.sec_ray_use_disc = false;
        renderer.config.prim_ray_quad_size = 32;
        renderer.config.prim_ray_use_zcurve = false;
        if (renderer.initialize("output.csv")) {
                std::cout << "Error initializing renderer.\n";
                return -1;
        }

        // Initialize scene
        if (scene.initialize()) {
                std::cout << "Error initializing scene.\n";
                return -1;
        }


        return 0;
}

size_t 
Tester::getWindowWidth()
{
        return window_size.width;
}

size_t 
Tester::getWindowHeight()
{
        return window_size.height;
}

Scene* 
Tester::getScene()
{
        return &scene;
}

Renderer* 
Tester::getRenderer()
{
        return &renderer;
}


void
Tester::loop() {

        CLInfo::instance()->set_sync(false);

        loop_timer.snap_time();
        debug_timer.snap_time();
        renderer.set_max_bounces(5);

        int combinations = 1;
        combinations *= view_positions.size();
        combinations *= tile_to_cores_ratios.size();
        combinations *= sec_ray_use_disc.size();
        combinations *= sec_ray_use_atomics.size();
        combinations *= prim_ray_quad_sizes.size();
        combinations *= prim_ray_use_zcurve.size();
        combinations *= bvh_max_depths.size();
        combinations *= bvh_min_leaf_sizes.size();
        combinations *= bvh_refit_only.size();

        printStatsHeaders();

        for (scene_i = 0; scene_i < scenes.size(); ++scene_i) {
                
          //// Set scene
          std::cerr << "Initializing scene " << scene_i << "\n";
          if (loadScene(scene_i))
                  return;

          for (wsize_i = 0; wsize_i < window_sizes.size(); ++wsize_i) {
                        
            //// Set screen resolution
            std::cerr << "Setting resolution " << wsize_i << "\n";
            if (setResolution(wsize_i))
                    return;

            for (int i = 0; i < combinations; ++i) {

                    //// Compute index for each parameter
                    int index = i;
                    bvh_refit_i = index % bvh_refit_only.size();
                    index /= bvh_refit_only.size();

                    bvh_min_leaf_sizes_i = index % bvh_min_leaf_sizes.size();
                    index /= bvh_min_leaf_sizes.size();

                    bvh_max_depths_i = index % bvh_max_depths.size();
                    index /= bvh_max_depths.size();

                    prim_ray_use_zcurve_i = index % prim_ray_use_zcurve.size();
                    index /= prim_ray_use_zcurve.size();

                    prim_ray_quad_sizes_i = index % prim_ray_quad_sizes.size();
                    index /= prim_ray_quad_sizes.size();

                    sec_ray_use_atomics_i = index % sec_ray_use_atomics.size();
                    index /= sec_ray_use_atomics.size();

                    sec_ray_use_disc_i = index % sec_ray_use_disc.size();
                    index /= sec_ray_use_disc.size();

                    tile_to_cores_ratios_i = index % tile_to_cores_ratios.size();
                    index /= tile_to_cores_ratios.size();

                    view_positions_i = index % view_positions.size();
                    index /= view_positions.size();

                    //// Get config values
                    bool bvh_refit         = bvh_refit_only[bvh_refit_i];
                    int  bvh_min_leaf_size = bvh_min_leaf_sizes[bvh_min_leaf_sizes_i];
                    int  bvh_max_depth     = bvh_max_depths[bvh_max_depths_i];
                    bool prim_use_zcurve   = prim_ray_use_zcurve[prim_ray_use_zcurve_i];
                    int  prim_quad_size    = prim_ray_quad_sizes[prim_ray_quad_sizes_i];
                    bool sec_use_atomics   = sec_ray_use_atomics[sec_ray_use_atomics_i];
                    bool sec_use_disc      = sec_ray_use_disc[sec_ray_use_disc_i];
                    int  tile_cores_ratio  = tile_to_cores_ratios[tile_to_cores_ratios_i];
                    int  view_position     = view_positions[view_positions_i];

                    //// Impossible / not implemented combinations
                    if (sec_use_atomics && sec_use_disc)
                            continue;

                    if (prim_use_zcurve && window_size.width != window_size.height)
                            continue;

                    if (prim_use_zcurve && !((window_size.width-1) & window_size.width))
                        continue; // Second clause means 'if not a power of 2'

                    //// Set renderer config
                    renderer.config.use_lbvh             = true;
                    renderer.config.bvh_refit_only       = bvh_refit;
                    renderer.config.bvh_depth            = bvh_max_depth;
                    renderer.config.bvh_min_leaf_size    = bvh_min_leaf_size;
                    renderer.config.prim_ray_use_zcurve  = prim_use_zcurve;
                    renderer.config.prim_ray_quad_size   = prim_quad_size;
                    renderer.config.sec_ray_use_atomics  = sec_use_atomics;
                    renderer.config.sec_ray_use_disc     = sec_use_disc;
                    renderer.config.tile_to_cores_ratio  = tile_cores_ratio;

                    //// Set camera config
                    scene.camera.set(stats_camera_pos[view_position],//pos 
                                     stats_camera_dir[view_position],//dir
                                     makeVector(0,1,0), //up
                                     M_PI/4.,
                                     window_size.width / (float)window_size.height);

                    if (testConfiguration()) {
                            std::cerr << "Error with configuration " << i << "\n";
                            return;
                    }
                    

            }
          }
        }

}

int 
Tester::testConfiguration() 
{
        // Attempt to initialize renderer (just in case)
        renderer.initialize("lala.csv");

        for (int i = 0; i < 3; ++i) {
                if (!i) {
                        renderer.clear_stats();
                }

                if (renderOneFrame())
                        return -1;

                if (i == 2) {
                        printStatsValues();
                }
        }

        return 0;
}

int
Tester::renderOneFrame()
{
        
        DeviceInterface* device = DeviceInterface::instance();
        device->finish_commands(0);

        // Update renderer config parameters
        if (renderer.update_configuration()) {
                std::cerr << "Error updating renderer configuration\n";
                return -1;
        }

        //Acquire graphics library resources, clear framebuffer, 
        // clear counters and create bvh if needed
        if (renderer.set_up_frame(cl_tex_id, scene)) {
                std::cerr << "Error in setting up frame" << "\n";
                return -1;
        }

        //Render image to framebuffer and copy to set up texture
        if (renderer.render_to_framebuffer(scene)) {
                std::cerr << "Error in rendering to framebuffer" << "\n";
                return -1;
        }

        if (renderer.copy_framebuffer()) {
                std::cerr << "Error in copying framebuffer" << "\n";
                return -1;
        }

        // if (renderer.render_to_texture(scene, gl_tex_id)) {
        //         std::cerr << "Error in rendering" << "\n";
        //         return -1;
        // }

        //Do cleanup after rendering frame
        if (renderer.conclude_frame(scene)) {
                std::cerr << "Error concluding frame" << "\n";
                return -1;
        }

        ////////////////// Immediate mode textured quad
        glLoadIdentity();
        glBindTexture(GL_TEXTURE_2D, gl_tex_id);

        glBegin(GL_TRIANGLE_STRIP);

        glTexCoord2f(1.0,1.0);
        glVertex2f(1.0,1.0);

        glTexCoord2f(1.0,0.0);
        glVertex2f(1.0,-1.0);

        glTexCoord2f(0.0,1.0);
        glVertex2f(-1.0,1.0);

        glTexCoord2f(0.0,0.0);
        glVertex2f(-1.0,-1.0);

        glEnd();
        ////////////////////////////////////////////
        
        glutSwapBuffers();

        return 0;
}

int 
Tester::loadScene(int i) 
{
        size_t wsize[2] = {window_size.width, window_size.height};

        scene.destroy();
        scene.initialize();
        if (scene_i == 0) {
                hand_set_scene(scene, wsize);
                stats_camera_pos = hand_stats_camera_pos;
                stats_camera_dir = hand_stats_camera_dir;
        }
        else if (scene_i == 1) {
                ben_set_scene(scene, wsize);
                stats_camera_pos = ben_stats_camera_pos;
                stats_camera_dir = ben_stats_camera_dir;
        }
        else if (scene_i == 2) {
                boat_set_scene(scene, wsize);
                stats_camera_pos = boat_stats_camera_pos;
                stats_camera_dir = boat_stats_camera_dir;
        }
        else if (scene_i == 3) {
                dragon_set_scene(scene, wsize);
                stats_camera_pos = dragon_stats_camera_pos;
                stats_camera_dir = dragon_stats_camera_dir;
        }
        else  {
                buddha_set_scene(scene, wsize);
                stats_camera_pos = buddha_stats_camera_pos;
                stats_camera_dir = buddha_stats_camera_dir;
        }
                        
        if (scene.create_aggregate_mesh()) { 
                std::cerr << "Failed to create aggregate mesh.\n";
                return -1;
        }
        if (scene.transfer_aggregate_mesh_to_device()) {
                std::cerr << "Failed to transfer aggregate mesh to device memory."
                          << "\n";
                return -1;
        }
        // if (scene.create_aggregate_bvh()) { 
        //         std::cerr << "Failed to create aggregate bvh.\n";
        //         return -1;
        // }
        // if (scene.transfer_aggregate_bvh_to_device()) {
        //         std::cerr << "Failed to transfer aggregate bvh to device memory."
        //                   << "\n";
        //         return -1;
        // }
                
        std::string cubemap_path = "textures/cubemap/Path/";
        if (scene.cubemap.initialize(cubemap_path + "posx.jpg",
                                     cubemap_path + "negx.jpg",
                                     cubemap_path + "posy.jpg",
                                     cubemap_path + "negy.jpg",
                                     cubemap_path + "posz.jpg",
                                     cubemap_path + "negz.jpg")) {
                std::cerr << "Failed to initialize cubemap." << "\n";
        }
                        
        scene.cubemap.enabled = true;
        directional_light_cl dl;
        dl.set_dir(0,-0.8,-0.3);
        dl.set_color(0.8,0.8,0.8);
        scene.set_dir_light(dl);

        return 0;
}

int 
Tester::setResolution(int i) 
{
        window_size = window_sizes[i];
        size_t sz[2] = {window_size.width, window_size.height};

        GLInfo* glinfo = GLInfo::instance();
        DeviceInterface* device = DeviceInterface::instance();

        delete_tex_gl(gl_tex_id);
        device->delete_memory(cl_tex_id);

        std::cerr << "Creating gl tex with size " << sz[0] << "x" << sz[1] << ".\n";

        gl_tex_id = create_tex_gl(sz[0], sz[1]);
        cl_tex_id = device->new_memory();
        DeviceMemory& tex_mem = device->memory(cl_tex_id);
        if (tex_mem.initialize_from_gl_texture(gl_tex_id)) {
                std::cerr << "Failed to create memory object from gl texture\n";
                return -1;
        }

        glinfo->resize_window(sz);
        renderer.resize_output(sz);

        glutPostRedisplay();

        return 0;
}

void 
Tester::printStatsHeaders() 
{
        renderer.log.enabled = true;
        renderer.log.silent = true;

        renderer.log << "Scene";
        renderer.log << ", " << "Resolution";
        renderer.log << ", " << "Bvh refit only";
        renderer.log << ", " << "Bvh minimum leaf size";
        renderer.log << ", " << "Bvh max depth";
        renderer.log << ", " << "Prim gen uses zcurve";
        renderer.log << ", " << "Prim gen quad size";
        renderer.log << ", " << "Sec gen uses atomics";
        renderer.log << ", " << "Sec gen separates refl/refr";
        renderer.log << ", " << "Tile to cores ratio";
        renderer.log << ", " << "Camera preset";
        renderer.log << "\n";
}

void 
Tester::printStatsValues() 
{
        renderer.log.enabled = true;
        renderer.log.silent = true;

        //// Get config values
        bool bvh_refit         = bvh_refit_only[bvh_refit_i];
        int  bvh_min_leaf_size = bvh_min_leaf_sizes[bvh_min_leaf_sizes_i];
        int  bvh_max_depth     = bvh_max_depths[bvh_max_depths_i];
        bool prim_use_zcurve   = prim_ray_use_zcurve[prim_ray_use_zcurve_i];
        int  prim_quad_size    = prim_ray_quad_sizes[prim_ray_quad_sizes_i];
        bool sec_use_atomics   = sec_ray_use_atomics[sec_ray_use_atomics_i];
        bool sec_use_disc      = sec_ray_use_disc[sec_ray_use_disc_i];
        int  tile_cores_ratio  = tile_to_cores_ratios[tile_to_cores_ratios_i];
        int  view_position     = view_positions[view_positions_i];

        renderer.log << scene_i;
        renderer.log << ", " << window_size.width << "x" << window_size.height;
        renderer.log << ", " << (bvh_refit ? "T" : "F");
        renderer.log << ", " << bvh_min_leaf_size;
        renderer.log << ", " << bvh_max_depth;
        renderer.log << ", " << (prim_use_zcurve ? "T" : "F");
        renderer.log << ", " << prim_quad_size;
        renderer.log << ", " << (sec_use_atomics ? "T" : "F");
        renderer.log << ", " << (sec_use_disc ? "T" : "F");
        renderer.log << ", " << tile_cores_ratio;
        renderer.log << ", " << view_position;
        renderer.log << "\n";
        
        // const FrameStats& stats = renderer.get_frame_stats();

        // renderer.log.enabled = false;

        //         renderer.log << "\nFrame mean time:\t" << stats.get_mean_frame_time()
        //                      << "\n";
        //         renderer.log << "Mean FPS:\t" << 1000.f/stats.get_mean_frame_time()
        //                      << "\n" << "\n";

        //         renderer.log << "Resolution:\t" << renderer.get_framebuffer_w() 
        //                      << "x" << renderer.get_framebuffer_h() << "\n";
        //         renderer.log << "Max ray bounces:\t" << renderer.get_max_bounces() << "\n";

        //         renderer.log << "\n====== Stage times\n\n" ;
        //         renderer.log << "\nBVH Build mean time:\t" 
        //                      << stats.get_stage_mean_time(BVH_BUILD) << "\n";
        //         renderer.log << "Prim Gen mean time: \t" 
        //                      << stats.get_stage_mean_time(PRIM_RAY_GEN)<< "\n";
        //         renderer.log << "Sec Gen mean time: \t" 
        //                      << stats.get_stage_mean_time(SEC_RAY_GEN)  << "\n";
        //         renderer.log << "Tracer mean time: \t" 
        //                      << stats.get_stage_mean_time(PRIM_TRACE) + 
        //                         stats.get_stage_mean_time(SEC_TRACE)
        //                      << " (" <<  stats.get_stage_mean_time(PRIM_TRACE) 
        //                      << " - " << stats.get_stage_mean_time(SEC_TRACE)
        //                      << ")" << "\n";
        //         renderer.log << "Shadow mean time: \t" 
        //                      << stats.get_stage_mean_time(PRIM_SHADOW_TRACE) + 
        //                         stats.get_stage_mean_time(SEC_SHADOW_TRACE)
        //                      << " (" <<  stats.get_stage_mean_time(PRIM_SHADOW_TRACE)
        //                      << " - " << stats.get_stage_mean_time(SEC_SHADOW_TRACE) 
        //                      << ")" << "\n";
        //         renderer.log << "Shader mean time: \t" 
        //                      << stats.get_stage_mean_time(SHADE) << "\n";
        //         renderer.log << "Fb clear mean time: \t" 
        //                      << stats.get_stage_mean_time(FB_CLEAR) << "\n";
        //         renderer.log << "Fb copy mean time: \t" 
        //                      << stats.get_stage_mean_time(FB_COPY) << "\n";
        //         renderer.log << "\n";

        renderer.log.o().flush();
}
