#include <rt/renderer-threaded.hpp>
#include <algorithm>

#include <stdio.h>
//#include <conio.h>

int32_t RendererT::set_up_frame(const memory_id tex_id, Scene& scene)
{
        (void)scene; // To avoid warning

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
        if (device.acquire_graphic_resource(target_tex_id, true)) {
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

        return 0;
}

int32_t RendererT::update_accelerator(Scene& scene, size_t cq_i)
{
        bvh_builder.logging(false);
        log.silent = false;
        
        if (bvh_builder.build_lbvh(scene, cq_i)) {
                std::cout << "BVH builder failed." << std::endl;
                return -1;
        } 
        stats.stage_times[BVH_BUILD] = bvh_builder.get_exec_time();

        return 0;
}

int32_t RendererT::render_to_framebuffer(Scene& scene)
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
                for (uint32_t bounce = 0; bounce < max_bounces; ++bounce) {


                        size_t sec_ray_in = sec_ray_count;
                        if (sec_ray_gen.generate(scene, 
                                                 *ray_in, 
                                                 sec_ray_in, 
                                                 hit_bundle, 
                                                 *ray_out, 
                                                 &sec_ray_count)) {
                                std::cerr << "Failed to create secondary rays." 
                                          << std::endl;
                                return -1;
                        }


                        // size_t sec_ray_in = sec_ray_count;
                        // if (sec_ray_gen.generate_disc(scene, 
                        //                               *ray_in, 
                        //                               sec_ray_in, 
                        //                               hit_bundle, 
                        //                               *ray_out, 
                        //                               &sec_ray_count)) {
                        //         std::cerr << "Failed to create secondary rays." 
                        //                   << std::endl;
                        //         return -1;
                        // }
                        // //// If sec_ray_count is too small, then it's very likely that 
                        // //// reflect and refract rays from the same base ray will be 
                        // //// handled by different warps at the same time. If that
                        // //// seems like it will be the case, then compute it so that
                        // //// they will be bundled together for (hopefully) the same 
                        // //// work group to analize. TODO: do this on the sec gen
                        // size_t max_count = 
                        //         4 * clinfo->max_work_group_size * clinfo->max_compute_units;
                        // if (sec_ray_count < max_count) {
                        //         if (sec_ray_gen.generate(scene, 
                        //                                  *ray_in, 
                        //                                  sec_ray_in, 
                        //                                  hit_bundle, 
                        //                                  *ray_out, 
                        //                                  &sec_ray_count)) {
                        //                 std::cerr << "Failed to create secondary rays." 
                        //                           << std::endl;
                        //                 return -1;
                        //         }
                        // }

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
                                        std::cerr << "Error tracing secondary rays" 
                                                  << std::endl;
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

        return 0;
}

int32_t RendererT::copy_framebuffer(memory_id tex_id)
{
        DeviceInterface& device = *DeviceInterface::instance();

        if (!device.good() || framebuffer.copy(device.memory(tex_id))){
                std::cerr << "Failed to copy framebuffer." << std::endl;
                return -1;
        }
        stats.stage_times[FB_COPY] = framebuffer.get_copy_exec_time();
        
        return 0;
}

int32_t RendererT::copy_framebuffer()
{
        return copy_framebuffer(target_tex_id);
}

int32_t RendererT::render_to_texture(Scene& scene, memory_id tex_id)
{
        if (render_to_framebuffer(scene))
                return -1;
        if (copy_framebuffer(tex_id))
                return -1;
        return 0;
}

int32_t RendererT::render_to_texture(Scene& scene)
{
        return render_to_texture(scene, target_tex_id);
}

int32_t RendererT::conclude_frame(Scene& scene, memory_id tex_id)
{
        (void)scene; // To avoid warning

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
        for (uint32_t i = 0; i < STAGE_COUNT; ++i)
                stats.stage_acc_times[i] += stats.stage_times[i];

        stats.acc_frames++;
        return 0;
}

int32_t RendererT::conclude_frame(Scene& scene)
{
        return conclude_frame(scene, target_tex_id);
}

int32_t RendererT::initialize_from_ini_file(std::string file_path)
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

int32_t RendererT::initialize(std::string log_filename)
{
        
        CLInfo* clinfo = clinfo->instance();
        if (!clinfo->initialized())
                return -1;

        log.initialize(log_filename);
        log.silent = false;
        log.enabled = false;

        if (bvh_builder.initialize()) {
                std::cerr << "Failed to initialize bvh builder" << std::endl;
                return -1;
        } else {
                std::cout << "Initialized bvh builder succesfully" << std::endl;
        }
        bvh_builder.set_log(&log);

        /*---------------------------- Set tile size ------------------------------*/
        size_t pixel_count = fb_w * fb_h;
        tile_size = clinfo->max_compute_units * clinfo->max_work_item_sizes[0];
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
RendererT::set_samples_per_pixel(size_t spp, pixel_sample_cl const* pixel_samples)
{
        return prim_ray_gen.set_spp(spp,pixel_samples);
}

size_t
RendererT::get_samples_per_pixel() 
{
	return prim_ray_gen.get_spp();
}


/////////////////////// Threaded only

struct BVHThreadArguments {

        RendererT* renderer;
        Scene*     last_scene;
        Scene*     new_scene;
        
};

void* bvh_thread_function(void* arg) 
{
        void *ret = (void*)0;

        BVHThreadArguments* args = (BVHThreadArguments*)arg;

        /// Thread 2
        if (args->renderer->update_accelerator(*args->new_scene, 1)) {
                std::cout << "Error updating accelerator\n";
                ret = (void*)1;
        }

        DeviceInterface& device = *DeviceInterface::instance();
        device.finish_commands(1);

        pthread_exit(ret);

        return 0;
}

int32_t RendererT::init_loop(Scene& new_scene)
{
        if (last_scene.initialize()) {
                std::cout << "Failed to initialize new scene\n"; 
                return -1;
        }

        ////// Copy scene information
        if (last_scene.copy_mem_from(new_scene)) {
                std::cout << "Failed to copy scene\n"; 
                return -1;
        }

        ////// Create accelerator
        if (update_accelerator(last_scene, 0)) {
                std::cout << "Failed to create bvh\n"; 
                return -1;
        }
        DeviceInterface& device = *DeviceInterface::instance();
        device.finish_commands(0);

        return 0;
}

int32_t RendererT::render_one_frame(Scene& new_scene, memory_id tex_id)
{

        BVHThreadArguments args;
        args.renderer = this;
        args.last_scene = &this->last_scene;
        args.new_scene = &new_scene;
                
        pthread_create( &bvh_thread, NULL, &bvh_thread_function, &args);

        /// Thread 1
        if (set_up_frame(tex_id, last_scene)) {
                std::cout << "Error setting up frame\n";
                return -1;
        }
                
        if (render_to_framebuffer(last_scene)) {
                std::cout << "Error rendering frame\n";
                return -1;
        }

        if (copy_framebuffer()) {
                std::cout << "Error copying framebuffer\n";
                return -1;
        }

        if (conclude_frame(last_scene)) {
                std::cout << "Error concluding frame\n";
                return -1;
        }



        void* thread_ret;
        pthread_join(bvh_thread, &thread_ret);
        if (thread_ret) {
                std::cout << "Pthread return an error!\n";
                return -1;
        }

        /////////////// Once finished
        DeviceInterface& device = *DeviceInterface::instance();
        device.finish_commands(0);
        device.finish_commands(1);

        if (last_scene.copy_mem_from(new_scene)) {
                std::cout << "Failed to copy scene\n"; 
                return -1;
        }
        device.finish_commands(0);
        device.finish_commands(1);

        return 0;

}


int32_t RendererT::finalize_loop()
{
        last_scene.destroy();
        return 0;
}

