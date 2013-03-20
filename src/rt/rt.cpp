#include <algorithm>
#include <iostream>
#include <sstream>

#include <cl-gl/opengl-init.hpp>
#include <cl-gl/opencl-init.hpp>
#include <rt/rt.hpp>

#define TOTAL_STATS_TO_LOG 12
#define CONF_STATS_TO_LOG  4

LinearCameraTrajectory cam_traj;
Renderer renderer;

CLInfo clinfo;
GLInfo glinfo;

DeviceInterface& device = *DeviceInterface::instance();
memory_id tex_id;

int model_count = 44; 
int current_model = 0;
Scene scene;
std::vector<Scene> scenes;

Log rt_log;
GLuint gl_tex;

bool  print_fps = true;
bool logging = false;
bool logged = false;
bool custom_logging = false;
int  stats_logged = 0;

//hand
 //vec3 stats_camera_pos[] = {makeVector(-0.542773, 0.851275, 0.193901) ,
 //                           makeVector(0.496716, 0.56714, 0.721966) ,
 //                           makeVector(-0.563173, 1.41939, -0.134797) };
 //vec3 stats_camera_dir[] = {makeVector(0.658816, -0.55455, -0.508365),
 //                           makeVector(-0.472905, -0.0779194, -0.877661) ,
 //                           makeVector(0.502332, -0.860415, -0.0857217) };

//ben
 //vec3 stats_camera_pos[] = {makeVector(-0.126448, 0.546994, 0.818811) ,
 //                           makeVector(0.231515, 0.555109, 0.274977) ,
 //                           makeVector(0.198037, 0.799047, 0.392632) };
 //vec3 stats_camera_dir[] = {makeVector(0.157556,-0.121695, -0.979983),
 //                           makeVector(-0.745308, 0.139962, -0.651864) ,
 //                           makeVector(-0.329182, -0.636949, -0.697091) };

//fairy
// vec3 stats_camera_pos[] = {makeVector(-0.118154, 0.417017, 1.14433) ,
//                           makeVector(-0.464505, 0.717691, 0.279553) ,
//                           makeVector(0.560874, 3.02102, 2.22842) };
// vec3 stats_camera_dir[] = {makeVector(0.0700268, -0.095778, -0.992936),
//                           makeVector(0.666486, -0.576098, -0.473188) ,
//                           makeVector(-0.149838, -0.788657, -0.596295) };

//Boat
vec3 stats_camera_pos[] = {makeVector(20.0186, -5.49632, 71.8718) ,
                           makeVector(-56.9387, -2.41959, 29.574) ,
                           makeVector(68.6859, 5.18034, 13.6691) };
vec3 stats_camera_dir[] = {makeVector(0.131732, 0.0845093, -0.987677),
                           makeVector(0.927574, -0.22893, -0.295292) ,
                           makeVector(-0.820819, -0.478219, -0.31235) };

//dragon
  //vec3 stats_camera_pos[] = {makeVector(-39.9102, 9.42978, 55.1501) ,
  //                          makeVector(-1.1457, -0.774464, 49.4609) ,
  //                          makeVector(16.6885, 22.435, 25.0718) };




  //vec3 stats_camera_dir[] = {makeVector(0.558171, -0.165305, -0.813093),
  //                          makeVector(-0.00931118, -0.14159, -0.989882) ,
  //                          makeVector(-0.402962, -0.631356, -0.66258) };

///Buddha
 //vec3 stats_camera_pos[] = {makeVector(0.741773, -1.754, 4.95699) ,
 //                           makeVector(-56.9387, -2.41959, 29.574) ,
 //                           makeVector(68.6859, 5.18034, 13.6691) };
 //vec3 stats_camera_dir[] = {makeVector(-0.179715, -0.0878783, -0.979786),
 //                           makeVector(0.927574, -0.22893, -0.295292) ,
 //                           makeVector(-0.820819, -0.478219, -0.31235) };


