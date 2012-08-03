#include <algorithm>
#include <iostream>
#include <sstream>

#include <cl-gl/opengl-init.hpp>
#include <cl-gl/opencl-init.hpp>
#include <rt/rt.hpp>

#include <gpu/scan.hpp> //!!

#define GPU_BVH_BUILD 1

#define TOTAL_STATS_TO_LOG 12
#define CONF_STATS_TO_LOG  4

int max_depth = 0;
void go_deeper(std::vector<KDTNode>& nodes, int node, int depth) {
        if (depth > max_depth)
                max_depth = depth;
        if (nodes[node].m_leaf)
                return;
        go_deeper(nodes, nodes[node].m_l_child,depth+1);
        go_deeper(nodes, nodes[node].m_r_child,depth+1);
}
int MAX_BOUNCE = 9;

CLInfo clinfo;
GLInfo glinfo;

int frame = 0;

Mesh orig_mesh;

DeviceInterface& device = *DeviceInterface::instance();
memory_id tex_id;

RayBundle             ray_bundle_1,ray_bundle_2;
HitBundle             hit_bundle;
PrimaryRayGenerator   prim_ray_gen;
SecondaryRayGenerator sec_ray_gen;
RayShader             ray_shader;
Scene                 scene;

int model_count = 44; 
int current_model = 0;
std::vector<Scene> scenes;

Cubemap               cubemap;
FrameBuffer           framebuffer;
Tracer                tracer;
BVHBuilder            bvh_builder;
Camera                camera;

GLuint gl_tex;
rt_time_t rt_time;
size_t window_size[] = {512, 512};

size_t pixel_count = window_size[0] * window_size[1];
size_t best_tile_size;
Log rt_log;
bool  print_fps = true;
bool logging = false;
bool logged = false;
bool custom_logging = false;
int  stats_logged = 0;

//hand
// vec3 stats_camera_pos[] = {makeVector(-0.542773, 0.851275, 0.193901) ,
//                            makeVector(0.496716, 0.56714, 0.721966) ,
//                            makeVector(-0.563173, 1.41939, -0.134797) };
// vec3 stats_camera_dir[] = {makeVector(0.658816, -0.55455, -0.508365),
//                            makeVector(-0.472905, -0.0779194, -0.877661) ,
//                            makeVector(0.502332, -0.860415, -0.0857217) };

//ben
// vec3 stats_camera_pos[] = {makeVector(-0.126448, 0.546994, 0.818811) ,
//                            makeVector(0.231515, 0.555109, 0.274977) ,
//                            makeVector(0.198037, 0.799047, 0.392632) };
// vec3 stats_camera_dir[] = {makeVector(0.157556,-0.121695, -0.979983),
//                            makeVector(-0.745308, 0.139962, -0.651864) ,
//                            makeVector(-0.329182, -0.636949, -0.697091) };

//fairy
// vec3 stats_camera_pos[] = {makeVector(-0.118154, 0.417017, 1.14433) ,
//                            makeVector(-0.464505, 0.717691, 0.279553) ,
//                            makeVector(0.560874, 3.02102, 2.22842) };
// vec3 stats_camera_dir[] = {makeVector(0.0700268, -0.095778, -0.992936),
//                            makeVector(0.666486, -0.576098, -0.473188) ,
//                            makeVector(-0.149838, -0.788657, -0.596295) };

///Boat
// vec3 stats_camera_pos[] = {makeVector(20.0186, -5.49632, 71.8718) ,
//                            makeVector(-56.9387, -2.41959, 29.574) ,
//                            makeVector(68.6859, 5.18034, 13.6691) };
// vec3 stats_camera_dir[] = {makeVector(0.131732, 0.0845093, -0.987677),
//                            makeVector(0.927574, -0.22893, -0.295292) ,
//                            makeVector(-0.820819, -0.478219, -0.31235) };

//dragon
// vec3 stats_camera_pos[] = {makeVector(-39.9102, 9.42978, 55.1501) ,
//                            makeVector(-1.1457, -0.774464, 49.4609) ,
//                            makeVector(16.6885, 22.435, 25.0718) };
// vec3 stats_camera_dir[] = {makeVector(0.558171, -0.165305, -0.813093),
//                            makeVector(-0.00931118, -0.14159, -0.989882) ,
//                            makeVector(-0.402962, -0.631356, -0.66258) };

