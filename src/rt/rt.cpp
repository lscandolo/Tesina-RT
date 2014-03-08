#include <algorithm>
#include <iostream>
#include <sstream>

#include <cl-gl/opengl-init.hpp>
#include <cl-gl/opencl-init.hpp>
#include <rt/rt.hpp>

#include <rt/test-params-buddha.hpp>
#include <rt/test-params.hpp>

#define TOTAL_STATS_TO_LOG 12
#define CONF_STATS_TO_LOG  4

/*
 *  Debug Declaration Area
 */

rt_time_t debug_timer;
FrameStats debug_stats;

/*
 *
 */

size_t frame = 1000;
Renderer renderer;

memory_id tex_id;

int model_count   = 44; 
int current_model = 0;
Scene scene;

GLuint gl_tex;

bool  print_fps     = true;
bool logging        = false;
bool logged         = false;
bool custom_logging = false;
int  stats_logged   = 0;

void gl_mouse(int x, int y)
{

        size_t window_size[] = {renderer.get_framebuffer_w(), renderer.get_framebuffer_h()};
        float delta = 0.001f;
        float d_inc = delta * (window_size[1]*0.5f - y);/* y axis points downwards */ 
        float d_yaw = delta * (x - window_size[0]*0.5f);

        d_inc = std::min(std::max(d_inc, -0.1f), 0.1f);
        d_yaw = std::min(std::max(d_yaw, -0.1f), 0.1f);

        if (d_inc == 0.f && d_yaw == 0.f)
                return;

        scene.camera.modifyAbsYaw(d_yaw);
        scene.camera.modifyPitch(d_inc);
        glutWarpPointer(window_size[0] * 0.5f, window_size[1] * 0.5f);
}

void gl_key(unsigned char key, int x, int y)
{
        float delta = 2.f;

        static float scale = 1.f;
        static float tilt = 0.f;

        size_t window_size[] = {renderer.get_framebuffer_w(), renderer.get_framebuffer_h()};

        const pixel_sample_cl samples1[] = {{ 0.f , 0.f, 1.f}};
        const pixel_sample_cl samples4[] = {{ 0.25f , 0.25f, 0.25f},
                                      { 0.25f ,-0.25f, 0.25f},
                                      {-0.25f , 0.25f, 0.25f},
                                      {-0.25f ,-0.25f, 0.25f}};

        switch (key){
        case '+':
                renderer.set_max_bounces(std::min(renderer.get_max_bounces()+1, 10u));
                break;
        case '-':
                renderer.set_max_bounces(std::min(renderer.get_max_bounces()-1, 0u));
                break;
        case 'a':
                scene.camera.panRight(-delta*scale);
                break;
        case 's':
                scene.camera.panForward(-delta*scale);
                break;
        case 'w':
                scene.camera.panForward(delta*scale);
                break;
        case 'd':
                scene.camera.panRight(delta*scale);
                break;
        case 'p':
                std::cout << std::endl;
                std::cout << "scene.camera Pos:\n" << scene.camera.pos << std::endl;
                std::cout << "scene.camera Dir:\n" << scene.camera.dir << std::endl;
                break;
        case 'u':
                scene.camera.set(stats_camera_pos[0],//pos 
                           stats_camera_dir[0],//dir
                           makeVector(0,1,0), //up
                           M_PI/4.,
                           window_size[0] / (float)window_size[1]);
        
                break;
        case 'i':
                scene.camera.set(stats_camera_pos[1],//pos 
                           stats_camera_dir[1],//dir
                           makeVector(0,1,0), //up
                           M_PI/4.,
                           window_size[0] / (float)window_size[1]);
        
                break;
        case 'o':
                scene.camera.set(stats_camera_pos[2],//pos 
                           stats_camera_dir[2],//dir
                           makeVector(0,1,0), //up
                           M_PI/4.,
                           window_size[0] / (float)window_size[1]);
        
                break;
        case 'b':
                renderer.log.enabled = true;
                renderer.log.silent = !renderer.log.silent;
                break;
        case 'f':
                print_fps = !print_fps;
                break;
        case 'x':
                renderer.set_static_bvh(!renderer.is_static_bvh());
                break;
        case 'e':
                CLInfo::instance()->set_sync(!CLInfo::instance()->sync());
                break;
        case 'c':
                scene.cubemap.enabled = !scene.cubemap.enabled;
                break;
        case '1': /* Set 1 sample per pixel */
                if (renderer.set_samples_per_pixel(1,samples1)){
                        std::cerr << "Error seting spp" << std::endl;
                        pause_and_exit(1);
                }
                break;
        case '4': /* Set 4 samples per pixel */
                if (renderer.set_samples_per_pixel(4,samples4)){
                        std::cerr << "Error seting spp" << std::endl;
                        pause_and_exit(1);
                }
                break;
        case 'q':
                // std::cout << std::endl << "pause_and_exiting..." << std::endl;
                // pause_and_exit(1);
                exit(1);
                break;
        case 't':
                tilt -= 0.01f;
                scale *= 1.5f;
                scene.update_bvh_roots();
                break;
        case 'y':
                tilt += 0.01f;
                scale /= 1.5f;
                scene.update_bvh_roots();
                break;
        case 'm':
                current_model = 0;
                break;
        }
}

