#include <rt/renderer.hpp>
#include <algorithm>

#include <stdio.h>

Renderer::Renderer()
{
        initialized = false;
        config.set_target(this);
}

uint32_t Renderer::set_up_frame(const memory_id tex_id, Scene& scene)
{
        // Initialize frame timer
        frame_timer.snap_time();

        // Obtain device handle and check it's ok
        DeviceInterface& device = *DeviceInterface::instance();
        if (!device.good())
                return -1;

        // Keep a copy of tex_id
        target_tex_id = tex_id;

        //Set time variables to 0
        stats.clear_times();

        //Set ray counters to 0
        stats.total_ray_count = 0;
        stats.total_sec_ray_count = 0;

        // Acquire resources for render texture
        if (device.acquire_graphic_resource(target_tex_id)) {
                //|| scene.acquire_graphic_resources()) {// Scene acquire is not needed
                std::cerr << "Error acquiring texture resource." << "\n";
                return -1;
        }
        
        // Clear the framebuffer
        if (framebuffer.clear()) {
                std::cerr << "Failed to clear framebuffer." << "\n";
                return -1;
        }
        stats.stage_times[FB_CLEAR] = framebuffer.get_clear_exec_time();

        // Create accelerator (if needed)
        if (update_accelerator(scene))
                return -1;

        return 0;
}

uint32_t Renderer::update_accelerator(Scene& scene)
{
        bvh_builder.logging(false);

        AcceleratorType type = scene.get_accelerator_type();

        if (config.use_lbvh) {
                if (config.bvh_refit_only && 
                    type == LBVH_ACCELERATOR && 
                    scene.ready()) {
                        if (bvh_builder.refit_lbvh(scene)) {
                                std::cout << "BVH refitting failed." << "\n";
                                return -1;
                        }
                } else { 
                        if (bvh_builder.build_lbvh(scene)) {
                                std::cout << "BVH building failed." << "\n";
                                return -1;
                        } 
                }
        }

        stats.stage_times[BVH_BUILD] = bvh_builder.get_exec_time();
        return 0;
}

uint32_t Renderer::update_configuration()
{
        CLInfo* clinfo = clinfo->instance();

        size_t old_tile_size = tile_size;
        size_t pixel_count = fb_w * fb_h;
        tile_size = clinfo->max_compute_units * clinfo->max_work_item_sizes[0];
        tile_size *= config.tile_to_cores_ratio;
        tile_size = std::min(pixel_count, tile_size);

        if (tile_size != old_tile_size) {
                size_t ray_bundle_size = tile_size * 3;
                size_t hit_bundle_size = ray_bundle_size;
                
                std::cout << "ray_bundle_size: " << ray_bundle_size << std::endl;

                if (ray_bundle_1.resize(ray_bundle_size) || 
                    ray_bundle_2.resize(ray_bundle_size) || 
                    hit_bundle.resize(hit_bundle_size)) {
                        std::cerr << "Error resizing ray and hit bundles.\n";
                        return -1;
                }
                sec_ray_gen.set_max_rays(ray_bundle_1.count());
        }

        bvh_builder.update_configuration(config);
        prim_ray_gen.update_configuration(config);
        sec_ray_gen.update_configuration(config);
        tracer.update_configuration(config);
        ray_shader.update_configuration(config);
        
        return 0;
}

