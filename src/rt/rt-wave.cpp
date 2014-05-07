#include <algorithm>
#include <iostream>
#include <sstream>

#include <cl-gl/opengl-init.hpp>
#include <cl-gl/opencl-init.hpp>
#include <rt/rt.hpp>

#include <rt/test-params.hpp>

rt_time_t debug_timer;
FrameStats debug_stats;
function_id mangler_id;

/*
 *
 */

Renderer renderer;

memory_id tex_id;

int model_count = 44; 
int current_model = 0;
Scene scene;
std::vector<Scene> scenes;

GLuint gl_tex;

bool  print_fps = true;
bool logging = false;
bool logged = false;
bool custom_logging = false;
int  stats_logged = 0;

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
        (void)x;
        (void)y;

        float delta = 2.f;

        static float scale = 1.f;
        static float tilt = 0.f;

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
                std::cout << "\n";
                std::cout << "scene.camera Pos:\n" << scene.camera.pos << "\n";
                std::cout << "scene.camera Dir:\n" << scene.camera.dir << "\n";
                break;
        case 'b':
                renderer.log.silent = !renderer.log.silent;
                break;
        case 'f':
                print_fps = !print_fps;
                break;
        case 'e':
                CLInfo::instance()->set_sync(!CLInfo::instance()->sync());
                break;
        case 'c':
                scene.cubemap.enabled = !scene.cubemap.enabled;
                break;
        case '1': /* Set 1 sample per pixel */
                if (renderer.set_samples_per_pixel(1,samples1)){
                        std::cerr << "Error seting spp\n";
                        pause_and_exit(1);
                }
                break;
        case '4': /* Set 4 samples per pixel */
                if (renderer.set_samples_per_pixel(4,samples4)){
                        std::cerr << "Error seting spp\n";
                        pause_and_exit(1);
                }
                break;
        case 'q':
                std::cout << "\n" << "pause_and_exiting...\n";
                pause_and_exit(1);
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
	static cl_float mangler_arg = 0;
	static cl_float last_mangler_arg = -1000;

        debug_timer.snap_time();//!!

        glClear(GL_COLOR_BUFFER_BIT);

	/*-------------- Mangle verts in OpenCL---------------------*/

	mangler_arg += 1.f;
	rt_time_t mangle_timer;

        DeviceInterface* device = DeviceInterface::instance();
        DeviceFunction& mangler_function = device->function(mangler_id);

        std::cerr << "Using GPU...\n";
        mangle_timer.snap_time();
        mangler_function.set_arg(1,sizeof(cl_float),&mangler_arg);
        mangler_function.set_arg(2,sizeof(cl_float),&last_mangler_arg);

        if (mangler_function.execute()) {
                std::cerr << "Error executing mangler.\n";
                exit(1);
        }
	double mangle_time = mangle_timer.msec_since_snap();
	std::cerr << "Mangle time: "  << mangle_time << " msec.\n";
	last_mangler_arg = mangler_arg;
	/*--------------------------------------*/

        //Acquire graphics library resources, clear framebuffer, 
        // clear counters and create bvh if needed
        if (renderer.set_up_frame(tex_id ,scene)) {
                std::cerr << "Error in setting up frame\n";
                exit(-1);
        }

        //Set camera parameters (if needed)

        //Render image to framebuffer and copy to set up texture
        //renderer.render_to_texture(scene);

        renderer.render_to_framebuffer(scene);//!!
        
        debug_stats.stage_acc_times[2] += debug_timer.msec_since_snap();//!!

        renderer.copy_framebuffer();//!!

        debug_stats.stage_acc_times[3] += debug_timer.msec_since_snap();//!!

        //Do cleanup after rendering frame
        if (renderer.conclude_frame(scene)) {
                std::cerr << "Error concluding frame\n";
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

        //std::cout << "Camera Pos: " << scene.camera.pos << "\n";
        const FrameStats& stats = renderer.get_frame_stats();

        if (print_fps) {
                glRasterPos2f(-0.95f,0.9f);
                std::stringstream ss;
                ss << "FPS: " << (1000.f / stats.get_frame_time());
                std::string fps_s = ss.str();
                for (uint32_t i = 0; i < fps_s.size(); ++i)
                        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *(fps_s.c_str()+i));
        }


        glutSwapBuffers();


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
                  << "\n";
        if (renderer.log.silent)
                renderer.log<< "Time elapsed: "
                << stats.get_frame_time() << " milliseconds "
                << "\t"
                << (1000.f / stats.get_frame_time())
                << " FPS"
                << "                \r";
        //std::flush(renderer.log.o());
        renderer.log << "\nBVH Build time:\t" << stats.get_stage_time(BVH_BUILD) << "\n";
        renderer.log << "Prim Gen time: \t" << stats.get_stage_time(PRIM_RAY_GEN)<< "\n";
        renderer.log << "Sec Gen time: \t" << stats.get_stage_time(SEC_RAY_GEN)  << "\n";
        renderer.log << "Tracer time: \t" << stats.get_stage_time(PRIM_TRACE) + stats.get_stage_time(SEC_TRACE)
                  << " (" <<  stats.get_stage_time(PRIM_TRACE) << " - " << stats.get_stage_time(SEC_TRACE)
                  << ")\n";
        renderer.log << "Shadow time: \t" << stats.get_stage_time(PRIM_SHADOW_TRACE) + stats.get_stage_time(SEC_SHADOW_TRACE)
                  << " (" <<  stats.get_stage_time(PRIM_SHADOW_TRACE)
                  << " - " << stats.get_stage_time(SEC_SHADOW_TRACE) << ")\n";
        renderer.log << "Shader time: \t" << stats.get_stage_time(SHADE) << "\n";
        renderer.log << "Fb clear time: \t" << stats.get_stage_time(FB_CLEAR) << "\n";
        renderer.log << "Fb copy time: \t" << stats.get_stage_time(FB_COPY) << "\n";
        renderer.log << "\n";

}