void gl_loop()
{

        debug_timer.snap_time();

        glClear(GL_COLOR_BUFFER_BIT);

        //Acquire graphics library resources, clear framebuffer, 
        // clear counters and create bvh if needed
        if (renderer.set_up_frame(tex_id ,scene)) {
                std::cerr << "Error in setting up frame" << std::endl;
                exit(-1);
        }

        //Set camera parameters (if needed)
        CameraTrajectory* cam_traj;
        cam_traj = &buddha_cam_traj;

        debug_stats.stage_acc_times[0] += debug_timer.msec_since_snap();


        vec3 cam_pos,cam_up,cam_dir;
        if (frame > 0 && frame < 100) {
                cam_traj->get_next_camera_params(&cam_pos, &cam_dir, &cam_up);
                float FOV = M_PI/4.f;
                float aspect = renderer.get_framebuffer_w()/(float)renderer.get_framebuffer_h(); 
                scene.camera.set(cam_pos,cam_dir,cam_up, FOV, aspect);
        }
        
        debug_stats.stage_acc_times[1] += debug_timer.msec_since_snap();//!!

        //Render image to framebuffer and copy to set up texture
        //renderer.render_to_texture(scene);

        renderer.render_to_framebuffer(scene);//!!
        
        debug_stats.stage_acc_times[2] += debug_timer.msec_since_snap();//!!

        renderer.copy_framebuffer();//!!

        debug_stats.stage_acc_times[3] += debug_timer.msec_since_snap();//!!

        //Do cleanup after rendering frame
        if (renderer.conclude_frame(scene)) {
                std::cerr << "Error concluding frame" << std::endl;
                exit(-1);
        }

        debug_stats.stage_acc_times[4] += debug_timer.msec_since_snap();//!!

        ////////////////// Immediate mode textured quad
        glLoadIdentity();
        glBindTexture(GL_TEXTURE_2D, gl_tex);

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
        
        debug_stats.stage_acc_times[5] += debug_timer.msec_since_snap();//!!

        frame++;
        //std::cout << "Camera Pos: " << scene.camera.pos << std::endl;
        const FrameStats& stats = renderer.get_frame_stats();

        if (print_fps) {
                glRasterPos2f(-0.95f,0.9f);
                std::stringstream ss;
                ss << "FPS: " << (1000.f / stats.get_frame_time());
                if (frame < 100) {
                        ss << "\n  LOGGING";
                }
                std::string fps_s = ss.str();
                for (uint32_t i = 0; i < fps_s.size(); ++i)
                        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *(fps_s.c_str()+i));
        }


        glutSwapBuffers();

        if (frame == 1){
                debug_stats.clear_mean_times();//!!
                debug_stats.clear_times();//!!
                renderer.clear_stats();
        }

        debug_stats.acc_frames++;//!!
        if (frame == 100 && false) {
                renderer.log.enabled = true;
                renderer.log << "========================================================\n";
                renderer.log << "========================================================\n";
                renderer.log << "                      BUDDHA SCENE\n";
                renderer.log << "========================================================\n";
                renderer.log << "========================================================\n";

                renderer.log << "\nFrame mean time:\t" << stats.get_mean_frame_time()
                             << std::endl;
                renderer.log << "Mean FPS:\t" << 1000.f/stats.get_mean_frame_time()
                             << std::endl << std::endl;

                renderer.log << "Resolution:\t" << renderer.get_framebuffer_w() << "x" << renderer.get_framebuffer_h() << std::endl;
                renderer.log << "Max ray bounces:\t" << renderer.get_max_bounces() << std::endl;
                
                renderer.log << "\n====== Stage times\n\n" ;
                renderer.log << "\nBVH Build mean time:\t" << stats.get_stage_mean_time(BVH_BUILD) << std::endl;
                renderer.log << "Prim Gen mean time: \t" << stats.get_stage_mean_time(PRIM_RAY_GEN)<< std::endl;
                renderer.log << "Sec Gen mean time: \t" << stats.get_stage_mean_time(SEC_RAY_GEN)  << std::endl;
                renderer.log << "Tracer mean time: \t" << stats.get_stage_mean_time(PRIM_TRACE) + stats.get_stage_mean_time(SEC_TRACE)
                        << " (" <<  stats.get_stage_mean_time(PRIM_TRACE) << " - " << stats.get_stage_mean_time(SEC_TRACE)
                        << ")" << std::endl;
                renderer.log << "Shadow mean time: \t" << stats.get_stage_mean_time(PRIM_SHADOW_TRACE) + stats.get_stage_mean_time(SEC_SHADOW_TRACE)
                        << " (" <<  stats.get_stage_mean_time(PRIM_SHADOW_TRACE)
                        << " - " << stats.get_stage_mean_time(SEC_SHADOW_TRACE) << ")" << std::endl;
                renderer.log << "Shader mean time: \t" << stats.get_stage_mean_time(SHADE) << std::endl;
                renderer.log << "Fb clear mean time: \t" << stats.get_stage_mean_time(FB_CLEAR) << std::endl;
                renderer.log << "Fb copy mean time: \t" << stats.get_stage_mean_time(FB_COPY) << std::endl;
                renderer.log << std::endl;

                ////////// Debug Info
                renderer.log << "\n\n\nUPTO Stats: \n";
                renderer.log << "Up to start_up_frame:\t" << debug_stats.get_stage_mean_time((rt_stage)0)
                             << std::endl;
                renderer.log << "Up to camera.set:\t" << debug_stats.get_stage_mean_time(rt_stage(1))
                             << std::endl;
                renderer.log << "Up to render_to_framebuffer:\t" << debug_stats.get_stage_mean_time(rt_stage(2))
                             << std::endl;
                renderer.log << "Up to copy_framebuffer:\t" << debug_stats.get_stage_mean_time(rt_stage(3))
                             << std::endl;
                renderer.log << "Up to conclude_frame:\t" << debug_stats.get_stage_mean_time(rt_stage(4))
                             << std::endl;
                renderer.log << "Up to glEnd:\t" << debug_stats.get_stage_mean_time(rt_stage(5))
                             << std::endl;
                ////////////////


                renderer.log.enabled = false;
        }

        if (!(frame % 100) && frame < 500 && false){
                std::cout << "Switching scene!" << std::endl;
                if (scene.destroy()) {
                        std::cout << "Error destroying scene!!\n";
                        exit(-1);
                }
                scene.initialize();

                if (frame == 100)
                        buddha_set_scene(scene);
                if (frame == 200)
                        buddha_set_scene(scene);
                if (frame == 300)
                        buddha_set_scene(scene);
                if (frame == 400)
                        buddha_set_scene(scene);

                if (scene.create_aggregate_mesh() || scene.create_aggregate_bvh()) { 
                        std::cerr << "Failed to create aggregate mesh" << std::endl;
                        pause_and_exit(1);
                } else {
                        std::cout << "Created aggregate mesh succesfully" << std::endl;
                }
                if (scene.transfer_aggregate_mesh_to_device() ||
                    scene.transfer_aggregate_bvh_to_device()) {
                        std::cerr << "Failed to transfer aggregate mesh to device memory"
                                << std::endl;
                        pause_and_exit(1);
                } else {
                        std::cout << "Transfered aggregate mesh to device succesfully"
                                << std::endl;
                }
        }

        renderer.log << "Time elapsed: " 
                  << stats.get_frame_time() << " milliseconds " 
                  << "\t" 
                  << (1000.f / stats.get_frame_time())
                  << " FPS"
                  << "\t"
                  << stats.get_ray_count()
                  << " rays casted "
                  << "\t(" << stats.get_ray_count() - stats.get_secondary_ray_count() << " primary, "
                  << stats.get_secondary_ray_count() << " secondary)"
                  << std::endl;
        if (renderer.log.silent)
                renderer.log<< "Time elapsed: "
                << stats.get_frame_time() << " milliseconds "
                << "\t"
                << (1000.f / stats.get_frame_time())
                << " FPS"
                << "                \r";
        std::flush(renderer.log.o());
        std::cout << std::flush;
        renderer.log << "\nBVH Build time:\t" << stats.get_stage_time(BVH_BUILD) << std::endl;
        renderer.log << "Prim Gen time: \t" << stats.get_stage_time(PRIM_RAY_GEN)<< std::endl;
        renderer.log << "Sec Gen time: \t" << stats.get_stage_time(SEC_RAY_GEN)  << std::endl;
        renderer.log << "Tracer time: \t" << stats.get_stage_time(PRIM_TRACE) + stats.get_stage_time(SEC_TRACE)
                  << " (" <<  stats.get_stage_time(PRIM_TRACE) << " - " << stats.get_stage_time(SEC_TRACE)
                  << ")" << std::endl;
        renderer.log << "Shadow time: \t" << stats.get_stage_time(PRIM_SHADOW_TRACE) + stats.get_stage_time(SEC_SHADOW_TRACE)
                  << " (" <<  stats.get_stage_time(PRIM_SHADOW_TRACE)
                  << " - " << stats.get_stage_time(SEC_SHADOW_TRACE) << ")" << std::endl;
        renderer.log << "Shader time: \t" << stats.get_stage_time(SHADE) << std::endl;
        renderer.log << "Fb clear time: \t" << stats.get_stage_time(FB_CLEAR) << std::endl;
        renderer.log << "Fb copy time: \t" << stats.get_stage_time(FB_COPY) << std::endl;
        renderer.log << std::endl;


}


