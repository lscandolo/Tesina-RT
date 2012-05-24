#include <algorithm>
#include <iostream>
#include <sstream>

#include <cl-gl/opengl-init.hpp>
#include <cl-gl/opencl-init.hpp>
#include <rt/rt.hpp>

int MAX_BOUNCE = 0;

CLInfo clinfo;
GLInfo glinfo;

DeviceInterface device;
memory_id tex_id;

// object_id item_id;
// object_id boat_id;
// object_id floor_id;

RayBundle             ray_bundle_1,ray_bundle_2;
HitBundle             hit_bundle;
PrimaryRayGenerator   prim_ray_gen;
SecondaryRayGenerator sec_ray_gen;
RayShader             ray_shader;
Scene                 scene;
Cubemap               cubemap;
FrameBuffer           framebuffer;
Tracer                tracer;
Camera                camera;

GLuint gl_tex;
rt_time_t rt_time;
size_t window_size[] = {512, 512};

size_t pixel_count = window_size[0] * window_size[1];
size_t best_tile_size;
Log rt_log;
bool  print_fps = true;

#define STEPS 16

void gl_mouse(int x, int y)
{
        float delta = 0.001f;
        float d_inc = delta * (window_size[1]*0.5f - y);/* y axis points downwards */ 
        float d_yaw = delta * (x - window_size[0]*0.5f);

        d_inc = std::min(std::max(d_inc, -0.1f), 0.1f);
        d_yaw = std::min(std::max(d_yaw, -0.1f), 0.1f);

        if (d_inc == 0.f && d_yaw == 0.f)
                return;

        camera.modifyAbsYaw(d_yaw);
        camera.modifyPitch(d_inc);
        glutWarpPointer(window_size[0] * 0.5f, window_size[1] * 0.5f);
}

void gl_key(unsigned char key, int x, int y)
{
        float delta = 2.f;

        static float scale = 1.f;
        static float tilt = 0.f;
        // Object& boat_obj = scene.object(boat_id);
        // Object& floor_obj = scene.object(floor_id);
        // Object& item_obj = scene.object(item_id);

        const sample_cl samples1[] = {{ 0.f , 0.f, 1.f}};
        const sample_cl samples4[] = {{ 0.25f , 0.25f, 0.25f},
                                      { 0.25f ,-0.25f, 0.25f},
                                      {-0.25f , 0.25f, 0.25f},
                                      {-0.25f ,-0.25f, 0.25f}};

        switch (key){
        case '+':
                MAX_BOUNCE = std::min(MAX_BOUNCE+1, 10);
                break;
        case '-':
                MAX_BOUNCE = std::max(MAX_BOUNCE-1, 0);
                break;
        case 'p':
                std::cout << std::endl;
                std::cout << "Camera Pos:\n" << camera.pos << std::endl;
                std::cout << "Camera Dir:\n" << camera.dir << std::endl;
                break;
        case 'a':
                camera.panRight(-delta*scale);
                break;
        case 's':
                camera.panForward(-delta*scale);
                break;
        case 'w':
                camera.panForward(delta*scale);
                break;
        case 'd':
                camera.panRight(delta*scale);
                break;
        case 'l':
                rt_log.silent = !rt_log.silent;
                break;
        case 'f':
                print_fps = !print_fps;
                break;
        case 'c':
                cubemap.enabled = !cubemap.enabled;
                break;
        case '1': /* Set 1 sample per pixel */
                if (prim_ray_gen.set_spp(1,samples1)){
                        std::cerr << "Error seting spp" << std::endl;
                        pause_and_exit(1);
                }
                break;
        case '4': /* Set 4 samples per pixel */
                if (prim_ray_gen.set_spp(4,samples4)){
                        std::cerr << "Error seting spp" << std::endl;
                        pause_and_exit(1);
                }
                break;
        case 'q':
                std::cout << std::endl << "pause_and_exiting..." << std::endl;
                pause_and_exit(1);
                break;
        case 't':
                tilt -= 0.01f;
                scale *= 1.5f;
                // item_obj.geom.setScale(scale);
                // boat_obj.geom.setRpy(makeVector(tilt,0.f,0.f));
                // floor_obj.geom.setRpy(makeVector(0.f,0.f,tilt));
                scene.update_bvh_roots();
                break;
        case 'y':
                tilt += 0.01f;
                scale /= 1.5f;
                // item_obj.geom.setScale(scale);
                // boat_obj.geom.setRpy(makeVector(tilt,0.f,0.f));
                // floor_obj.geom.setRpy(makeVector(0.f,0.f,tilt));
                scene.update_bvh_roots();
                break;
        }
}