int main (int argc, char** argv) 
{
        // Initialize renderer
        renderer.configure_from_ini_file("rt.ini");

        // Initialize OpenGL and OpenCL
        size_t window_size[] = {renderer.get_framebuffer_w(), renderer.get_framebuffer_h()};

        GLInfo* glinfo = GLInfo::instance();

        if (glinfo->initialize(argc,argv, window_size, "RT") != 0){
                std::cerr << "Failed to initialize GL\n";
                pause_and_exit(1);
        } else { 
                std::cout << "Initialized GL succesfully\n";
        }

        CLInfo* clinfo = CLInfo::instance();
        if (clinfo->initialize(2) != CL_SUCCESS){
                std::cerr << "Failed to initialize CL\n";
                pause_and_exit(1);
        } else { 
                std::cout << "Initialized CL succesfully\n";
        }
        clinfo->print_info();

        // Initialize device interface and generic gpu library
        DeviceInterface* device = DeviceInterface::instance();
        if (device->initialize()) {
                std::cerr << "Failed to initialize device interface\n";
                pause_and_exit(1);
        }

        if (DeviceFunctionLibrary::instance()->initialize()) {
                std::cerr << "Failed to initialize function library\n";
                pause_and_exit(1);
        }

        gl_tex = create_tex_gl(window_size[0],window_size[1]);
        tex_id = device->new_memory();
        DeviceMemory& tex_mem = device->memory(tex_id);
        if (tex_mem.initialize_from_gl_texture(gl_tex)) {
                std::cerr << "Failed to create memory object from gl texture\n";
                pause_and_exit(1);
        }

        /*---------------------- Set up scene ---------------------------*/
        if (scene.initialize()) {
                std::cerr << "Failed to initialize scene\n";
                pause_and_exit(1);
        } else {
                std::cout << "Initialized scene succesfully\n";
        }
        
        /*---------------------- Scene definition -----------------------*/

	mesh_id floor_mesh_id = scene.load_obj_file_as_aggregate("models/obj/grid100.obj");
	object_id floor_obj_id  = scene.add_object(floor_mesh_id);
	Object& floor_obj = scene.object(floor_obj_id);
 	floor_obj.geom.setScale(10.f);
	floor_obj.geom.setPos(makeVector(0.f,0.f,0.f));
	floor_obj.mat.diffuse = White;
	floor_obj.mat.reflectiveness = 0.95f;
	floor_obj.mat.refractive_index = 1.333f;

	directional_light_cl light;
	light.set_dir(0.05f, -1.f, -0.02f);
	light.set_color(0.7f,0.7f,0.7f);
	scene.set_dir_light(light);

	color_cl ambient;
	ambient[0] = ambient[1] = ambient[2] = 0.2f;
	scene.set_ambient_light(ambient);

        /*---------------------- Move scene data to gpu -----------------------*/
         if (scene.create_aggregate_mesh()) { 
                std::cerr << "Failed to create aggregate mesh\n";
                pause_and_exit(1);
         } else {
                 std::cout << "Created aggregate mesh succesfully\n";
         }
         if (scene.create_aggregate_bvh()) { 
                std::cerr << "Failed to create aggregate bvh\n";
                pause_and_exit(1);
         } else {
                 std::cout << "Created aggregate bvh succesfully\n";
         }
         if (scene.transfer_aggregate_mesh_to_device() ||
             scene.transfer_aggregate_bvh_to_device()) {
                     std::cerr << "Failed to transfer aggregate mesh and bvh to device memory"
                         << "\n";
                 pause_and_exit(1);
         } else {
                 std::cout << "Transfered aggregate mesh and bvh to device succesfully"
                         << "\n";
         }


         /*---------------------- Set initial scene.camera paramaters -----------------------*/
        scene.camera.set(makeVector(0,3,-30), makeVector(0,0,1), makeVector(0,1,0), M_PI/4.,
                window_size[0] / (float)window_size[1]);

        /*----------------------- Initialize cubemap ---------------------------*/
        
        std::string cubemap_path = "textures/cubemap/Path/";
        if (scene.cubemap.initialize(cubemap_path + "posx.jpg",
                cubemap_path + "negx.jpg",
                cubemap_path + "posy.jpg",
                cubemap_path + "negy.jpg",
                cubemap_path + "posz.jpg",
                cubemap_path + "negz.jpg")) {
                        std::cerr << "Failed to initialize cubemap.\n";
                        pause_and_exit(1);
        }

        Mesh& agg = scene.get_aggregate_mesh();
        std::cout << "Triangles: " << agg.triangleCount() << "\n";
        std::cout << "Vertices : " << agg.vertexCount() << "\n";

        /* ----------------------- Initialize renderer --------------------------- */
        std::string log_filename = "rt-wave-log"; 
        renderer.initialize(log_filename);
        renderer.set_max_bounces(5);
        renderer.log.silent = true;

	/* ------------------------- Create vertex mangler -----------------------*/
        mangler_id = device->new_function();
        DeviceFunction& mangler_function = device->function(mangler_id);
        if (mangler_function.initialize("src/kernel/mangler.cl", "mangle")) {
		std::cerr << "Error initializing mangler kernel.\n";
		exit(1);
	}

        mangler_function.set_dims(1);
	Mesh& scene_mesh = scene.get_aggregate_mesh();
        size_t mangler_global_size[] = {scene_mesh.vertexCount(), 0 , 0};
	cl_float h = 0.2; // WAVE_HEIGHT;
        mangler_function.set_global_size(mangler_global_size);
        if (mangler_function.set_arg(0, scene.vertex_mem()))
                std::cerr << "Error setting mangler argument 0\n";
        if (mangler_function.set_arg(3,sizeof(cl_float), &h))
                std::cerr << "Error setting mangler argument 3\n";


        /* ------------------------ Set callbacks ----------------------------- */
        glutKeyboardFunc(gl_key);
        glutMotionFunc(gl_mouse);
        glutDisplayFunc(gl_loop);
        glutIdleFunc(gl_loop);
        glutMainLoop(); 

        clinfo->release_resources();

        return 0;
}