int main (int argc, char** argv) 
{
        // Initialize renderer
        renderer.initialize_from_ini_file("rt.ini");
        int32_t ini_err;
        INIReader ini;
        ini_err = ini.load_file("rt.ini");
        if (ini_err < 0)
                std::cout << "Error at ini file line: " << -ini_err << std::endl;
        else if (ini_err > 0)
                std::cout << "Unable to open ini file" << std::endl;

        // Initialize OpenGL and OpenCL
        size_t window_size[] = {renderer.get_framebuffer_w(), renderer.get_framebuffer_h()};
        GLInfo* glinfo = GLInfo::instance();

        if (glinfo->initialize(argc,argv, window_size, "RT") != 0){
                std::cerr << "Failed to initialize GL" << std::endl;
                pause_and_exit(1);
        } else { 
                std::cout << "Initialized GL succesfully" << std::endl;
        }

        CLInfo* clinfo = CLInfo::instance();

        if (clinfo->initialize(2) != CL_SUCCESS){
                std::cerr << "Failed to initialize CL" << std::endl;
                pause_and_exit(1);
        } else { 
                std::cout << "Initialized CL succesfully" << std::endl;
        }
        clinfo->print_info();

        // Initialize device interface and generic gpu library
        DeviceInterface* device = DeviceInterface::instance();
        if (device->initialize()) {
                std::cerr << "Failed to initialize device interface" << std::endl;
                pause_and_exit(1);
        }

        if (DeviceFunctionLibrary::instance()->initialize()) {
                std::cerr << "Failed to initialize function library" << std::endl;
                pause_and_exit(1);
        }

        gl_tex = create_tex_gl(window_size[0],window_size[1]);
        tex_id = device->new_memory();
        DeviceMemory& tex_mem = device->memory(tex_id);
        if (tex_mem.initialize_from_gl_texture(gl_tex)) {
                std::cerr << "Failed to create memory object from gl texture" << std::endl;
                pause_and_exit(1);
        }

        /*---------------------- Set up scene ---------------------------*/
        if (scene.initialize()) {
                std::cerr << "Failed to initialize scene" << std::endl;
                pause_and_exit(1);
        } else {
                std::cout << "Initialized scene succesfully" << std::endl;
        }
        
        /*---------------------- Scene definition -----------------------*/
        buddha_set_cam_traj();
        dragon_set_scene(scene);
        // buddha_set_scene(scene);
        // hand_set_scene(scene);
        // fairy_set_scene(scene);

        /*---------------------- Move scene data to gpu -----------------------*/
         if (scene.create_aggregate_mesh()) { 
                std::cerr << "Failed to create aggregate mesh" << std::endl;
                pause_and_exit(1);
         } else {
                 std::cout << "Created aggregate mesh succesfully" << std::endl;
         }

         int32_t gpu_bvh = true;
         ini.get_int_value("RT", "gpu_bvh", gpu_bvh);

         if (!gpu_bvh) {
                 if (scene.create_aggregate_bvh()) { 
                         std::cerr << "Failed to create aggregate bvh" << std::endl;
                         pause_and_exit(1);
                 } else {
                         std::cout << "Created aggregate bvh succesfully" << std::endl;
                 }
                 if (scene.transfer_aggregate_bvh_to_device()) {
                         std::cerr << "Failed to transfer aggregate mesh and bvh "
                                   << "to device memory"
                                   << std::endl;
                         pause_and_exit(1);
                 } else {
                         std::cout << "Transfered aggregate mesh and bvh " 
                                   << "to device succesfully" << std::endl;
                 }
         }

         if (scene.transfer_aggregate_mesh_to_device()) {
                 std::cerr << "Failed to transfer aggregate mesh to device memory"
                           << std::endl;
                 pause_and_exit(1);
         } else {
                 std::cout << "Transfered aggregate mesh to device succesfully"
                           << std::endl;
         }



         /*---------------------- Set initial scene.camera paramaters -----------------------*/
        // scene.camera.set(makeVector(0,3,-30), makeVector(0,0,1), makeVector(0,1,0), M_PI/4.,
        //         window_size[0] / (float)window_size[1]);
        scene.camera.set(makeVector(1, -1, -50), 
                         makeVector(0,0,1), 
                         makeVector(0,1,0), M_PI/4.,
                window_size[0] / (float)window_size[1]);

        /*----------------------- Initialize cubemap ---------------------------*/
        
        std::string cubemap_path = "textures/cubemap/Path/";
        if (scene.cubemap.initialize(cubemap_path + "posx.jpg",
                cubemap_path + "negx.jpg",
                cubemap_path + "posy.jpg",
                cubemap_path + "negy.jpg",
                cubemap_path + "posz.jpg",
                cubemap_path + "negz.jpg")) {
                        std::cerr << "Failed to initialize cubemap." << std::endl;
                        pause_and_exit(1);
        }
        scene.cubemap.enabled = true;
        directional_light_cl dl;
        dl.set_dir(0,-0.8,-0.3);
        dl.set_color(0.8,0.8,0.8);
        scene.set_dir_light(dl);

        Mesh& agg = scene.get_aggregate_mesh();
        std::cout << "Triangles: " << agg.triangleCount() << std::endl;
        std::cout << "Vertices : " << agg.vertexCount() << std::endl;

        /* Initialize renderer */
        log_filename = "rt-buddha-mod-log"; 
        renderer.initialize(log_filename);
        renderer.set_max_bounces(9);
        renderer.log.silent = false;

        /* Set callbacks */
        glutKeyboardFunc(gl_key);
        glutMotionFunc(gl_mouse);
        glutDisplayFunc(gl_loop);
        glutIdleFunc(gl_loop);
        glutMainLoop(); 

        CLInfo::instance()->set_sync(true);

        CLInfo::instance()->release_resources();

        return 0;
}


