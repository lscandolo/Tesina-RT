#include <rt/renderer.hpp>
#include <algorithm>

#include <stdio.h>
#include <conio.h>

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
                std::cerr << "Error acquiring texture resource." << std::endl;
                return -1;
        }
        
        // Clear the framebuffer
        if (framebuffer.clear()) {
                std::cerr << "Failed to clear framebuffer." << std::endl;
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
        bvh_builder.logging(true);
        log.silent = false;
        
        if (!static_bvh || !scene.ready()) {
                if (bvh_builder.build_bvh(scene)) {
                        std::cout << "BVH builder failed." << std::endl;
                        return -1;
                } 
                stats.stage_times[BVH_BUILD] = bvh_builder.get_exec_time();
        }
        return 0;
}

uint32_t Renderer::render_to_framebuffer(Scene& scene)
{
        size_t pixel_count  = fb_w * fb_h;
        size_t sample_count = pixel_count * prim_ray_gen.get_spp();
        size_t fb_size[] = {fb_w, fb_h};
        size_t current_tile_size = tile_size;

        for (size_t offset = 0; offset < sample_count; offset+= current_tile_size) {


                RayBundle* ray_in =  &ray_bundle_1;
                RayBundle* ray_out = &ray_bundle_2;

                if (sample_count - offset < current_tile_size)
                        current_tile_size = sample_count - offset;

                if (prim_ray_gen.generate(scene.camera, *ray_in, fb_size,
                    current_tile_size, offset)) {
                         std::cerr << "Error seting primary ray bundle" << std::endl;
                         return -1;
                }
                stats.stage_times[PRIM_RAY_GEN] += prim_ray_gen.get_exec_time();
                stats.total_ray_count += current_tile_size;

                if (tracer.trace(scene, current_tile_size, *ray_in, hit_bundle)) {
                        std::cerr << "Error tracing primary rays" << std::endl;
                        return -1;
                }
                stats.stage_times[PRIM_TRACE] += tracer.get_trace_exec_time();

                if (tracer.shadow_trace(scene, current_tile_size, *ray_in, hit_bundle)) {
                        std::cerr << "Error shadow tracing primary rays" << std::endl;
                        return - 1;
                }
                stats.stage_times[PRIM_SHADOW_TRACE] += tracer.get_shadow_exec_time();
                
                if (ray_shader.shade(*ray_in, hit_bundle, scene,
                    scene.cubemap, framebuffer, current_tile_size,true)){
                         std::cerr << "Failed to update framebuffer." << std::endl;
                         return -1;
                }
                stats.stage_times[SHADE] += ray_shader.get_exec_time();

                size_t sec_ray_count = current_tile_size;
                for (uint32_t i = 0; i < max_bounces; ++i) {

                        if (sec_ray_gen.generate(scene, *ray_in, sec_ray_count, 
                                hit_bundle, *ray_out, &sec_ray_count)) {
                                        std::cerr << "Failed to create secondary rays." 
                                                << std::endl;
                                        return -1;
                        }
                        stats.stage_times[SEC_RAY_GEN] += sec_ray_gen.get_exec_time();

                        std::swap(ray_in,ray_out);

                        if (!sec_ray_count)
                                break;
                        if (sec_ray_count == ray_out->count())
                                std::cerr << "Max sec rays reached!\n";

                        stats.total_ray_count += sec_ray_count;
                        stats.total_sec_ray_count += sec_ray_count;

                        if (tracer.trace(scene, sec_ray_count, 
                                *ray_in, hit_bundle, true)) {
                                        std::cerr << "Error tracing secondary rays" << std::endl;
                                        return -1;
                        }

                        stats.stage_times[SEC_TRACE] += tracer.get_trace_exec_time();
                        
                        if (tracer.shadow_trace(scene, sec_ray_count, 
                                *ray_in, hit_bundle, true)) {
                                        std::cerr << "Error shadow tracing primary rays" 
                                                << std::endl;
                                        return -1;
                        }
                        stats.stage_times[SEC_SHADOW_TRACE] += tracer.get_shadow_exec_time();

                        if (ray_shader.shade(*ray_in, hit_bundle, scene,
                                scene.cubemap, framebuffer, sec_ray_count)){
                                        std::cerr << "Ray shader failed execution." << std::endl;
                                        return -1;
                        }
                        stats.stage_times[SHADE] += ray_shader.get_exec_time();
                }
        }

        log.silent = true;
        return 0;
}