uint32_t Renderer::render_to_framebuffer(Scene& scene)
{
        CLInfo* clinfo = clinfo->instance();
        size_t pixel_count  = fb_w * fb_h;
        size_t sample_count = pixel_count * prim_ray_gen.get_spp();
        size_t fb_size[] = {fb_w, fb_h};

        for (size_t offset = 0; offset < sample_count; offset+= tile_size) {

                RayBundle* ray_in =  &ray_bundle_1;
                RayBundle* ray_out = &ray_bundle_2;

                size_t current_tile_size = tile_size;
                if (sample_count - offset < tile_size)
                        current_tile_size = sample_count - offset;

                if (prim_ray_gen.generate(scene.camera, *ray_in, fb_size,
                    current_tile_size, offset)) {
                         std::cerr << "Error seting primary ray bundle.\n";
                         return -1;
                }


                stats.stage_times[PRIM_RAY_GEN] += prim_ray_gen.get_exec_time();
                stats.total_ray_count += current_tile_size;

                if (tracer.trace(scene, current_tile_size, *ray_in, hit_bundle)) {
                        std::cerr << "Error tracing primary rays.\n";
                        return -1;
                }
                stats.stage_times[PRIM_TRACE] += tracer.get_trace_exec_time();

                if (tracer.shadow_trace(scene, current_tile_size, *ray_in, hit_bundle)) {
                        std::cerr << "Error shadow tracing primary rays.\n";
                        return - 1;
                }
                stats.stage_times[PRIM_SHADOW_TRACE] += tracer.get_shadow_exec_time();
                
                if (ray_shader.shade(*ray_in, hit_bundle, scene,
                    scene.cubemap, framebuffer, current_tile_size,true)){
                         std::cerr << "Failed to update framebuffer.\n";
                         return -1;
                }
                stats.stage_times[SHADE] += ray_shader.get_exec_time();

                size_t sec_ray_count = current_tile_size;
                for (uint32_t bounce = 0; bounce < max_bounces; ++bounce) {



                        size_t sec_ray_in = sec_ray_count;
                        if (sec_ray_gen.generate(scene, *ray_in, sec_ray_in, 
                                                 hit_bundle, *ray_out, &sec_ray_count)) {
                                std::cerr << "Failed to create secondary rays." 
                                          << "\n";
                                return -1;
                        }

                        // std::cout << "sec ray count: " << sec_ray_count << std::endl;
                        // exit(0);


                        stats.stage_times[SEC_RAY_GEN] += sec_ray_gen.get_exec_time();

                        std::swap(ray_in,ray_out);

                        if (!sec_ray_count)
                                break;
                        if (sec_ray_count == (size_t)ray_out->count())
                                std::cerr << "Max sec rays reached!\n";

                        stats.total_ray_count += sec_ray_count;
                        stats.total_sec_ray_count += sec_ray_count;

                        if (tracer.trace(scene, sec_ray_count, 
                                *ray_in, hit_bundle, true)) {
                                std::cerr << "Error tracing secondary rays\n";
                                return -1;
                        }

                        stats.stage_times[SEC_TRACE] += tracer.get_trace_exec_time();
                        
                        if (tracer.shadow_trace(scene, sec_ray_count, 
                                *ray_in, hit_bundle, true)) {
                                std::cerr << "Error shadow tracing primary rays\n" ;
                                return -1;
                        }

                        stats.stage_times[SEC_SHADOW_TRACE] += 
                                tracer.get_shadow_exec_time();

                        if (ray_shader.shade(*ray_in, hit_bundle, scene,
                                scene.cubemap, framebuffer, sec_ray_count)){
                                std::cerr << "Ray shader failed execution.\n";
                                return -1;
                        }
                        stats.stage_times[SHADE] += ray_shader.get_exec_time();
                }
        }

        return 0;
}

uint32_t Renderer::copy_framebuffer(memory_id tex_id)
{
        DeviceInterface& device = *DeviceInterface::instance();

        if (!device.good() || framebuffer.copy(device.memory(tex_id))){
                std::cerr << "Failed to copy framebuffer." << "\n";
                return -1;
        }
        stats.stage_times[FB_COPY] = framebuffer.get_copy_exec_time();
        
        return 0;
}

uint32_t Renderer::copy_framebuffer()
{
        return copy_framebuffer(target_tex_id);
}

uint32_t Renderer::render_to_texture(Scene& scene, memory_id tex_id)
{
        if (render_to_framebuffer(scene))
                return -1;
        if (copy_framebuffer(tex_id))
                return -1;
        return 0;
}

uint32_t Renderer::render_to_texture(Scene& scene)
{
        return render_to_texture(scene, target_tex_id);
}

uint32_t Renderer::conclude_frame(Scene& scene, memory_id tex_id)
{
        (void)scene;

        // Obtain device handle and check it's ok
        DeviceInterface& device = *DeviceInterface::instance();
        if (!device.good())
                return -1;
        
        // Release render texture resource
        if (device.release_graphic_resource(tex_id)) {
                //|| scene.release_graphic_resources()) { // Scene release is not needed
            std::cerr << "Error releasing texture resource." << "\n";
            return -1;
        }
        device.finish_commands();

        stats.frame_time = frame_timer.msec_since_snap();
        stats.frame_acc_time += stats.frame_time;
        for (uint32_t i = 0; i < STAGE_COUNT; ++i)
                stats.stage_acc_times[i] += stats.stage_times[i];

        stats.acc_frames++;
        return 0;
}

