#include <algorithm>
#include <iostream>

#include <cl-gl/opengl-init.hpp>
#include <cl-gl/opencl-init.hpp>
#include <rt/rt.hpp>

#define		GRID_SIZE_X	  100							// Width Of Our Water Height Map (NEW)
#define		GRID_SIZE_Y	  100							// Width Of Our Water Height Map (NEW)
#define		STEP_SIZE	  1							    // Width And Height Of Each Quad (NEW)
#define		HEIGHT_RATIO  1.5//1.5							// Ratio That The Y Is Scaled According To The X And Z (NEW)

//Fluid Module
extern "C" {
        void __declspec(dllimport) __stdcall LBM_initialize(int,int,double,double);
        void __declspec(dllimport) __stdcall LBM_update();
        double __declspec(dllimport) __stdcall LBM_getRho(int,int);
        void __declspec(dllimport) __stdcall LBM_addEvent(int,int,int,int,double);
        int __declspec(dllimport) __stdcall LBM_getWidth();
        int __declspec(dllimport) __stdcall LBM_getHeight();
}

#define MAX_BOUNCE 5
#define WAVE_HEIGHT 0.3

void rainEvent();

CLInfo clinfo;
GLInfo glinfo;

int loging_state = 0;

DeviceInterface& device = *DeviceInterface::instance();
memory_id tex_id;
mesh_id   grid_id;
object_id boat_id;

RayBundle             ray_bundle_1,ray_bundle_2;
HitBundle             hit_bundle;
Scene                 scene;
PrimaryRayGenerator   prim_ray_gen;
SecondaryRayGenerator sec_ray_gen;
RayShader             ray_shader;
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

#define STEPS 16

void gl_mouse(int x, int y)
{
	float delta = 0.005f;
	float d_inc = delta * (window_size[1]*0.5f - y);/* y axis points downwards */
	float d_yaw = delta * (x - window_size[0]*0.5f);

	float max_motion = 0.3f;

	d_inc = std::min(std::max(d_inc, -max_motion), max_motion);
	d_yaw = std::min(std::max(d_yaw, -max_motion), max_motion);

	if (d_inc == 0.f && d_yaw == 0.f)
		return;

	camera.modifyAbsYaw(d_yaw);
	camera.modifyPitch(d_inc);
	glutWarpPointer(window_size[0] * 0.5f, window_size[1] * 0.5f);
}

void gl_key(unsigned char key, int x, int y)
{
	float delta = 2.f;
        static vec3 boat_pos = makeVector(0.f,-9.f,0.f);
        Object& boat_obj = scene.object(boat_id);
	const pixel_sample_cl samples1[] = {{ 0.f , 0.f, 1.f}};
	const pixel_sample_cl samples4[] = {{ 0.25f , 0.25f, 0.25f},
				      { 0.25f ,-0.25f, 0.25f},
				      {-0.25f , 0.25f, 0.25f},
				      {-0.25f ,-0.25f, 0.25f}};

	switch (key){
	case 'a':
		camera.panRight(-delta);
		break;
	case 's':
		camera.panForward(-delta);
		break;
	case 'w':
		camera.panForward(delta);
		break;
	case 'd':
		camera.panRight(delta);
		break;
	case 'q':
		std::cout << std::endl << "Exiting..." << std::endl;
		exit(1);
		break;
	case 'l':
		if (rt_log.initialize("rt-lbm-log")){
			std::cerr << "Error initializing log!" << std::endl;
		}
		loging_state = 1;
		rt_log.enabled = true;
		rt_log << "SPP: " << prim_ray_gen.get_spp() << std::endl;
        case 'u':
                boat_pos[2] += 1.f;
                boat_obj.geom.setPos(boat_pos);
                scene.update_bvh_roots();
                break;
        case 'j':
                boat_pos[2] -= 1.f;
                boat_obj.geom.setPos(boat_pos);
                scene.update_bvh_roots();
                break;
        case 'h':
                boat_pos[0] -= 1.f;
                boat_obj.geom.setPos(boat_pos);
                scene.update_bvh_roots();
                break;
        case 'k':
                boat_pos[0] += 1.f;
                boat_obj.geom.setPos(boat_pos);
                scene.update_bvh_roots();
                break;
        case 'r':
		LBM_addEvent(50,50,53,53,0.035f);
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
	}
}