///Buddha
vec3 stats_camera_pos[] = {makeVector(0.741773, -1.754, 4.95699) ,
                           makeVector(-56.9387, -2.41959, 29.574) ,
                           makeVector(68.6859, 5.18034, 13.6691) };
vec3 stats_camera_dir[] = {makeVector(-0.179715, -0.0878783, -0.979786),
                           makeVector(0.927574, -0.22893, -0.295292) ,
                           makeVector(-0.820819, -0.478219, -0.31235) };


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

        const pixel_sample_cl samples1[] = {{ 0.f , 0.f, 1.f}};
        const pixel_sample_cl samples4[] = {{ 0.25f , 0.25f, 0.25f},
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
        case 'p':
                std::cout << std::endl;
                std::cout << "Camera Pos:\n" << camera.pos << std::endl;
                std::cout << "Camera Dir:\n" << camera.dir << std::endl;
                break;
        case 'u':
                camera.set(stats_camera_pos[0],//pos 
                           stats_camera_dir[0],//dir
                           makeVector(0,1,0), //up
                           M_PI/4.,
                           window_size[0] / (float)window_size[1]);
        
                break;
        case 'i':
                camera.set(stats_camera_pos[1],//pos 
                           stats_camera_dir[1],//dir
                           makeVector(0,1,0), //up
                           M_PI/4.,
                           window_size[0] / (float)window_size[1]);
        
                break;
        case 'o':
                camera.set(stats_camera_pos[2],//pos 
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
                rt_log << "Camera Pos: " << camera.pos << std::endl;
                rt_log << "Camera Dir: " << camera.dir << std::endl;
                rt_log << "MAX_BOUNCE: "<< MAX_BOUNCE << std::endl;
                rt_log << std::endl;
                logging = true;
                custom_logging = true;
                stats_logged = 0;
                break;
        case 'l':
                clinfo.set_sync(true);
                if (logged)
                        break;
                rt_log.silent = true;
                rt_log.enabled = true;
                logging = true;
                logged = true;
                stats_logged = 0;
                // MAX_BOUNCE = 5;
                MAX_BOUNCE = 9;
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
        static int i = 0;
        static int dir = 1;
        static int total_ray_count = 0;
        static int total_sec_ray_count = 0;

        double bvh_build_time = 0;
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
        light.set_dir(0.05f,-1.f,-0.02f);
        // light.set_dir(0.05f,-1.f,-1.9f);
        light.set_color(1.f,1.f,1.f);
        scene.set_dir_light(light);
        color_cl ambient;
        ambient[0] = ambient[1] = ambient[2] = 0.1f;
        scene.set_ambient_light(ambient);

        if (logging) {
                if (!custom_logging) {
                int32_t stats_index = stats_logged / CONF_STATS_TO_LOG;
                camera.set(stats_camera_pos[stats_index],//pos 
                           stats_camera_dir[stats_index],//dir
                           makeVector(0,1,0), //up
                           M_PI/4.,
                           window_size[0] / (float)window_size[1]);
                }
                if (!(stats_logged % CONF_STATS_TO_LOG) && !custom_logging) {
                                rt_log << "-=-=-=-=-=-=-=-=-=-=-=- Stats for conf "
                                       << (stats_logged / CONF_STATS_TO_LOG + 1)
                                       << " -=-=-=-=-=-=-=-=-=-=-=-\n\n"
                                       << "Camera Pos: " << camera.pos << std::endl
                                       << "Camera Dir: " << camera.dir << std::endl
                                       << std::endl;
                }
                rt_log << "---------------------" << std::endl;
        }

        if (scenes.size() && false) {
                Mesh& mesh = scene.get_aggregate_mesh();
                Mesh& pose_mesh = scenes[current_model].get_aggregate_mesh();
                
                for (size_t v = 0; v < mesh.vertexCount(); ++ v) {
                        mesh.vertex(v) = pose_mesh.vertex(v);
                }
                if ( scene.update_aggregate_mesh_vertices()) {
                        std::cout << "Error updating vertices" << std::endl;
                }
                device.finish_commands();
                
                current_model = (current_model+1)%model_count;

        }

	// Mesh& mesh = scene.get_aggregate_mesh();
        // for (size_t v = 0; v < mesh.vertexCount(); ++ v) {
	// 	Vertex& vert = mesh.vertex(v);
	// 	Vertex& orig_vert = orig_mesh.vertex(v);
        //         vert.position.s[0] = orig_vert.position.s[0] + sin(frame*0.1);
        //         vert.position.s[1] = orig_vert.position.s[1] + cos(frame*0.06);
        //         vert.position.s[2] = orig_vert.position.s[2] + sin(frame*0.08);
        //         // vert.position.s[0] *= 1.01f;
        //         // vert.position.s[1] *= 0.99f;
        //         // vert.position.s[2] *= 1.01f;
        // }                
        // scene.update_aggregate_mesh_vertices();
        // frame ++;

        if (GPU_BVH_BUILD) {
                if (bvh_builder.build_bvh(scene)) {
                        std::cout << "BVH builder failed." << std::endl;
                        pause_and_exit(1);
                }
                bvh_build_time = bvh_builder.get_exec_time();
        }


        int32_t sample_count = pixel_count * prim_ray_gen.get_spp();
        for (int32_t offset = 0; offset < sample_count; offset+= tile_size) {

		RayBundle* ray_in =  &ray_bundle_1;
		RayBundle* ray_out = &ray_bundle_2;

                if (sample_count - offset < tile_size)
                        tile_size = sample_count - offset;

                if (prim_ray_gen.set_rays(camera, *ray_in, window_size,
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
                        if (sec_ray_count == ray_out->count())
                                std::cerr << "Max sec rays reached!\n";

                        total_ray_count += sec_ray_count;
                        total_sec_ray_count += sec_ray_count;

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

        if (device.release_graphic_resource(tex_id) ||
            cubemap.release_graphic_resources() || 
            scene.release_graphic_resources()) {
            std::cerr << "Error releasing texture resource." << std::endl;
            pause_and_exit(1);
        }
        device.finish_commands();
        double total_msec = rt_time.msec_since_snap();
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
                  << "\t(" << total_ray_count - total_sec_ray_count << " primary, " 
                  << total_sec_ray_count << " secondary)"
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
        total_sec_ray_count = 0;

        rt_log << "\nBVH Build time:\t" << bvh_build_time << std::endl;
        rt_log << "Prim Gen time: \t" << prim_gen_time << std::endl;
        rt_log << "Sec Gen time: \t" << sec_gen_time << std::endl;
        rt_log << "Tracer time: \t" << prim_trace_time + sec_trace_time  
                  << " (" <<  prim_trace_time << " - " << sec_trace_time 
                  << ")" << std::endl;
        rt_log << "Shadow time: \t" << prim_shadow_trace_time + sec_shadow_trace_time 
                  << " (" <<  prim_shadow_trace_time 
                  << " - " << sec_shadow_trace_time << ")" << std::endl;
        rt_log << "Shader time: \t" << shader_time << std::endl;
        rt_log << "Fb clear time: \t" << fb_clear_time << std::endl;
        rt_log << "Fb copy time: \t" << fb_copy_time << std::endl;
        rt_log << std::endl;

        if (logging) {
                stats_logged++;
                if (stats_logged >= TOTAL_STATS_TO_LOG || custom_logging) {
                        logging = false;
                        custom_logging = false;
                        stats_logged = 0;
                        rt_log.silent = true;
                        rt_log.enabled = false;
                }
        }


        if (print_fps) {
                glRasterPos2f(-0.95f,0.9f);
                std::stringstream ss;
                ss << "FPS: " << (1000.f / total_msec);
                if (logging) {
                        ss << "   LOGGING";
                }
                std::string fps_s = ss.str();
                for (int i = 0; i < fps_s.size(); ++i)
                        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *(fps_s.c_str()+i));
        }

        glutSwapBuffers();
}

int main (int argc, char** argv)
{

        /*---------------------- Initialize OpenGL and OpenCL ----------------------*/

        if (rt_log.initialize("rt-log-boat")){
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
        /* Initialize generic gpu library */
        DeviceFunctionLibrary::initialize(clinfo);

        /*---------------------- Create shared GL-CL texture ----------------------*/
        gl_tex = create_tex_gl(window_size[0],window_size[1]);
        
        tex_id = device.new_memory();
        DeviceMemory& tex_mem = device.memory(tex_id);
        if (tex_mem.initialize_from_gl_texture(gl_tex)) {
                std::cerr << "Failed to create memory object from gl texture" << std::endl;
                pause_and_exit(1);
        }

        //////////////////// test ///////////////////////////////
        // memory_id in_mem_id  = device.new_memory();
        // DeviceMemory& in_mem = device.memory(in_mem_id);
        
        // in_mem.initialize(sizeof(cl_uint));

        // for (int j = 0; j < 100; ++j) {
        //         int N = 10001 + j * 100;
        //         std::vector<uint32_t> vals(N);
        //         uint32_t gt_total = 0;
        //         for (int i = 0 ; i < N ; ++i) {
        //                 vals[i] = rand()%100;
        //                 gt_total += vals[i];
        //         }

        //         if (in_mem.resize(sizeof(cl_uint)*(N+1)))
        //                 return -1;

        //         if (in_mem.write(sizeof(cl_uint)*(N+1), &(vals[0])))
        //                 return -1;

        //         // std::cout << "Initialized\n";
        //         gpu_scan_uint(device, in_mem_id, N+1, in_mem_id);

        //         if (in_mem.read(sizeof(cl_uint)*N, &(vals[0])))
        //                 return -1;

        //         uint32_t total;
        //         if (in_mem.read(sizeof(cl_uint), &total, sizeof(cl_uint)*N))
        //                 return -1;

        //         // std::cout << "Vals: " << std::endl;
        //         // for (int i = 0 ; i < N ; ++i) {
        //         //         std::cout << vals[i] << " ";
        //         // }
        //         // std::cout << std::endl;

        //         std::cout << "N: " << N << std::endl;
        //         std::cout << "Total: " << total << std::endl;
        //         std::cout << "GT: " << gt_total << std::endl;
        //         if (total != gt_total) {
        //                 std::cout << std::endl << std::endl;
        //                 std::cout << "ERROR --------------------------"<< std::endl;
        //                 std::cout << std::endl << std::endl;
        //                 break;
        //         }
        //         std::cout << std::endl;
        // }
        // return 0;
        /////////////////////////////////////////////////////////

        /*---------------------- Set up scene ---------------------------*/
        if (scene.initialize()) {
                std::cerr << "Failed to initialize scene" << std::endl;
                pause_and_exit(1);
        } else {
                std::cout << "Initialized scene succesfully" << std::endl;
        }

        if (GPU_BVH_BUILD) {
                if (bvh_builder.initialize(clinfo)) {
                        std::cerr << "Failed to initialize bvh builder" << std::endl;
                        pause_and_exit(1);
                } else {
                        std::cout << "Initialized bvh builder succesfully" << std::endl;
                }
        }

        //// Ben sequence
        // // model_count = 1;
        // model_count = 30;
        // scene.load_obj_file_and_make_objs("models/obj/ben/ben_00.obj");

        // for (int i = 0; i < model_count; ++i) {
        //         std::stringstream ss;
        //         if (i < 10)
        //                 ss << "models/obj/ben/ben_0" << i << ".obj";
        //         else
        //                 ss << "models/obj/ben/ben_" << i << ".obj";
        //         scenes.push_back(Scene());
        //         scenes[i].load_obj_file_and_make_objs(ss.str());
        //         scenes[i].create_aggregate_mesh();
        // }

        //// Hand sequence
        // // model_count = 1;
        // model_count = 44;
        // scene.load_obj_file_and_make_objs("models/obj/hand/hand_00.obj");

        // for (int i = 0; i < model_count; ++i) {
        //         std::stringstream ss;
        //         if (i < 10)
        //                 ss << "models/obj/hand/hand_0" << i << ".obj";
        //         else
        //                 ss << "models/obj/hand/hand_" << i << ".obj";
        //         scenes.push_back(Scene());
        //         scenes[i].load_obj_file_and_make_objs(ss.str());
        //         scenes[i].create_aggregate_mesh();
        // }


        //// Fairy sequence
        // model_count = 21;
        // scene.load_obj_file_and_make_objs("models/obj/fairy_forest/f000.obj");

        // for (int i = 0; i < model_count; ++i) {
        //         std::stringstream ss;
        //         if (i < 10)
        //                 ss << "models/obj/fairy_forest/f0" << i << "0.obj";
        //         else
        //                 ss << "models/obj/fairy_forest/f" << i << "0.obj";
        //         scenes.push_back(Scene());
        //         scenes[i].load_obj_file_and_make_objs(ss.str());
        //         scenes[i].create_aggregate_mesh();
        // }

        // Teapots
        // mesh_id teapot_mesh_id = 
        //         scene.load_obj_file_as_aggregate("models/obj/teapot2.obj");
        //         // scene.load_obj_file_as_aggregate("models/obj/grid100.obj");
        // object_id teapot_obj_id  = scene.add_object(teapot_mesh_id);
        // Object& teapot_obj = scene.object(teapot_obj_id);
        // teapot_obj.geom.setScale(2.f);
        // teapot_obj.geom.setPos(makeVector(0.f,-8.f,0.f));
        // teapot_obj.mat.diffuse = Green;
        // // teapot_obj.mat.diffuse = Blue;
        // teapot_obj.mat.reflectiveness = 0.6f;
        // // teapot_obj.mat.refractive_index = 1.333f;

        // object_id teapot_2_obj_id  = scene.add_object(teapot_mesh_id);
        // Object& teapot_2_obj = scene.object(teapot_2_obj_id);
        // teapot_2_obj.geom.setScale(.8f);
        // teapot_2_obj.geom.setPos(makeVector(16.f,0.f,0.f));
        // teapot_2_obj.geom.setRpy(makeVector(-0.4f,0.f,0.f));
        // teapot_2_obj.mat.diffuse = Blue;
        // // teapot_2_obj.mat.diffuse = Blue;
        // teapot_2_obj.mat.reflectiveness = 0.7f;
        // // teapot_2_obj.mat.refractive_index = 1.333f;


        /// Hand
        // scene.load_obj_file_and_make_objs("models/obj/hand/hand_40.obj");

        /// Ben
        // scene.load_obj_file_and_make_objs("models/obj/ben/ben_00.obj");

        //// Fairy
        // scene.load_obj_file_and_make_objs("models/obj/fairy_forest/f000.obj");

        //// Boat
        mesh_id floor_mesh_id = 
                scene.load_obj_file_as_aggregate("models/obj/frame_water1.obj");
                // scene.load_obj_file_as_aggregate("models/obj/grid100.obj");
        object_id floor_obj_id  = scene.add_object(floor_mesh_id);
        Object& floor_obj = scene.object(floor_obj_id);
        floor_obj.geom.setScale(2.f);
        floor_obj.geom.setPos(makeVector(0.f,-8.f,0.f));
        floor_obj.mat.diffuse = White;
        // floor_obj.mat.diffuse = Blue;
        floor_obj.mat.reflectiveness = 0.98f;
        floor_obj.mat.refractive_index = 1.333f;

         mesh_id boat_mesh_id = 
                 scene.load_obj_file_as_aggregate("models/obj/frame_boat1.obj");
         object_id boat_obj_id = scene.add_object(boat_mesh_id);
         Object& boat_obj = scene.object(boat_obj_id);
         boat_obj.geom.setPos(makeVector(0.f,-8.f,0.f));
         boat_obj.geom.setRpy(makeVector(0.f,0.f,0.f));
         boat_obj.geom.setScale(2.f);
         boat_obj.mat.diffuse = Red;
         boat_obj.mat.shininess = 1.f;
         boat_obj.mat.reflectiveness = 0.0f;

        /// Dragon
        // mesh_id floor_mesh_id = 
        //         scene.load_obj_file_as_aggregate("models/obj/frame_water1.obj");
        // object_id floor_obj_id  = scene.add_object(floor_mesh_id);
        // Object& floor_obj = scene.object(floor_obj_id);
        // floor_obj.geom.setScale(2.f);
        // floor_obj.geom.setPos(makeVector(0.f,-8.f,0.f));
        // floor_obj.mat.diffuse = Blue;
        // floor_obj.mat.reflectiveness = 0.9f;
        // floor_obj.mat.refractive_index = 1.333f;

        //  mesh_id dragon_mesh_id = 
        //          scene.load_obj_file_as_aggregate("models/obj/dragon.obj");
        //  object_id dragon_obj_id = scene.add_object(dragon_mesh_id);
        //  Object& dragon_obj = scene.object(dragon_obj_id);
        //  dragon_obj.geom.setPos(makeVector(0.f,-8.f,0.f));
        //  dragon_obj.geom.setRpy(makeVector(0.f,0.f,0.f));
        //  dragon_obj.geom.setScale(2.f);
        //  dragon_obj.mat.diffuse = Red;
        //  dragon_obj.mat.shininess = 1.f;
        //  dragon_obj.mat.reflectiveness = 0.7f;

        //// Buddha
        // std::vector<mesh_id> box_meshes = 
        //         scene.load_obj_file("models/obj/box-no-ceil.obj");
        // std::vector<object_id> box_objs = scene.add_objects(box_meshes);

        // for (uint32_t i = 0; i < box_objs.size(); ++i) {
        //         Object& obj = scene.object(box_objs[i]);
        //         obj.geom.setRpy(makeVector(0.f,0.f,0.4f));
        //         if (obj.mat.texture > 0)
        //                 obj.mat.reflectiveness = 0.8f;
        //         // obj.geom.setPos(makeVector(0.f,-30.f,0.f));
        // }

        //  mesh_id buddha_mesh_id = 
        //          scene.load_obj_file_as_aggregate("models/obj/buddha.obj");
        //  object_id buddha_obj_id = scene.add_object(buddha_mesh_id);
        //  Object& buddha_obj = scene.object(buddha_obj_id);
        //  buddha_obj.geom.setPos(makeVector(0.f,-4.f,0.f));
        //  buddha_obj.geom.setRpy(makeVector(0.f,0.f,0.f));
        //  buddha_obj.geom.setScale(0.3f);
        //  buddha_obj.mat.diffuse = White;
        //  buddha_obj.mat.shininess = 1.f;


#if 1
         /* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- Aggregate BVH -=-=-=-=-=-=-=-=-=-=-=-=-=- */
         if (scene.create_aggregate_mesh()) { 
                std::cerr << "Failed to create aggregate mesh" << std::endl;
                pause_and_exit(1);
         } else {
                 std::cout << "Created aggregate mesh succesfully" << std::endl;
         }

         orig_mesh = scene.get_aggregate_mesh();
         // scene.set_accelerator_type(KDTREE_ACCELERATOR);
         scene.set_accelerator_type(BVH_ACCELERATOR);

         clinfo.set_sync(false);

         if (GPU_BVH_BUILD) {
                 if (scene.transfer_aggregate_mesh_to_device()) {
                         std::cerr << "Failed to transfer aggregate mesh to device memory" 
                                   << std::endl;
                         pause_and_exit(1);
                 } else {
                         std::cout << "Transfered aggregate mesh to device succesfully" 
                                   << std::endl;
                 }

                 if (bvh_builder.build_bvh(scene)) {
                         std::cout << "BVH builder failed." << std::endl;
                         pause_and_exit(1);
                 }

         } else {

                 if (scene.create_aggregate_accelerator()) {
                         std::cerr << "Failed to create aggregate accelerator" << std::endl;
                         pause_and_exit(1);
                 } else {
                         std::cout << "Created aggregate accelerator succesfully" << std::endl;
                 }

                 if (scene.transfer_aggregate_mesh_to_device()) {
                         std::cerr << "Failed to transfer aggregate mesh to device memory" 
                                   << std::endl;
                         pause_and_exit(1);
                 } else {
                         std::cout << "Transfered aggregate mesh to device succesfully" 
                                   << std::endl;
                 }

                 if (scene.transfer_aggregate_accelerator_to_device()) {
                         std::cerr << "Failed to transfer aggregate accelerator to device memory" 
                                   << std::endl;
                         pause_and_exit(1);
                 } else {
                         std::cout << "Transfered aggregate accelerator to device succesfully" 
                                   << std::endl;
                 }
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

        if (ray_bundle_1.initialize(ray_bundle_size)) {
                std::cerr << "Error initializing ray bundle" << std::endl;
                std::cerr.flush();
                pause_and_exit(1);
        }

	if (ray_bundle_2.initialize(ray_bundle_size)) {
		std::cerr << "Error initializing ray bundle 2" << std::endl;
		std::cerr.flush();
		exit(1);
	}
        std::cout << "Initialized ray bundles succesfully" << std::endl;

        /*---------------------- Initialize hit bundle -----------------------------*/
        int32_t hit_bundle_size = ray_bundle_size;

        if (hit_bundle.initialize(hit_bundle_size)) {
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
                               "textures/cubemap/Path/negz.jpg")) {
                std::cerr << "Failed to initialize cubemap." << std::endl;
                pause_and_exit(1);
        }
        std::cerr << "Initialized cubemap succesfully." << std::endl;

        /*------------------------ Initialize FrameBuffer ---------------------------*/
        if (framebuffer.initialize(window_size)) {
                std::cerr << "Error initializing framebuffer." << std::endl;
                pause_and_exit(1);
        }
        std::cout << "Initialized framebuffer succesfully." << std::endl;

        /* ------------------ Initialize ray tracer kernel ----------------------*/

        if (tracer.initialize()){
                std::cerr << "Failed to initialize tracer." << std::endl;
                pause_and_exit(1);
        }
        std::cerr << "Initialized tracer succesfully." << std::endl;


        /* ------------------ Initialize Primary Ray Generator ----------------------*/

        if (prim_ray_gen.initialize()) {
                std::cerr << "Error initializing primary ray generator." << std::endl;
                pause_and_exit(1);
        }
        std::cout << "Initialized primary ray generator succesfully." << std::endl;
                

        /* ------------------ Initialize Secondary Ray Generator ----------------------*/
        if (sec_ray_gen.initialize()) {
                std::cerr << "Error initializing secondary ray generator." << std::endl;
                pause_and_exit(1);
        }
        sec_ray_gen.set_max_rays(ray_bundle_1.count());
        std::cout << "Initialized secondary ray generator succesfully." << std::endl;

        /*------------------------ Initialize RayShader ---------------------------*/
        if (ray_shader.initialize()) {
                std::cerr << "Error initializing ray shader." << std::endl;
                pause_and_exit(1);
        }
        std::cout << "Initialized ray shader succesfully." << std::endl;

        /*----------------------- Enable timing in all clases -------------------*/
        bvh_builder.timing(true);
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
        total_cl_mem += ray_bundle_1.mem().size()*2;
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
        std::cerr << "sample_cl size: "
                  << sizeof(sample_cl)
                  << std::endl;
        std::cerr << "sample_trace_info_cl size: "
                  << sizeof(sample_trace_info_cl)
                  << std::endl;


        if (scene.get_accelerator_type() == BVH_ACCELERATOR) {
                std::cerr << "sizeof(BVHNode): " << sizeof(BVHNode) << std::endl;
                std::cerr << "sizeof(bvh_root): " << sizeof(BVHRoot) << std::endl;
                std::cerr << "bvh_root_mem_size: " << scene.bvh_roots_mem().size() 
                          << std::endl
                          << "object_count * sizeof(bvhroot): " 
                          << scene.object_count()*sizeof(BVHRoot) 
                          << std::endl;
        }
        if (scene.get_accelerator_type() == KDTREE_ACCELERATOR) {
                std::cerr << "sizeof(KDTNode): " << sizeof(KDTNode) << std::endl;
        }

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