uint32_t Renderer::conclude_frame(Scene& scene)
{
        return conclude_frame(scene, target_tex_id);
}

uint32_t Renderer::configure_from_defaults()
{
        fb_w = 512;
        fb_h = 512;
        static_bvh = false;
        max_bounces = 9;

        config.bvh_depth = 32;
        config.tile_to_cores_ratio = 128;
        config.use_lbvh = true;
        config.bvh_refit_only = false;
        config.sec_ray_use_disc = false;
        config.prim_ray_quad_size = 32;
        config.prim_ray_use_zcurve = false;

        return 0;
}

uint32_t Renderer::configure_from_ini_file(std::string file_path)
{
        int32_t ini_err;
        ini_err = ini.load_file(file_path);
        if (ini_err < 0)
                std::cout << "Error at ini file line: " << -ini_err << "\n";
        else if (ini_err > 0)
                std::cout << "Unable to open ini file" << "\n";
        else {
                int32_t int_val;
                float   float_val;
                std::string str_val;
                if (!ini.get_int_value("RT", "screen_w", int_val))
                        fb_w = int_val;

                if (!ini.get_int_value("RT", "screen_h", int_val))
                        fb_h = int_val;

                if (!ini.get_int_value("RT", "gpu_bvh", int_val))
                        static_bvh = !int_val;

                if (!ini.get_int_value("RT", "max_bounce", int_val)) 
                        max_bounces = int_val;
                
                if (!ini.get_int_value("Renderer", "bvh_refit_only", int_val))
                        config.bvh_refit_only = int_val;

                if (!ini.get_int_value("Renderer", "bvh_max_depth", int_val))
                        config.bvh_depth = int_val;

                if (!ini.get_int_value("Renderer", "bvh_min_leaf_size", int_val))
                        config.bvh_min_leaf_size = int_val;

                if (!ini.get_int_value("Renderer", "sec_use_atomics", int_val))
                        config.sec_ray_use_atomics = int_val;

                if (!ini.get_int_value("Renderer", "sec_use_disc", int_val))
                        config.sec_ray_use_disc = int_val;

                if (!ini.get_int_value("Renderer", "prim_use_zcurve", int_val))
                        config.prim_ray_use_zcurve = int_val;

                if (!ini.get_int_value("Renderer", "prim_quad_size", int_val))
                        config.prim_ray_quad_size = int_val;

                if (!ini.get_float_value("Renderer", "tile_to_cores_ratio", float_val))
                        config.tile_to_cores_ratio = float_val;
        }

        return 0;

}

