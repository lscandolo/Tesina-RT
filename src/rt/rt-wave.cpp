#include <algorithm>
#include <iostream>

#include <cl-gl/opengl-init.hpp>
#include <cl-gl/opencl-init.hpp>
#include <rt/rt.hpp>

#define MAX_BOUNCE 5
#define WAVE_HEIGHT 0.2

CLInfo clinfo;
GLInfo glinfo;
Scene scene;

DeviceInterface device;
memory_id tex_id;
function_id mangler_id;

bool gpu_mangling = true;

RayBundle             ray_bundle_1,ray_bundle_2;
HitBundle             hit_bundle;
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

#define STEPS 16

void gl_mouse(int x, int y)
{
	float delta = 0.001f;
	float d_inc = delta * (window_size[1]*0.5f - y);/* y axis points downwards */
	float d_yaw = delta * (x - window_size[0]*0.5f);

	float max_motion = 0.05f;

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

	const sample_cl samples1[] = {{ 0.f , 0.f, 1.f}};
	const sample_cl samples4[] = {{ 0.25f , 0.25f, 0.25f},
				      { 0.25f ,-0.25f, 0.25f},
				      {-0.25f , 0.25f, 0.25f},
				      {-0.25f ,-0.25f, 0.25f}};
	const sample_cl samples9[] = {{-0.33f , 0.33f, 0.11111f},
				      { 0.f   , 0.33f, 0.11111f},
				      { 0.33f , 0.33f, 0.11111f},
				      {-0.33f , 0.f, 0.11111f},
				      { 0.f   , 0.f, 0.11111f},
				      { 0.33f , 0.f, 0.11111f},
				      {-0.33f , 0.33f, 0.11111f},
				      { 0.f   , 0.33f, 0.11111f},
				      { 0.33f , 0.33f, 0.11111f}};
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
	case 'g':
		gpu_mangling = true;
		break;
	case 'c':
		gpu_mangling = false;
		break;
	case '1': /* Set 1 sample per pixel */
		if (prim_ray_gen.set_spp(1,samples1)){
			std::cerr << "Error seting spp" << std::endl;
			exit(1);
		}
		break;
	case '4': /* Set 4 samples per pixel */
		if (prim_ray_gen.set_spp(4,samples4)){
			std::cerr << "Error seting spp" << std::endl;
			exit(1);
		}
		break;
	case '9': /* Set 9 samples per pixel */
		if (prim_ray_gen.set_spp(9,samples9)){
			std::cerr << "Error seting spp" << std::endl;
			exit(1);
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
                    exit(1);
    }

        if (framebuffer.clear()) {
		std::cerr << "Failed to clear framebuffer." << std::endl;
		exit(1);
	}
	fb_clear_time = framebuffer.get_clear_exec_time();

	cl_int arg = i%STEPS;
	int32_t tile_size = best_tile_size;

	mangler_arg += 1.f;
	rt_time_t mangle_timer;

        DeviceFunction& mangler_function = device.function(mangler_id);

	/*-------------- Mangle verts in OpenCL---------------------*/
	if (gpu_mangling) {
		std::cerr << "Using GPU..." << std::endl;
		mangle_timer.snap_time();
                mangler_function.set_arg(1,sizeof(cl_float),&mangler_arg);
                mangler_function.set_arg(2,sizeof(cl_float),&last_mangler_arg);

                if (mangler_function.execute()) {
			std::cerr << "Error executing mangler." << std::endl;
			exit(1);
                }
		
	} else {
		/*-------------- Mangle verts in CPU ---------------------*/
		std::cerr << "Using CPU..." << std::endl;
		mangle_timer.snap_time();
		Mesh& mesh = scene.get_aggregate_mesh();
		for (uint32_t v = 0; v < mesh.vertexCount(); ++ v) {
			Vertex& vert = mesh.vertex(v);
			vert.position.s[1] = WAVE_HEIGHT * sin(0.33*mangler_arg+vert.position.s[0]);
			vert.position.s[1] *= cos(0.57*mangler_arg+vert.position.s[2]);
			vert.normal.s[0] = cos(0.33*mangler_arg+vert.position.s[0]);
			vert.normal.s[2] = -sin(0.57*mangler_arg+vert.position.s[2]);
		}
                DeviceMemory& vertex_mem = scene.vertex_mem();
                vertex_mem.write(mesh.vertexCount() * sizeof(Vertex),mesh.vertexArray());
	}
	/*--------------------------------------*/
	double mangle_time = mangle_timer.msec_since_snap();
	std::cerr << "Mangle time: "  << mangle_time << " msec." << std::endl;
	last_mangler_arg = mangler_arg;
	

	directional_light_cl light;
        light.set_dir(1.f,-1.f,0.f);
        light.set_color(1.f,1.f,1.f);
	// light.set_dir(0.05f * (arg - 8.f) , -0.6f, 0.2f);
	// light.set_color(0.05f * (fabsf(arg)) + 0.1f, 0.2f, 0.05f * fabsf(arg+4.f));
	scene.set_dir_light(light);
	color_cl ambient;
	ambient[0] = ambient[1] = ambient[2] = 0.1f;
	scene.set_ambient_light(ambient);

	int32_t sample_count = pixel_count * prim_ray_gen.get_spp();
	for (int32_t offset = 0; offset < sample_count; offset+= tile_size) {

		
		RayBundle* ray_in  =  &ray_bundle_1;
		RayBundle* ray_out = &ray_bundle_2;

		if (sample_count - offset < tile_size)
			tile_size = sample_count - offset;

		if (prim_ray_gen.set_rays(camera, ray_bundle_1, window_size,
                                          tile_size, offset)) {
			std::cerr << "Error seting primary ray bundle" << std::endl;
			exit(1);
		}
		prim_gen_time += prim_ray_gen.get_exec_time();

		total_ray_count += tile_size;

		if (tracer.trace(scene, tile_size, *ray_in, hit_bundle)){
			std::cerr << "Failed to trace." << std::endl;
			exit(1);
		}
		prim_trace_time += tracer.get_trace_exec_time();

		if (tracer.shadow_trace(scene, tile_size, *ray_in, hit_bundle)){
			std::cerr << "Failed to shadow trace." << std::endl;
			exit(1);
		}
		prim_shadow_trace_time += tracer.get_shadow_exec_time();

		if (ray_shader.shade(*ray_in, hit_bundle, scene,
                                     cubemap, framebuffer, tile_size,true)){
			std::cerr << "Failed to update framebuffer." << std::endl;
			exit(1);
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
				std::cerr << "Max sec rays reached!n";

			total_ray_count += sec_ray_count;

			if (tracer.trace(scene, sec_ray_count, 
                                         *ray_in, hit_bundle, true)) {
                                std::cerr << "Failed to trace." << std::endl;
                                exit(1);
                        }
                        sec_trace_time += tracer.get_trace_exec_time();

			if (tracer.shadow_trace(scene, sec_ray_count, 
                                                *ray_in, hit_bundle, true)) {
                                std::cerr << "Failed to shadow trace." << std::endl;
                                exit(1);
                        }
			sec_shadow_trace_time += tracer.get_shadow_exec_time();

			if (ray_shader.shade(*ray_in, hit_bundle, scene,
                                             cubemap, framebuffer, sec_ray_count)){
				std::cerr << "Ray shader failed execution." << std::endl;
				exit(1);
			}
			shader_time += ray_shader.get_exec_time();
		}

	}

	if (framebuffer.copy(device.memory(tex_id))){
		std::cerr << "Failed to copy framebuffer." << std::endl;
		exit(1);
	}
	fb_copy_time = framebuffer.get_copy_exec_time();
	double total_msec = rt_time.msec_since_snap();

        if (device.release_graphic_resource(tex_id) ||
            cubemap.release_graphic_resources() ||
            scene.release_graphic_resources()) {
                std::cerr << "Error releasing texture resource." << std::endl;
                exit(1);
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
	std::cout << "Time elapsed: " 
		  << total_msec << " milliseconds " 
		  << "\t" 
		  << (1000.f / total_msec)
		  << " FPS"
		  << "\t"
		  << total_ray_count
		  << " rays casted "
		  << "\t(" << pixel_count << " primary, " 
		  << total_ray_count-pixel_count << " secondary)"
		  << "               \r" ;
	std::flush(std::cout);
	rt_time.snap_time();
	total_ray_count = 0;

	std::cout << "\nPrim Gen time: \t" << prim_gen_time  << std::endl;
	std::cout << "Sec Gen time: \t" << sec_gen_time << std::endl;
	std::cout << "Tracer time: \t" << prim_trace_time + sec_trace_time  
		  << " (" <<  prim_trace_time << " - " << sec_trace_time 
		  << ")" << std::endl;
	std::cout << "Shadow time: \t" << prim_shadow_trace_time + sec_shadow_trace_time 
		  << " (" <<  prim_shadow_trace_time 
		  << " - " << sec_shadow_trace_time << ")" << std::endl;
	std::cout << "Shader time: \t " << shader_time << std::endl;
	std::cout << "Fb clear time: \t" << fb_clear_time << std::endl;
	std::cout << "Fb copy time: \t" << fb_copy_time << std::endl;
	std::cout << std::endl;

	glutSwapBuffers();
}

int main (int argc, char** argv)
{

	/*---------------------- Initialize OpenGL and OpenCL ----------------------*/

	if (init_gl(argc,argv,&glinfo, window_size, "RT") != 0){
		std::cerr << "Failed to initialize GL" << std::endl;
		exit(1);
	} else { 
		std::cout << "Initialized GL succesfully" << std::endl;
	}

	if (init_cl(glinfo,&clinfo) != CL_SUCCESS){
		std::cerr << "Failed to initialize CL" << std::endl;
		exit(1);
	} else { 
		std::cout << "Initialized CL succesfully" << std::endl;
	}
	print_cl_info(clinfo);

        /* Initialize device interface */
        if (device.initialize(clinfo)) {
                std::cerr << "Failed to initialize device interface" << std::endl;
                exit(1);
        }

	/*---------------------- Create shared GL-CL texture ----------------------*/
	gl_tex = create_tex_gl(window_size[0],window_size[1]);

        tex_id = device.new_memory();
        DeviceMemory& tex_mem = device.memory(tex_id);
        if (tex_mem.initialize_from_gl_texture(gl_tex)) {
                std::cerr << "Failed to create memory object from gl texture" << std::endl;
                exit(1);
        }
	/*---------------------- Set up scene ---------------------------*/
        if (scene.initialize(clinfo)) {
                std::cerr << "Failed to initialize scene" << std::endl;
                exit(1);
        } else {
                std::cout << "Initialized scene succesfully" << std::endl;
        }

	mesh_id floor_mesh_id = scene.load_obj_file_as_aggregate("models/obj/grid100.obj");
	object_id floor_obj_id  = scene.add_object(floor_mesh_id);
	Object& floor_obj = scene.object(floor_obj_id);
 	floor_obj.geom.setScale(10.f);
	// floor_obj.geom.setPos(makeVector(0.f,0.f,0.f));
	floor_obj.mat.diffuse = White;
	floor_obj.mat.reflectiveness = 0.95f;
	floor_obj.mat.refractive_index = 1.5f;
	floor_obj.slack = makeVector(0.f,WAVE_HEIGHT,0.f);

	scene.create_aggregate_mesh();
	Mesh& scene_mesh = scene.get_aggregate_mesh();
	std::cout << "Created scene aggregate succesfully." << std::endl;

	rt_time.snap_time();
	scene.create_aggregate_bvh();
	BVH& scene_bvh   = scene.get_aggregate_bvh ();
	double bvh_build_time = rt_time.msec_since_snap();
	std::cout << "Created BVH succesfully (build time: " 
		  << bvh_build_time << " msec, " 
		  << scene_bvh.nodeArraySize() << " nodes)." << std::endl;


        scene.transfer_aggregate_mesh_to_device();
        scene.transfer_aggregate_bvh_to_device();
	/*---------------------- Set initial Camera paramaters -----------------------*/
	camera.set(makeVector(0,3,-30), makeVector(0,0,1), makeVector(0,1,0), M_PI/4.,
		   window_size[0] / (float)window_size[1]);


	/*---------------------------- Set tile size ------------------------------*/
	best_tile_size = clinfo.max_compute_units * clinfo.max_work_item_sizes[0];
	best_tile_size *= 64;
	best_tile_size = std::min(pixel_count, best_tile_size);

	/*---------------------- Initialize ray bundles -----------------------------*/
	int32_t ray_bundle_size = best_tile_size * 3;

	if (ray_bundle_1.initialize(ray_bundle_size, clinfo)) {
		std::cerr << "Error initializing ray bundle 1" << std::endl;
		std::cerr.flush();
		exit(1);
	}

	if (ray_bundle_2.initialize(ray_bundle_size, clinfo)) {
		std::cerr << "Error initializing ray bundle 2" << std::endl;
		std::cerr.flush();
		exit(1);
	}
	std::cout << "Initialized ray bundles succesfully" << std::endl;

	/*---------------------- Initialize hit bundle -----------------------------*/
	int32_t hit_bundle_size = ray_bundle_size;

	if (hit_bundle.initialize(hit_bundle_size, clinfo)) {
		std::cerr << "Error initializing hit bundle" << std::endl;
		std::cerr.flush();
		exit(1);
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
		exit(1);
	}
	std::cerr << "Initialized cubemap succesfully." << std::endl;

	/*------------------------ Initialize FrameBuffer ---------------------------*/
	if (framebuffer.initialize(clinfo, window_size)) {
		std::cerr << "Error initializing framebuffer." << std::endl;
		exit(1);
	}
	std::cout << "Initialized framebuffer succesfully." << std::endl;

	/* ------------------ Initialize ray tracer kernel ----------------------*/

	if (tracer.initialize(clinfo)){
		std::cerr << "Failed to initialize tracer." << std::endl;
		return 0;
	}
	std::cerr << "Initialized tracer succesfully." << std::endl;


	/* ------------------ Initialize Primary Ray Generator ----------------------*/

	if (prim_ray_gen.initialize(clinfo)) {
		std::cerr << "Error initializing primary ray generator." << std::endl;
		exit(1);
	}
	std::cout << "Initialized primary ray generator succesfully." << std::endl;


	/* ------------------ Initialize Secondary Ray Generator ----------------------*/
	if (sec_ray_gen.initialize(clinfo)) {
		std::cerr << "Error initializing secondary ray generator." << std::endl;
		exit(1);
	}
	sec_ray_gen.set_max_rays(ray_bundle_1.count());
	std::cout << "Initialized secondary ray generator succesfully." << std::endl;

	/*------------------------ Initialize RayShader ---------------------------*/
	if (ray_shader.initialize(clinfo)) {
		std::cerr << "Error initializing ray shader." << std::endl;
		exit(1);
	}
	std::cout << "Initialized ray shader succesfully." << std::endl;

	/*----------------------- Enable timing in all clases -------------------*/
	framebuffer.timing(true);
	prim_ray_gen.timing(true);
	sec_ray_gen.timing(true);
	tracer.timing(true);
	ray_shader.timing(true);

	/* ------------------------- Create vertex mangler -----------------------*/
        mangler_id = device.new_function();
        DeviceFunction& mangler_function = device.function(mangler_id);
        if (mangler_function.initialize("src/kernel/mangler.cl", "mangle")) {
		std::cerr << "Error initializing mangler kernel." << std::endl;
		exit(1);
	}

        mangler_function.set_dims(1);
        size_t mangler_global_size[] = {scene_mesh.vertexCount(), 0 , 0};
	cl_float h = WAVE_HEIGHT;
        mangler_function.set_global_size(mangler_global_size);
        if (mangler_function.set_arg(0, scene.vertex_mem()))
                std::cerr << "Error setting mangler argument 0" << std::endl;
        if (mangler_function.set_arg(3,sizeof(cl_float), &h))
                std::cerr << "Error setting mangler argument 3" << std::endl;

	/*---------------------- Print scene data ----------------------*/
	std::cerr << "\nScene stats: " << std::endl;
	std::cerr << "\tTriangle count: " << scene_mesh.triangleCount() << std::endl;
	std::cerr << "\tVertex count: " << scene_mesh.vertexCount() << std::endl;

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

	/*------------------------ Set GLUT and misc functions -----------------------*/
	rt_time.snap_time();

	glutKeyboardFunc(gl_key);
	glutMotionFunc(gl_mouse);
	glutDisplayFunc(gl_loop);
	glutIdleFunc(gl_loop);
	glutMainLoop();	

	clinfo.release_resources();

	return 0;
}