#define STEPS 16

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
                //MAX_BOUNCE = std::min(MAX_BOUNCE+1, 10u);!!
                break;
        case '-':
                //MAX_BOUNCE = std::max(MAX_BOUNCE-1, 0u);!!
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
        case 'k':
                rt_log.silent = true;
                rt_log.enabled = true;
                rt_log << "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- " 
                       << "CUSTOM LOG: " 
                       << "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- " 
                       << std::endl;
                rt_log << "scene.camera Pos: " << scene.camera.pos << std::endl;
                rt_log << "scene.camera Dir: " << scene.camera.dir << std::endl;
                //rt_log << "MAX_BOUNCE: "<< MAX_BOUNCE << std::endl;!!
                rt_log << std::endl;
                logging = true;
                custom_logging = true;
                stats_logged = 0;
                break;
        case 'l':
                if (logged)
                        break;
                rt_log.silent = true;
                rt_log.enabled = true;
                logging = true;
                logged = true;
                stats_logged = 0;
                //MAX_BOUNCE = 9;
                break;
        case 'b':
                rt_log.silent = !rt_log.silent;
                break;
        case 'f':
                print_fps = !print_fps;
                break;
        case 'e':
                clinfo.set_sync(!clinfo.sync());
                break;
        case 'c':
                scene.cubemap.enabled = !scene.cubemap.enabled;
                break;
        case '1': /* Set 1 sample per pixel */
                //if (prim_ray_gen.set_spp(1,samples1)){
                //        std::cerr << "Error seting spp" << std::endl;
                //        pause_and_exit(1);
                //}
                break;
        case '4': /* Set 4 samples per pixel */
                //if (prim_ray_gen.set_spp(4,samples4)){
                //        std::cerr << "Error seting spp" << std::endl;
                //        pause_and_exit(1);
                //}
                break;
        case 'q':
                std::cout << std::endl << "pause_and_exiting..." << std::endl;
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
        glClear(GL_COLOR_BUFFER_BIT);

        //Acquire graphics library resources, clear framebuffer, 
        // clear counters and create bvh if needed
        renderer.set_up_frame(tex_id ,scene);

        //Set camera parameters (if needed)
        vec3 cam_pos,cam_up,cam_dir;
        cam_traj.get_next_camera_params(&cam_pos, &cam_dir, &cam_up);
        cam_pos = makeVector(0.f,0.f,0.f);
        cam_dir = makeVector(0.f,0.f,1.f);
        cam_up = makeVector(0.f,1.f,0.f);
        float FOV = M_PI/4.f;
        float aspect = 1.f; //!!window_size[0] / (float)window_size[1];
        //scene.camera.set(cam_pos,cam_dir,cam_up, FOV, aspect);
        
        //Render image to framebuffer and copy to set up texture
        renderer.render_to_texture(scene);
        
        //Do cleanup after rendering frame
        renderer.conclude_frame(scene);

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
        
        glutSwapBuffers();
}

        //rt_log<< "Time elapsed: " 
        //          << total_msec << " milliseconds " 
        //          << "\t" 
        //          << (1000.f / total_msec)
        //          << " FPS"
        //          << "\t"
        //          << total_ray_count
        //          << " rays casted "
        //          << "\t(" << total_ray_count - total_sec_ray_count << " primary, " 
        //          << total_sec_ray_count << " secondary)"
        //          << std::endl;
        //if (rt_log.silent)
        //        std::cout<< "Time elapsed: "
        //        << total_msec << " milliseconds "
        //        << "\t"
        //        << (1000.f / total_msec)
        //        << " FPS"
        //        << "                \r";
        //std::flush(std::cout);
        //rt_log << "\nBVH Build time:\t" << bvh_build_time << std::endl;
        //rt_log << "Prim Gen time: \t" << prim_gen_time << std::endl;
        //rt_log << "Sec Gen time: \t" << sec_gen_time << std::endl;
        //rt_log << "Tracer time: \t" << prim_trace_time + sec_trace_time  
        //          << " (" <<  prim_trace_time << " - " << sec_trace_time 
        //          << ")" << std::endl;
        //rt_log << "Shadow time: \t" << prim_shadow_trace_time + sec_shadow_trace_time 
        //          << " (" <<  prim_shadow_trace_time 
        //          << " - " << sec_shadow_trace_time << ")" << std::endl;
        //rt_log << "Shader time: \t" << shader_time << std::endl;
        //rt_log << "Fb clear time: \t" << fb_clear_time << std::endl;
        //rt_log << "Fb copy time: \t" << fb_copy_time << std::endl;
        //rt_log << std::endl;

        //if (print_fps) {
        //        glRasterPos2f(-0.95f,0.9f);
        //        std::stringstream ss;
        //        ss << "FPS: " << (1000.f / total_msec);
        //        if (logging) {
        //                ss << "   LOGGING";
        //        }
        //        std::string fps_s = ss.str();
        //        for (uint32_t i = 0; i < fps_s.size(); ++i)
        //                glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *(fps_s.c_str()+i));
        //}