uint32_t Renderer::copy_framebuffer(memory_id tex_id)
{
        DeviceInterface& device = *DeviceInterface::instance();

        if (!device.good() || framebuffer.copy(device.memory(tex_id))){
                std::cerr << "Failed to copy framebuffer." << std::endl;
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
        // Obtain device handle and check it's ok
        DeviceInterface& device = *DeviceInterface::instance();
        if (!device.good())
                return -1;
        
        // Release render texture resource
        if (device.release_graphic_resource(tex_id)) {
                //|| scene.release_graphic_resources()) { // Scene release is not needed
            std::cerr << "Error releasing texture resource." << std::endl;
            return -1;
        }
        device.finish_commands();

        stats.frame_time = frame_timer.msec_since_snap();
        stats.frame_acc_time += stats.frame_time;
        for (uint32_t i = 0; i < rt_stages; ++i)
                stats.stage_acc_times[i] += stats.stage_times[i];

        stats.acc_frames++;
        return 0;
}

uint32_t Renderer::conclude_frame(Scene& scene)
{
        return conclude_frame(scene, target_tex_id);
}

uint32_t Renderer::initialize_from_ini_file(std::string file_path)
{
        int32_t ini_err;
        ini_err = ini.load_file(file_path);
        if (ini_err < 0)
                std::cout << "Error at ini file line: " << -ini_err << std::endl;
        else if (ini_err > 0)
                std::cout << "Unable to open ini file" << std::endl;
        else {
                int32_t int_val;
                std::string str_val;
                if (!ini.get_int_value("RT", "screen_w", int_val))
                        fb_w = int_val;
                if (!ini.get_int_value("RT", "screen_h", int_val))
                        fb_h = int_val;
                if (!ini.get_int_value("RT", "gpu_bvh", int_val))
                        static_bvh = !int_val;
                if (!ini.get_int_value("RT", "max_bounce", int_val)) 
                        max_bounces = int_val;
        }        return 0;

}

uint32_t Renderer::initialize(CLInfo clinfo, std::string log_filename)
{
        
        log.initialize(log_filename);
        log.silent = false;
        log.enabled = false;

        if (bvh_builder.initialize(clinfo)) {
                std::cerr << "Failed to initialize bvh builder" << std::endl;
                return -1;
        } else {
                std::cout << "Initialized bvh builder succesfully" << std::endl;
        }
        bvh_builder.set_log(&log);

        /*---------------------------- Set tile size ------------------------------*/
        size_t pixel_count = fb_w * fb_h;
        tile_size = clinfo.max_compute_units * clinfo.max_work_item_sizes[0];
        tile_size *= 128;
        tile_size = std::min(pixel_count, tile_size);

        /*---------------------- Initialize ray bundles -----------------------------*/
        size_t ray_bundle_size = tile_size * 3;

        if (ray_bundle_1.initialize(ray_bundle_size)) {
                std::cerr << "Error initializing ray bundle" << std::endl;
                std::cerr.flush();
                return -1;
        }

        if (ray_bundle_2.initialize(ray_bundle_size)) {
                std::cerr << "Error initializing ray bundle 2" << std::endl;
                std::cerr.flush();
                return -1;
        }
        std::cout << "Initialized ray bundles succesfully" << std::endl;

        /*---------------------- Initialize hit bundle -----------------------------*/
        size_t hit_bundle_size = ray_bundle_size;

        if (hit_bundle.initialize(hit_bundle_size)) {
                std::cerr << "Error initializing hit bundle" << std::endl;
                std::cerr.flush();
                return -1;
        }
        std::cout << "Initialized hit bundle succesfully" << std::endl;

        /*------------------------ Initialize FrameBuffer ---------------------------*/
        size_t fb_size[] = {fb_w, fb_h};
        if (framebuffer.initialize(fb_size)) {
                std::cerr << "Error initializing framebuffer." << std::endl;
                return -1;
        }
        std::cout << "Initialized framebuffer succesfully." << std::endl;

        /* ------------------ Initialize ray tracer kernel ----------------------*/

        if (tracer.initialize()){
                std::cerr << "Failed to initialize tracer." << std::endl;
                return -1;
        }
        std::cerr << "Initialized tracer succesfully." << std::endl;


        /* ------------------ Initialize Primary Ray Generator ----------------------*/

        if (prim_ray_gen.initialize()) {
                std::cerr << "Error initializing primary ray generator." << std::endl;
                return -1;
        }
        std::cout << "Initialized primary ray generator succesfully." << std::endl;
                

        /* ------------------ Initialize Secondary Ray Generator ----------------------*/
        if (sec_ray_gen.initialize()) {
                std::cerr << "Error initializing secondary ray generator." << std::endl;
                return -1;
        }
        sec_ray_gen.set_max_rays(ray_bundle_1.count());
        std::cout << "Initialized secondary ray generator succesfully." << std::endl;

        /*------------------------ Initialize RayShader ---------------------------*/
        if (ray_shader.initialize()) {
                std::cerr << "Error initializing ray shader." << std::endl;
                return -1;
        }
        std::cout << "Initialized ray shader succesfully." << std::endl;

        /*----------------------- Enable timing in all clases -------------------*/
        bvh_builder.timing(true);
        framebuffer.timing(true);
        prim_ray_gen.timing(true);
        sec_ray_gen.timing(true);
        tracer.timing(true);
        ray_shader.timing(true);

        clear_stats();

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