void gl_loop()
{
        static int i = 0;
        static int dir = 1;
        static int total_ray_count = 0;

        double prim_gen_time = 0;
        double sec_gen_time = 0;
        double prim_trace_time = 0;
        double sec_trace_time = 0;
        double prim_shadow_trace_time = 0;
        double sec_shadow_trace_time = 0;
        double shader_time = 0;
        double fb_clear_time = 0;
        double fb_copy_time = 0;

        glClear(GL_COLOR_BUFFER_BIT);

        if (device.acquire_graphic_resource(tex_id) ||
            cubemap.acquire_graphic_resources() || 
            scene.acquire_graphic_resources()) {
                std::cerr << "Error acquiring texture resource." << std::endl;
                pause_and_exit(1);
        }
        
        if (framebuffer.clear()) {
                std::cerr << "Failed to clear framebuffer." << std::endl;
                pause_and_exit(1);
        }
        fb_clear_time = framebuffer.get_clear_exec_time();

        int32_t tile_size = best_tile_size;

        directional_light_cl light;
        // cl_int arg = i%STEPS;
        // light.set_dir(1.f,-1.f,0.f);
        // light.set_dir(0.05f * (arg - 8.f) , -0.6f, 0.2f);
        // light.set_color(0.05f * (fabsf(arg)) + 0.1f, 0.2f, 0.05f * fabsf(arg+4.f));
        light.set_dir(0.05f,-1.f,-1.9f);
        light.set_color(1.f,1.f,1.f);
        scene.set_dir_light(light);
        color_cl ambient;
        ambient[0] = ambient[1] = ambient[2] = 0.1f;
        scene.set_ambient_light(ambient);

        int32_t sample_count = pixel_count * prim_ray_gen.get_spp();
        for (int32_t offset = 0; offset < sample_count; offset+= tile_size) {

                
                RayBundle* ray_in =  &ray_bundle_1;
                RayBundle* ray_out = &ray_bundle_2;

                if (sample_count - offset < tile_size)
                        tile_size = sample_count - offset;

                if (prim_ray_gen.set_rays(camera, ray_bundle_1, window_size,
                                           tile_size, offset)) {
                        std::cerr << "Error seting primary ray bundle" << std::endl;
                        pause_and_exit(1);
                }
                prim_gen_time += prim_ray_gen.get_exec_time();

                total_ray_count += tile_size;

                if (tracer.trace(scene, tile_size, *ray_in, hit_bundle)) {
                        std::cerr << "Error tracing primary rays" << std::endl;
                        pause_and_exit(1);
                }
                prim_trace_time += tracer.get_trace_exec_time();

                if (tracer.shadow_trace(scene, tile_size, *ray_in, hit_bundle)) {
                        std::cerr << "Error shadow tracing primary rays" << std::endl;
                        pause_and_exit(1);
                }
                prim_shadow_trace_time += tracer.get_shadow_exec_time();

                if (ray_shader.shade(*ray_in, hit_bundle, scene,
                                     cubemap, framebuffer, tile_size,true)){
                        std::cerr << "Failed to update framebuffer." << std::endl;
                        pause_and_exit(1);
                }
                shader_time += ray_shader.get_exec_time();

                size_t sec_ray_count = tile_size;
                for (uint32_t i = 0; i < MAX_BOUNCE; ++i) {

                        if (sec_ray_gen.generate(scene, *ray_in, sec_ray_count, 
                                                 hit_bundle, *ray_out, &sec_ray_count)) {
                                std::cerr << "Failed to create secondary rays." 
                                          << std::endl;
                                pause_and_exit(1);
                        }
                        sec_gen_time += sec_ray_gen.get_exec_time();

                        std::swap(ray_in,ray_out);
 
                        if (!sec_ray_count)
                                break;
                        if (sec_ray_count == ray_bundle_1.count())
                                std::cerr << "Max sec rays reached!\n";

                        total_ray_count += sec_ray_count;

                        if (tracer.trace(scene, sec_ray_count, 
                                         *ray_in, hit_bundle, true)) {
                                std::cerr << "Error tracing secondary rays" << std::endl;
                                pause_and_exit(1);
                        }

                        sec_trace_time += tracer.get_trace_exec_time();

                        if (tracer.shadow_trace(scene, sec_ray_count, 
                                                *ray_in, hit_bundle, true)) {
                                std::cerr << "Error shadow tracing primary rays" 
                                          << std::endl;
                                pause_and_exit(1);
                        }
                        sec_shadow_trace_time += tracer.get_shadow_exec_time();

                        if (ray_shader.shade(*ray_in, hit_bundle, scene,
                                             cubemap, framebuffer, sec_ray_count)){
                                std::cerr << "Ray shader failed execution." << std::endl;
                                pause_and_exit(1);
                        }
                        shader_time += ray_shader.get_exec_time();
                }

        }

        if (framebuffer.copy(device.memory(tex_id))){
                std::cerr << "Failed to copy framebuffer." << std::endl;
                pause_and_exit(1);
        }
        fb_copy_time = framebuffer.get_copy_exec_time();
        double total_msec = rt_time.msec_since_snap();

        if (device.release_graphic_resource(tex_id) ||
            cubemap.release_graphic_resources() || 
            scene.release_graphic_resources()) {
            std::cerr << "Error releasing texture resource." << std::endl;
            pause_and_exit(1);
    }
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

        i += dir;
        if (!(i % (STEPS-1))){
                dir *= -1;
        }               
        rt_log<< "Time elapsed: " 
                  << total_msec << " milliseconds " 
                  << "\t" 
                  << (1000.f / total_msec)
                  << " FPS"
                  << "\t"
                  << total_ray_count
                  << " rays casted "
                  << "\t(" << pixel_count << " primary, " 
                  << total_ray_count-pixel_count << " secondary)"
                  << std::endl;
        if (rt_log.silent)
                std::cout<< "Time elapsed: "
                << total_msec << " milliseconds "
                << "\t"
                << (1000.f / total_msec)
                << " FPS"
                << "                \r";
        std::flush(std::cout);
        rt_time.snap_time();
        total_ray_count = 0;

        rt_log << "\nPrim Gen time: \t" << prim_gen_time << std::endl;
        rt_log << "Sec Gen time: \t" << sec_gen_time << std::endl;
        rt_log << "Tracer time: \t" << prim_trace_time + sec_trace_time  
                  << " (" <<  prim_trace_time << " - " << sec_trace_time 
                  << ")" << std::endl;
        rt_log << "Shadow time: \t" << prim_shadow_trace_time + sec_shadow_trace_time 
                  << " (" <<  prim_shadow_trace_time 
                  << " - " << sec_shadow_trace_time << ")" << std::endl;
        rt_log << "Shader time: \t " << shader_time << std::endl;
        rt_log << "Fb clear time: \t" << fb_clear_time << std::endl;
        rt_log << "Fb copy time: \t" << fb_copy_time << std::endl;
        rt_log << std::endl;

        // glRasterPos2f(-window_size[0]/2.f,window_size[1]/2.f - 30.f);

        if (print_fps) {
                glRasterPos2f(-0.95f,0.9f);
                std::stringstream ss;
                ss << "FPS: " << (1000.f / total_msec);
                std::string fps_s = ss.str();
                for (int i = 0; i < fps_s.size(); ++i)
                        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *(fps_s.c_str()+i));
        }

        glutSwapBuffers();
}