uint32_t Renderer::initialize(std::string log_filename)
{
        
        CLInfo* clinfo = clinfo->instance();
        if (!clinfo->initialized() || initialized)
                return -1;

        log.initialize(log_filename);
        log.silent = false;
        log.enabled = false;

        if (bvh_builder.initialize()) {
                std::cerr << "Failed to initialize bvh builder" << "\n";
                return -1;
        } else {
                std::cout << "Initialized bvh builder succesfully." << "\n";
        }
        bvh_builder.set_log(&log);

        /*---------------------------- Set tile size ------------------------------*/
        size_t pixel_count = fb_w * fb_h;
        tile_size = clinfo->max_compute_units * clinfo->max_work_item_sizes[0];
        tile_size *= config.tile_to_cores_ratio;
        tile_size = std::min(pixel_count, tile_size);

        /*---------------------- Initialize ray bundles -----------------------------*/
        size_t ray_bundle_size = tile_size * 3;

        if (ray_bundle_1.initialize(ray_bundle_size)) {
                std::cerr << "Error initializing ray bundle" << "\n";
                std::cerr.flush();
                return -1;
        }

        if (ray_bundle_2.initialize(ray_bundle_size)) {
                std::cerr << "Error initializing ray bundle 2" << "\n";
                std::cerr.flush();
                return -1;
        }
        std::cout << "Initialized ray bundles succesfully." << "\n";

        /*---------------------- Initialize hit bundle -----------------------------*/
        size_t hit_bundle_size = ray_bundle_size;

        if (hit_bundle.initialize(hit_bundle_size)) {
                std::cerr << "Error initializing hit bundle" << "\n";
                std::cerr.flush();
                return -1;
        }
        std::cout << "Initialized hit bundle succesfully." << "\n";

        /*------------------------ Initialize FrameBuffer ---------------------------*/
        size_t fb_size[] = {fb_w, fb_h};
        if (framebuffer.initialize(fb_size)) {
                std::cerr << "Error initializing framebuffer." << "\n";
                return -1;
        }
        std::cout << "Initialized framebuffer succesfully." << "\n";

        /* ------------------ Initialize ray tracer kernel ----------------------*/
        if (tracer.initialize()){
                std::cerr << "Failed to initialize tracer." << "\n";
                return -1;
        }
        std::cerr << "Initialized tracer succesfully." << "\n";


        /* ------------------ Initialize Primary Ray Generator ----------------------*/
        if (prim_ray_gen.initialize()) {
                std::cerr << "Error initializing primary ray generator." << "\n";
                return -1;
        }
        std::cout << "Initialized primary ray generator succesfully." << "\n";
                

        /* ------------------ Initialize Secondary Ray Generator ----------------------*/
        if (sec_ray_gen.initialize()) {
                std::cerr << "Error initializing secondary ray generator." << "\n";
                return -1;
        }
        sec_ray_gen.set_max_rays(ray_bundle_1.count());
        std::cout << "Initialized secondary ray generator succesfully." << "\n";

        /*------------------------ Initialize RayShader ---------------------------*/
        if (ray_shader.initialize()) {
                std::cerr << "Error initializing ray shader." << "\n";
                return -1;
        }
        std::cout << "Initialized ray shader succesfully." << "\n";

        /*----------------------- Enable timing in all clases -------------------*/
        bvh_builder.timing(true);
        framebuffer.timing(true);
        prim_ray_gen.timing(true);
        sec_ray_gen.timing(true);
        tracer.timing(true);
        ray_shader.timing(true);

        clear_stats();

        initialized = true;
        
        return 0;
}

uint32_t Renderer::resize_output(size_t width, size_t height)
{

        size_t sz[2] = {width, height};
        if (sz[0] == 0 || sz[1] == 0 || !initialized)
                return -1;

        CLInfo* clinfo = clinfo->instance();
        if (!clinfo->initialized())
                return -1;

        fb_w = sz[0];
        fb_h = sz[1];

        /*---------------------------- Set tile size ------------------------------*/
        size_t pixel_count = fb_w * fb_h;
        tile_size = clinfo->max_compute_units * clinfo->max_work_item_sizes[0];
        tile_size *= config.tile_to_cores_ratio;
        tile_size = std::min(pixel_count, tile_size);

        /*---------------------- Resize ray bundles -----------------------------*/
        size_t ray_bundle_size = tile_size * 3;

        if (ray_bundle_1.resize(ray_bundle_size)) {
                std::cerr << "Error resizing ray bundle (new size: " 
                          << ray_bundle_size << ")\n";
                std::cerr.flush();
                return -1;
        }

        if (ray_bundle_2.resize(ray_bundle_size)) {
                std::cerr << "Error resizing ray bundle (new size: " 
                          << ray_bundle_size << ")\n";
                std::cerr.flush();
                return -1;
        }

        /*---------------------- Resize hit bundle -----------------------------*/
        size_t hit_bundle_size = ray_bundle_size;

        if (hit_bundle.resize(hit_bundle_size)) {
                std::cerr << "Error resizing hit bundle (new size: " 
                          << ray_bundle_size << ")\n";
                std::cerr.flush();
                return -1;
        }

        /*------------------------ Resize FrameBuffer ---------------------------*/
        if (framebuffer.resize(sz)) {
                std::cerr << "Error resizing framebuffer." << "\n";
                return -1;
        }

        return 0;

}

int32_t 
Renderer::set_samples_per_pixel(size_t spp, pixel_sample_cl const* pixel_samples)
{
        return prim_ray_gen.set_spp(spp,pixel_samples);
}

size_t
Renderer::get_samples_per_pixel() 
{
	return prim_ray_gen.get_spp();
}