void gl_loop()
{
	static int i = 0;
	static cl_float mangler_arg = 0;
	static cl_float last_mangler_arg = -1000;
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

	cl_int arg = i%STEPS;
	size_t tile_size = best_tile_size;

	mangler_arg += 1.f;
	rt_time_t mangle_timer;

	if (loging_state){
		if(loging_state == 1){
			rt_log << "Camera configuration 'o' " << std::endl;
			camera.set(makeVector(0,3,-30), makeVector(0,0,1), makeVector(0,1,0), M_PI/4.,
                                   window_size[0] / (float)window_size[1]);
		} else if (loging_state == 3){
			rt_log << "Camera configuration 'i' " << std::endl;
			camera.set(makeVector(0,5,-30), makeVector(0,-0.5,1), makeVector(0,1,0), M_PI/4.,
                                   window_size[0] / (float)window_size[1]);
		} else if (loging_state == 5) {
			rt_log << "Camera configuration 'k' " << std::endl;
			camera.set(makeVector(0,25,-60), makeVector(0,-0.5,1), makeVector(0,1,0), M_PI/4.,
                                   window_size[0] / (float)window_size[1]);
		} else if (loging_state == 7) {
			rt_log << "Camera configuration 'l' " << std::endl;
			camera.set(makeVector(0,120,0), makeVector(0,-1,0.01), makeVector(0,1,0), M_PI/4.,
                                   window_size[0] / (float)window_size[1]);
		} else if (loging_state == 9) {
			rt_log.enabled = false;
			rt_log.silent = true;
			camera.set(makeVector(0,3,-30), makeVector(0,0,1), makeVector(0,1,0), M_PI/4.,
                                   window_size[0] / (float)window_size[1]);
			std::cout << "Done loging!"	<< std::endl;
			loging_state = 0;
		}
		if (loging_state)
			loging_state++;
	}

	/*-------------- Mangle verts in CPU ---------------------*/
	mangle_timer.snap_time();
	// Create new rain drop and update the lbm simulator
	rainEvent();
	LBM_update();
	/////
	Mesh& mesh = scene.get_mesh(grid_id);
        for (size_t v = 0; v < mesh.vertexCount(); ++ v) {
		Vertex& vert = mesh.vertex(v);
		float x = (vert.position.s[0] - 5.f)/10.f;
		float z = (vert.position.s[2] - 5.f)/10.f;
		
		x = (x + 1) * GRID_SIZE_X;
		z = (z + 1) * GRID_SIZE_Y;
		
		if (x < 1.f)
			x = 1.f;
		if (z < 1.f)
			z = 1.f;

		const float DROP_HEIGHT = 18.f;
		//const float DROP_HEIGHT = 10.f;
		
		float y = LBM_getRho((int)x,(int)z);
		
		y = y - 0.05f;
		y = y * DROP_HEIGHT;
		
		vert.position.s[1] = y;

		vec3 normalx = makeVector(0.f,1.f,0.f);
                vec3 normalz = makeVector(0.f,1.f,0.f);
        
                normalx[0] = -(LBM_getRho(x+1,z) - LBM_getRho(x-1,z))  * 0.5f * DROP_HEIGHT;
		normalz[2] = -(LBM_getRho(x,z+1) - LBM_getRho(x,z-1))  * 0.5f * DROP_HEIGHT;

                vec3 normal = (normalx.normalized() + normalz.normalized());

		vert.normal = vec3_to_float3(normal.normalized());
		
		if (fabsf(y) > WAVE_HEIGHT)
			std::cerr << "y: " << y << std::endl;
	}
        scene.update_mesh_vertices(grid_id);
		
        /*--------------------------------------*/
        double mangle_time = mangle_timer.msec_since_snap();
        //std::cerr << "Simulation time: "  << mangle_time << " msec." << std::endl;
        last_mangler_arg = mangler_arg;
	
        directional_light_cl light;
        light.set_dir(0.05f * (arg - 8.f) , -0.6f, 0.2f);
        //light.set_color(0.05f * (fabsf(arg)) + 0.1f, 0.2f, 0.05f * fabsf(arg+4.f));
        light.set_color(0.8f,0.8f,0.8f);
        scene.set_dir_light(light);
        color_cl ambient;
        ambient[0] = ambient[1] = ambient[2] = 0.1f;
        scene.set_ambient_light(ambient);

        size_t sample_count = pixel_count * prim_ray_gen.get_spp();
        for (size_t offset = 0; offset < sample_count; offset+= tile_size) {

		
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

                if (tracer.trace(scene, tile_size, *ray_in, hit_bundle)){
                        std::cerr << "Failed to trace." << std::endl;
                        pause_and_exit(1);
                }
                prim_trace_time += tracer.get_trace_exec_time();

                if (tracer.shadow_trace(scene, tile_size, *ray_in, hit_bundle)){
                        std::cerr << "Failed to shadow trace." << std::endl;
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

                        sec_ray_gen.generate(scene, *ray_in, sec_ray_count, 
                                             hit_bundle, *ray_out, &sec_ray_count);
                        sec_gen_time += sec_ray_gen.get_exec_time();

                        std::swap(ray_in,ray_out);

                        if (!sec_ray_count)
                                break;
                        if (sec_ray_count == ray_bundle_1.count())
                                std::cerr << "Max sec rays reached!\n";

                        total_ray_count += sec_ray_count;

                        if (tracer.trace(scene, sec_ray_count, 
                                         *ray_in, hit_bundle, true)) {
                                std::cerr << "Failed to trace." << std::endl;
                                pause_and_exit(1);
                        }
                        sec_trace_time += tracer.get_trace_exec_time();


                        if (tracer.shadow_trace(scene, sec_ray_count, 
                                                *ray_in, hit_bundle, true)) {
                                std::cerr << "Failed to shadow trace." << std::endl;
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

        rt_log << "\nPrim Gen time: \t" << prim_gen_time  << std::endl;
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

        glutSwapBuffers();

}

int main (int argc, char** argv)
{

	rt_log.enabled = true;
	rt_log.silent = false;

	/*---------------------- Initialize OpenGL and OpenCL ----------------------*/

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

	/*---------------------- Initialize lbm simulator ----------------------*/
        LBM_initialize(GRID_SIZE_X, GRID_SIZE_Y, 0.05f, 0.7f);
        //LBM_initialize(GRID_SIZE_X, GRID_SIZE_Y, 0.05f, 0.77f);
	//LBM_initialize(GRID_SIZE_X, GRID_SIZE_Y, 0.01f, 0.2f);

	/*---------------------- Create shared GL-CL texture ----------------------*/
	gl_tex = create_tex_gl(window_size[0],window_size[1]);
	tex_id = device.new_memory();
	DeviceMemory& tex_mem = device.memory(tex_id);
	if (tex_mem.initialize_from_gl_texture(gl_tex)) {
		std::cerr << "Failed to create memory object from gl texture" << std::endl;
		pause_and_exit(1);
	}

	/*---------------------- Set up scene ---------------------------*/
	if (scene.initialize()) {
		std::cerr << "Failed to initialize scene." << std::endl;
		pause_and_exit(1);
	}
	std::cout << "Initialized scene succesfully." << std::endl;
    scene.set_accelerator_type(BVH_ACCELERATOR);

    mesh_id floor_mesh_id = scene.load_obj_file_as_aggregate("models/obj/grid100.obj");
    object_id floor_obj_id  = scene.add_object(floor_mesh_id);
	Object& floor_obj = scene.object(floor_obj_id);
    floor_obj.geom.setPos(makeVector(0.f,-8.f,0.f));
 	floor_obj.geom.setScale(10.f);
	floor_obj.mat.diffuse = Blue;
	floor_obj.mat.reflectiveness = 0.95f;
	floor_obj.mat.refractive_index = 1.5f;
    /*Set slack for bvh*/
    vec3 slack = makeVector(0.f,WAVE_HEIGHT,0.f);
    scene.get_mesh(floor_mesh_id).set_global_slack(slack);

        mesh_id boat_mesh_id = scene.load_obj_file_as_aggregate("models/obj/frame_boat1.obj");
        object_id boat_obj_id = scene.add_object(boat_mesh_id);
        Object& boat_obj = scene.object(boat_obj_id);
        boat_id = boat_obj_id; 
        boat_obj.geom.setPos(makeVector(0.f,-9.f,0.f));
        boat_obj.geom.setRpy(makeVector(0.0f,0.f,0.f));
        boat_obj.geom.setScale(2.f);
        boat_obj.mat.diffuse = Red;
        boat_obj.mat.shininess = 0.3f;
        boat_obj.mat.reflectiveness = 0.0f;

        if (scene.create_bvhs()) {
                std::cerr << "Failed to create bvhs." << std::endl;
                pause_and_exit(1);
        }
        std::cout << "Created bvhs" << std::endl;
    
        if (scene.transfer_meshes_to_device()) {
                std::cerr << "Failed to transfer meshes to device." 
                          << std::endl;
                pause_and_exit(1);
        }
        std::cout << "Transfered meshes to device" << std::endl;

        if (scene.transfer_bvhs_to_device()) {
                std::cerr << "Failed to transfer bvhs to device." 
                          << std::endl;
                pause_and_exit(1);
        }
        std::cout << "Transfered bvhs to device" << std::endl;
        grid_id = floor_mesh_id;

	/*---------------------- Set initial Camera paramaters -----------------------*/
	camera.set(makeVector(0,3,-30), makeVector(0,0,1), makeVector(0,1,0), M_PI/4.,
		   window_size[0] / (float)window_size[1]);


	/*---------------------------- Set tile size ------------------------------*/
	best_tile_size = clinfo.max_compute_units * clinfo.max_work_item_sizes[0];
	best_tile_size *= 64;
	best_tile_size = std::min(pixel_count, best_tile_size);

	/*---------------------- Initialize ray bundles -----------------------------*/
	size_t ray_bundle_size = best_tile_size * 3;

	if (ray_bundle_1.initialize(ray_bundle_size)) {
		std::cerr << "Error initializing ray bundle 1" << std::endl;
		std::cerr.flush();
		pause_and_exit(1);
	}

	if (ray_bundle_2.initialize(ray_bundle_size)) {
		std::cerr << "Error initializing ray bundle 2" << std::endl;
		std::cerr.flush();
		pause_and_exit(1);
	}
	std::cout << "Initialized ray bundles succesfully" << std::endl;

	/*---------------------- Initialize hit bundle -----------------------------*/
	size_t hit_bundle_size = ray_bundle_size;

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
		return 0;
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
	framebuffer.timing(true);
	prim_ray_gen.timing(true);
	sec_ray_gen.timing(true);
	tracer.timing(true);
	ray_shader.timing(true);

	/*---------------------- Print scene data ----------------------*/
	//Mesh& scene_mesh = scene.get_aggregate_mesh();
	//std::cerr << "\nScene stats: " << std::endl;
	//std::cerr << "\tTriangle count: " << scene_mesh.triangleCount() << std::endl;
	//std::cerr << "\tVertex count: " << scene_mesh.vertexCount() << std::endl;

	/*------------------------- Count mem usage -----------------------------------*/
	size_t total_cl_mem = 0;
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
	std::cerr << "sample_cl size: "
		  << sizeof(sample_cl)
		  << std::endl;
	std::cerr << "sample_trace_info_cl size: "
		  << sizeof(sample_trace_info_cl)
		  << std::endl;

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


void rainEvent(){
	int x,y,size;
	int XMin = 3;
	int XMax = GRID_SIZE_X-5;
	
	int YMin = 3;
	int YMax = GRID_SIZE_Y-5;

	double r = double(rand())/(double(RAND_MAX)+1.0); // random double in range 0.0 to 1.0 (non inclusive)
        x = int(XMin + r*(XMax - XMin)); // transform to wanted range
    
	r = double(rand())/(double(RAND_MAX)+1.0); // random double in range 0.0 to 1.0 (non inclusive)
	y = int(YMin + r*(YMax - YMin)); // transform to wanted range
	
	XMin = 1;
	XMax = 5;

        size = int(XMin + r*(XMax - XMin)); // transform to wanted range

        LBM_addEvent(x,y,x+size,y+size,0.035f);
	//LBM_addEvent(x,y,x+size,y+size,0.035f);
}