int main (int argc, char** argv)
{

        /*---------------------- Initialize OpenGL and OpenCL ----------------------*/

        if (rt_log.initialize("rt-log")){
                std::cerr << "Error initializing log!" << std::endl;
        }
                
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

        /* Initialize device interface */
        if (device.initialize(clinfo)) {
                std::cerr << "Failed to initialize device interface" << std::endl;
                pause_and_exit(1);
        }
        
        /*---------------------- Create shared GL-CL texture ----------------------*/
        gl_tex = create_tex_gl(window_size[0],window_size[1]);
        
        tex_id = device.new_memory();
        DeviceMemory& tex_mem = device.memory(tex_id);
        if (tex_mem.initialize_from_gl_texture(gl_tex)) {
                std::cerr << "Failed to create memory object from gl texture" << std::endl;
                pause_and_exit(1);
        }
        /*---------------------- Set up scene ---------------------------*/
        if (scene.initialize(clinfo)) {
                std::cerr << "Failed to initialize scene" << std::endl;
                pause_and_exit(1);
        } else {
                std::cout << "Initialized scene succesfully" << std::endl;
        }

        /* Other models 
        models/obj/floor.obj
        models/obj/teapot-low_res.obj
        models/obj/teapot.obj
        models/obj/teapot2.obj
        models/obj/frame_boat1.obj
        models/obj/frame_others1.obj
        models/obj/frame_water1.obj
        */


        //mesh_id floor_mesh_id = scene.load_obj_file_as_aggregate("models/obj/pack1OBJ/gridFluid1.obj");
        // mesh_id floor_mesh_id = scene.load_obj_file_as_aggregate("models/obj/floor.obj");

         mesh_id floor_mesh_id = scene.load_obj_file_as_aggregate("models/obj/frame_water1.obj");
        object_id floor_obj_id  = scene.add_object(floor_mesh_id);
        Object& floor_obj = scene.object(floor_obj_id);
        floor_obj.geom.setScale(2.f);
        floor_obj.geom.setPos(makeVector(0.f,-8.f,0.f));
        floor_obj.mat.diffuse = Blue;
        floor_obj.mat.reflectiveness = 0.9f;
        floor_obj.mat.refractive_index = 1.333f;
        // floor_id = floor_obj_id; //!!

         mesh_id boat_mesh_id = 
                 scene.load_obj_file_as_aggregate("models/obj/frame_boat1.obj");
         object_id boat_obj_id = scene.add_object(boat_mesh_id);
         Object& boat_obj = scene.object(boat_obj_id);
         // boat_id = boat_obj_id; //!!
         boat_obj.geom.setPos(makeVector(0.f,-8.f,0.f));
         boat_obj.geom.setRpy(makeVector(0.f,0.f,0.f));
         boat_obj.geom.setScale(2.f);
         boat_obj.mat.diffuse = Red;
         boat_obj.mat.shininess = 1.f;
         boat_obj.mat.reflectiveness = 0.0f;

        // std::vector<mesh_id> hand_meshes;
        // hand_meshes = scene.load_obj_file("models/obj/hand/hand_00.obj");
        // std::vector<object_id> hand_objects;
        // hand_objects = scene.add_objects(hand_meshes);

        // scene.load_obj_file_and_make_objs("models/obj/hand/hand_40.obj");
        // scene.load_obj_file_and_make_objs("models/obj/ben/ben_00.obj");
        // scene.load_obj_file_and_make_objs("models/obj/fairy_forest/f000.obj");
        // scene.load_obj_file_and_make_objs("models/obj/marbles/marbles000.obj");
        // scene.load_obj_file_and_make_objs("models/obj/marbles/marbles000.obj");

        // // std::string mesh_file("models/obj/teapot2.obj");
        // std::string mesh_file("models/obj/hand/hand_00.obj");
        // // std::string mesh_file("models/obj/ben/ben_00.obj");
        // // std::string mesh_file("models/obj/fairy_forest/f000.obj");
        // // std::string mesh_file("models/obj/marbles/marbles000.obj");
        // mesh_id item_mesh_id = scene.load_obj_file_as_aggregate(mesh_file);
        // object_id item_obj_id = scene.add_object(item_mesh_id);
        // Object& item_obj = scene.object(item_obj_id);
        // // item_obj.geom.setPos(makeVector(-8.f,-5.f,0.f));
        // item_obj.mat.diffuse[0] = 239/255.f;
        // item_obj.mat.diffuse[1] = 208/255.f;
        // item_obj.mat.diffuse[2] = 207/255.f;
        // // item_obj.geom.setScale(2.f);
        // item_obj.mat.shininess = 1.f;
        // item_obj.mat.reflectiveness = 0.f;
        // item_id = item_obj_id;

        // texture_id hand_tex_id = scene.texture_atlas.load_texture("textures/hand/hand.ppm");
        // item_obj.mat.texture = hand_tex_id;

        // object_id item_obj_id_2 = scene.add_object(item_mesh_id);
        // Object& item_obj_2 = scene.object(item_obj_id_2);
        // item_obj_2.mat.diffuse = Red;
        // item_obj_2.mat.shininess = 1.f;
        // item_obj_2.geom.setPos(makeVector(8.f,5.f,0.f));
        // item_obj_2.geom.setRpy(makeVector(0.2f,0.1f,0.3f));
        // item_obj_2.geom.setScale(0.3f);
        // item_obj_2.mat.reflectiveness = 0.3f;

#if 1
         /* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- Aggregate BVH -=-=-=-=-=-=-=-=-=-=-=-=-=- */
         if (scene.create_aggregate_mesh()) { 
                std::cerr << "Failed to create aggregate mesh" << std::endl;
                pause_and_exit(1);
         } else {
                 std::cout << "Created aggregate mesh succesfully" << std::endl;
         }

         if (scene.create_aggregate_bvh()) { 
                std::cerr << "Failed to create aggregate bvh" << std::endl;
                pause_and_exit(1);
         } else {
                 std::cout << "Created aggregate bvh succesfully" << std::endl;
         }

         if (scene.transfer_aggregate_mesh_to_device()) {
                std::cerr << "Failed to transfer aggregate meshe to device memory" 
                          << std::endl;
                pause_and_exit(1);
         } else {
                 std::cout << "Transfered aggregate meshe to device succesfully" 
                           << std::endl;
         }

         if (scene.transfer_aggregate_bvh_to_device()) {
                std::cerr << "Failed to transfer aggregate bvh to device memory" 
                          << std::endl;
                pause_and_exit(1);
         } else {
                 std::cout << "Transfered aggregate bvh to device succesfully" 
                           << std::endl;
         }

#else

         /* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- Multi BVH -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */
         if (scene.create_bvhs()) { 
                std::cerr << "Failed to create bvhs" << std::endl;
                pause_and_exit(1);
         } else {
                 std::cout << "Create bvhs succesfully" << std::endl;
         }

         if (scene.transfer_meshes_to_device()) {
                std::cerr << "Failed to transfer meshes to device memory" << std::endl;
                pause_and_exit(1);
         } else {
                 std::cout << "Transfered meshes to device succesfully" << std::endl;
         }

         if (scene.transfer_bvhs_to_device()) {
                std::cerr << "Failed to transfer bvhs to device memory" << std::endl;
                pause_and_exit(1);
         } else {
                 std::cout << "Transfered bvhs to device succesfully" << std::endl;
         }

#endif

        /*---------------------- Set initial Camera paramaters -----------------------*/
        camera.set(makeVector(0,3,-30), makeVector(0,0,1), makeVector(0,1,0), M_PI/4.,
                   window_size[0] / (float)window_size[1]);

        /*---------------------------- Set tile size ------------------------------*/
        best_tile_size = clinfo.max_compute_units * clinfo.max_work_item_sizes[0];
        best_tile_size *= 128;
        best_tile_size = std::min(pixel_count, best_tile_size);

        /*---------------------- Initialize ray bundles -----------------------------*/
        int32_t ray_bundle_size = best_tile_size * 3;

        if (ray_bundle_1.initialize(ray_bundle_size, clinfo)) {
                std::cerr << "Error initializing ray bundle 1" << std::endl;
                std::cerr.flush();
                pause_and_exit(1);
        }

        if (ray_bundle_2.initialize(ray_bundle_size, clinfo)) {
                std::cerr << "Error initializing ray bundle 2" << std::endl;
                std::cerr.flush();
                pause_and_exit(1);
        }
        std::cout << "Initialized ray bundles succesfully" << std::endl;

        /*---------------------- Initialize hit bundle -----------------------------*/
        int32_t hit_bundle_size = ray_bundle_size;

        if (hit_bundle.initialize(hit_bundle_size, clinfo)) {
                std::cerr << "Error initializing hit bundle" << std::endl;
                std::cerr.flush();
                pause_and_exit(1);
        }
        std::cout << "Initialized hit bundle succesfully" << std::endl;

        /*----------------------- Initialize cubemap ---------------------------*/
        
        if (cubemap.initialize("textures/cubemap/Path/posx.jpg",
                               "textures/cubemap/Path/negx.jpg",
                               "textures/cubemap/Path/posy.jpg",
                               "textures/cubemap/Path/negy.jpg",
                               "textures/cubemap/Path/posz.jpg",
                               "textures/cubemap/Path/negz.jpg",
                               clinfo)) {
                std::cerr << "Failed to initialize cubemap." << std::endl;
                pause_and_exit(1);
        }
        std::cerr << "Initialized cubemap succesfully." << std::endl;

        /*------------------------ Initialize FrameBuffer ---------------------------*/
        if (framebuffer.initialize(clinfo, window_size)) {
                std::cerr << "Error initializing framebuffer." << std::endl;
                pause_and_exit(1);
        }
        std::cout << "Initialized framebuffer succesfully." << std::endl;

        /* ------------------ Initialize ray tracer kernel ----------------------*/

        if (tracer.initialize(clinfo)){
                std::cerr << "Failed to initialize tracer." << std::endl;
                pause_and_exit(1);
        }
        std::cerr << "Initialized tracer succesfully." << std::endl;


        /* ------------------ Initialize Primary Ray Generator ----------------------*/

        if (prim_ray_gen.initialize(clinfo)) {
                std::cerr << "Error initializing primary ray generator." << std::endl;
                pause_and_exit(1);
        }
        std::cout << "Initialized primary ray generator succesfully." << std::endl;
                

        /* ------------------ Initialize Secondary Ray Generator ----------------------*/
        if (sec_ray_gen.initialize(clinfo)) {
                std::cerr << "Error initializing secondary ray generator." << std::endl;
                pause_and_exit(1);
        }
        sec_ray_gen.set_max_rays(ray_bundle_1.count());
        std::cout << "Initialized secondary ray generator succesfully." << std::endl;

        /*------------------------ Initialize RayShader ---------------------------*/
        if (ray_shader.initialize(clinfo)) {
                std::cerr << "Error initializing ray shader." << std::endl;
                pause_and_exit(1);
        }
        std::cout << "Initialized ray shader succesfully." << std::endl;

        /*----------------------- Enable timing in all clases -------------------*/
        framebuffer.timing(true);
        prim_ray_gen.timing(true);
        sec_ray_gen.timing(true);
        tracer.timing(true);
        ray_shader.timing(true);

        /*---------------------- Print scene data ----------------------*/
        std::cerr << "\nScene stats: " << std::endl;
        std::cerr << "\tTriangle count: " << scene.triangle_count() << std::endl;
        std::cerr << "\tVertex count: " << scene.vertex_count() << std::endl;

        /*------------------------- Count mem usage -----------------------------------*/
        int32_t total_cl_mem = 0;
        total_cl_mem += pixel_count * 4; /* 4bpp texture */
        total_cl_mem += ray_bundle_1.mem().size() + ray_bundle_2.mem().size();
        total_cl_mem += hit_bundle.mem().size();
        total_cl_mem += cubemap.positive_x_mem().size() * 6;
        total_cl_mem += framebuffer.image_mem().size();

        std::cout << "\nMemory stats: " << std::endl;
        std::cout << "\tTotal opencl mem usage: " 
                  << total_cl_mem/1e6 << " MB." << std::endl;
        std::cout << "\tFramebuffer+Tex mem usage: " 
                  << (framebuffer.image_mem().size() + pixel_count * 4)/1e6
                  << " MB."<< std::endl;
        std::cout << "\tCubemap mem usage: " 
                  << (cubemap.positive_x_mem().size()*6)/1e6 
                  << " MB."<< std::endl;
        std::cout << "\tRay mem usage: " 
                  << (ray_bundle_1.mem().size()*2)/1e6
                  << " MB."<< std::endl;
        std::cout << "\tRay hit info mem usage: " 
                  << hit_bundle.mem().size()/1e6
                  << " MB."<< std::endl;

        /* !! ---------------------- Test area ---------------- */
        std::cerr << std::endl;
        std::cerr << "Misc info: " << std::endl;

        std::cerr << "Tile size: " << best_tile_size << std::endl;
        std::cerr << "Tiles: " << pixel_count / (float)best_tile_size << std::endl;
        std::cerr << "color_cl size: "
                  << sizeof(color_cl)
                  << std::endl;
        std::cerr << "directional_light_cl size: "
                  << sizeof(directional_light_cl)
                  << std::endl;

        std::cerr << "ray_cl size: "
                  << sizeof(ray_cl)
                  << std::endl;
        std::cerr << "ray_plus_cl size: "
                  << sizeof(ray_plus_cl)
                  << std::endl;
        std::cerr << "ray_hit_info_cl size: "
                  << sizeof(ray_hit_info_cl)
                  << std::endl;

        std::cerr << "sizeof(bvh_root): " << sizeof(BVHRoot) << std::endl;

        std::cerr << "bvh_root_mem_size: " << scene.bvh_roots_mem().size() << std::endl
                  << "object_count * sizeof(bvhroot): " 
                  << scene.object_count()*sizeof(BVHRoot) 
                  << std::endl;

        // std::map<mesh_id, BVH>& bvhs = scene.bvhs;
        // std::vector<mesh_id>& bvh_order = scene.bvh_order;
        // int node_count = 0;
        // for (int i = 0; i < bvh_order.size(); ++i){
        //         BVH& bvh = bvhs[bvh_order[i]];
        //         std::cerr << "BVH " << bvh_order[i] << " size: " << bvh.nodeArraySize()
        //                   << std::endl;
        //         node_count += bvh.nodeArraySize();
        //         std::cerr << "First node: " << bvh.m_nodes[0].m_parent << std::endl;
        // }
        // std::cerr << "bvh nodes: " << node_count << std::endl;
        // std::cerr << "Size in bytes: " << sizeof(BVHNode) * node_count << std::endl;
        // std::cerr << "Device memory size: " << scene.bvh_nodes_mem().size() << std::endl;

        /*------------------------ Set GLUT and misc functions -----------------------*/
        rt_time.snap_time();
        rt_log.enabled = false;
        rt_log.silent = true;

        glutKeyboardFunc(gl_key);
        glutMotionFunc(gl_mouse);
        glutDisplayFunc(gl_loop);
        glutIdleFunc(gl_loop);
        glutMainLoop(); 

        clinfo.release_resources();

        return 0;
}