int main (int argc, char** argv) 
{
        // Initialize renderer
        renderer.initialize_from_ini_file("rt.ini");

        // Initialize OpenGL and OpenCL
        size_t window_size[] = {renderer.get_framebuffer_w(), renderer.get_framebuffer_h()};
        if (init_gl(argc,argv,&glinfo, window_size, "RT") != 0){
                std::cerr << "Failed to initialize GL" << std::endl;
                pause_and_exit(1);
        } else { 
                std::cout << "Initialized GL succesfully" << std::endl;
        }

        if (init_cl(glinfo,&clinfo) != CL_SUCCESS){
                std::cerr << "Failed to initialize CL" << std::endl;
                pause_and_exit(1);
        } else { 
                std::cout << "Initialized CL succesfully" << std::endl;
        }
        print_cl_info(clinfo);

        // Initialize device interface and generic gpu library
        if (device.initialize(clinfo)) {
                std::cerr << "Failed to initialize device interface" << std::endl;
                pause_and_exit(1);
        }
        DeviceFunctionLibrary::initialize(clinfo);
        gl_tex = create_tex_gl(window_size[0],window_size[1]);
        tex_id = device.new_memory();
        DeviceMemory& tex_mem = device.memory(tex_id);
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
        
        /* Scene definition */
        //// Boat
         mesh_id floor_mesh_id = 
                scene.load_obj_file_as_aggregate("models/obj/frame_water1.obj");
                //scene.load_obj_file_as_aggregate("models/obj/grid200.obj");
         object_id floor_obj_id  = scene.add_object(floor_mesh_id);
         Object& floor_obj = scene.object(floor_obj_id);
         floor_obj.geom.setScale(2.f);
         floor_obj.geom.setPos(makeVector(0.f,-8.f,0.f));
         floor_obj.mat.diffuse = White;
         floor_obj.mat.diffuse = Blue;
         floor_obj.mat.reflectiveness = 0.98f;
         floor_obj.mat.refractive_index = 1.333f;

         mesh_id boat_mesh_id = 
                 scene.load_obj_file_as_aggregate("models/obj/frame_boat1.obj");
         object_id boat_obj_id = scene.add_object(boat_mesh_id);
         Object& boat_obj = scene.object(boat_obj_id);
         boat_obj.geom.setPos(makeVector(0.f,-8.f,0.f));
         boat_obj.geom.setRpy(makeVector(0.f,0.f,0.f));
          boat_obj.geom.setScale(2.f);
         //boat_obj.geom.setScale(0.005f);
         boat_obj.mat.diffuse = Red;
         boat_obj.mat.shininess = 1.f;
         boat_obj.mat.reflectiveness = 0.0f;

         directional_light_cl light;
         light.set_dir(0.05f, -1.f, -0.02f);
         light.set_color(0.7f,0.7f,0.7f);
         scene.set_dir_light(light);

         color_cl ambient;
         ambient[0] = ambient[1] = ambient[2] = 0.2f;
         scene.set_ambient_light(ambient);

        /*---------------------- Move scene data to gpu -----------------------*/
         if (scene.create_aggregate_mesh()) { 
                std::cerr << "Failed to create aggregate mesh" << std::endl;
                pause_and_exit(1);
         } else {
                 std::cout << "Created aggregate mesh succesfully" << std::endl;
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
                        std::cerr << "Failed to initialize cubemap." << std::endl;
                        pause_and_exit(1);
        }

        /* Initialize renderer */
        renderer.initialize(clinfo);
        renderer.set_max_bounces(5);
        
        /* Set callbacks */
        glutKeyboardFunc(gl_key);
        glutMotionFunc(gl_mouse);
        glutDisplayFunc(gl_loop);
        glutIdleFunc(gl_loop);
        glutMainLoop(); 

        clinfo.release_resources();

        return 0;
}


