#include <algorithm>
#include <iostream>

#include <cl-gl/opengl-init.hpp>
#include <cl-gl/opencl-init.hpp>
#include <rt/rt.hpp>

#define MAX_BOUNCE 5

cl_mem cl_tex_mem;
cl_mem cl_hit_mem;

RayBundle             ray_bundle_1,ray_bundle_2;
PrimaryRayGenerator   prim_ray_gen;
SecondaryRayGenerator sec_ray_gen;
RayShader             ray_shader;
SceneInfo             scene_info;
Cubemap               cubemap;
FrameBuffer           framebuffer;
Tracer                tracer;
Camera                camera;

GLuint gl_tex;
rt_time_t rt_time;
uint32_t window_size[] = {400, 400};

int32_t pixel_count = window_size[0] * window_size[1];
int32_t best_tile_size;

#define STEPS 16

void gl_mouse(int x, int y)
{
	float delta = 0.001f;
	float d_inc = delta * (window_size[1]*0.5f - y);/* y axis points downwards */
	float d_yaw = delta * (x - window_size[0]*0.5f);

	d_inc = std::min(std::max(d_inc, -0.01f), 0.01f);
	d_yaw = std::min(std::max(d_yaw, -0.01f), 0.01f);

	if (d_inc == 0.f && d_yaw == 0.f)
		return;

	camera.modifyAbsYaw(d_yaw);
	camera.modifyPitch(d_inc);
	glutWarpPointer(window_size[0] * 0.5f, window_size[1] * 0.5f);
}

void gl_key(unsigned char key, int x, int y)
{
	float delta = 2.f;

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
	}
}


void gl_loop()
{
	rt_time_t cl_time;
	static int i = 0;
	static int dir = 1;
	static int ray_count = 0;
	int tile_ray_count = 0;
	int tile_ray_aggregate = 0;
	int tile_ray_max = 0;

	// std::cerr << "Tiles in screen: " << ((float)pixel_count)/tile_size << std::endl;

	glClear(GL_COLOR_BUFFER_BIT);

	if (!framebuffer.clear()) {
		std::cerr << "Failed to clear framebuffer." << std::endl;
		exit(1);
	}

	cl_int arg = i%STEPS;

	int32_t tile_size = best_tile_size;

	for (int32_t offset = 0; offset < pixel_count; offset+= tile_size) {

		
		RayBundle* ray_in =  &ray_bundle_1;
		RayBundle* ray_out = &ray_bundle_2;

		if (pixel_count - offset < tile_size)
			tile_size = pixel_count - offset;


		if (prim_ray_gen.set_rays(camera, ray_bundle_1, window_size,
					  tile_size, offset)) {
			std::cerr << "Error seting primary ray bundle" << std::endl;
			exit(1);
		}


		cl_time.snap_time();

		tile_ray_count = tile_size;
		ray_count += tile_size;
		tracer.trace(scene_info, tile_size, *ray_in, cl_hit_mem);
		tracer.shadow_trace(scene_info, tile_size, *ray_in, cl_hit_mem, arg);

		if (!ray_shader.shade(*ray_in, cubemap, tile_size, arg)){
			std::cerr << "Failed to update framebuffer." << std::endl;
			exit(1);
		}

		int32_t secondary_ray_count;
		sec_ray_gen.generate(scene_info, *ray_in, tile_size,
				     cl_hit_mem, *ray_out, &secondary_ray_count);

		for (uint32_t i = 0; i < MAX_BOUNCE; ++i) {

			std::swap(ray_in,ray_out);

			if (!secondary_ray_count)
				break;

			tile_ray_count += secondary_ray_count;
			ray_count += secondary_ray_count;
			tracer.trace(scene_info, secondary_ray_count, 
				     *ray_in, cl_hit_mem);
			tracer.shadow_trace(scene_info, secondary_ray_count, 
					    *ray_in, cl_hit_mem, arg);

			if (!ray_shader.shade(*ray_in, cubemap, 
						secondary_ray_count, arg)){
				std::cerr << "Ray shader failed execution." << std::endl;
				exit(1);
			}

			sec_ray_gen.generate(scene_info, *ray_in, secondary_ray_count,
					     cl_hit_mem, *ray_out,&secondary_ray_count);

		}

		tile_ray_aggregate += tile_ray_count;
		tile_ray_max = std::max(tile_ray_max,tile_ray_count);

	}

	if (!framebuffer.copy(cl_tex_mem)){
		std::cerr << "Failed to copy framebuffer." << std::endl;
		exit(1);
	}

	// std::cerr << "Max Tile ray count: " << tile_ray_max << std::endl;
	// std::cerr << "Mean Tile ray count: " 
	// 	  << tile_ray_aggregate/(pixel_count/tile_size + 1)
	// 	  << std::endl;
	// double cl_msec = cl_time.msec_since_snap();
	// std::cout << "Time spent on opencl: " << cl_msec << " msec." << std::endl;
	
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
		double msec = rt_time.msec_since_snap();
		std::cout << "Time elapsed: " 
			  << msec << " milliseconds " 
			  << "\t" 
			  << int(STEPS / (msec/1000))
			  << " FPS"
			  << "\t"
			  << int(ray_count / STEPS)
			  << " rays casted (average)"
			  << "               \r" ;
		std::flush(std::cout);
		rt_time.snap_time();
		ray_count = 0;
	}		
	glutSwapBuffers();
}

int main (int argc, char** argv)
{

	CLInfo clinfo;
	GLInfo glinfo;

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

	/*---------------------- Create shared GL-CL texture ----------------------*/
	gl_tex = create_tex_gl(window_size[0],window_size[1]);
	if (create_cl_mem_from_gl_tex(clinfo, gl_tex, &cl_tex_mem))
		exit(1);

	/*---------------------- Set up scene ---------------------------*/
	Scene scene;

	/* Other models 
	models/obj/floor.obj
	models/obj/teapot-low_res.obj
	models/obj/teapot.obj
	models/obj/teapot2.obj
	models/obj/frame_boat1.obj
	models/obj/frame_others1.obj
	models/obj/frame_water1.obj
	*/

	// mesh_id floor_mesh_id = scene.load_obj_file("models/obj/floor.obj");
	mesh_id floor_mesh_id = scene.load_obj_file("models/obj/frame_water1.obj");
	object_id floor_obj_id  = scene.geometry.add_object(floor_mesh_id);
	Object& floor_obj = scene.geometry.object(floor_obj_id);
 	floor_obj.geom.setScale(2.f);
	floor_obj.geom.setPos(makeVector(0.f,-8.f,0.f));
	floor_obj.mat.diffuse = Blue;
	floor_obj.mat.reflectiveness = 0.9f;
	floor_obj.mat.refractive_index = 1.333f;

	mesh_id teapot_mesh_id = scene.load_obj_file("models/obj/teapot2.obj");
	// mesh_id teapot_mesh_id = scene.load_obj_file("models/obj/teapot-low_res.obj");
	object_id teapot_obj_id = scene.geometry.add_object(teapot_mesh_id);
	Object& teapot_obj = scene.geometry.object(teapot_obj_id);
	teapot_obj.geom.setPos(makeVector(-8.f,-5.f,0.f));
	teapot_obj.mat.diffuse = Green;
	teapot_obj.mat.shininess = 1.f;
	teapot_obj.mat.reflectiveness = 0.5f;

	object_id teapot_obj_id_2 = scene.geometry.add_object(teapot_mesh_id);
	Object& teapot_obj_2 = scene.geometry.object(teapot_obj_id_2);
	teapot_obj_2.mat.diffuse = Red;
	teapot_obj_2.mat.shininess = 1.f;
	teapot_obj_2.geom.setPos(makeVector(8.f,5.f,0.f));
	teapot_obj_2.geom.setRpy(makeVector(0.2f,0.1f,0.3f));
	teapot_obj_2.geom.setScale(0.3f);
	teapot_obj_2.mat.reflectiveness = 0.5f;

	// mesh_id boat_mesh_id = scene.load_obj_file("models/obj/frame_boat1.obj");
	// object_id boat_obj_id = scene.geometry.add_object(boat_mesh_id);
	// Object& boat_obj = scene.geometry.object(boat_obj_id);
	// boat_obj.geom.setPos(makeVector(0.f,-18.f,0.f));
	// boat_obj.mat.diffuse = Red;
	// boat_obj.mat.shininess = 1.f;
	// boat_obj.mat.reflectiveness = 0.0f;


	scene.create_aggregate();
	Mesh& scene_mesh = scene.get_aggregate_mesh();
	std::cout << "Created scene aggregate succesfully." << std::endl;

	rt_time.snap_time();
	scene.create_bvh();
	BVH& scene_bvh   = scene.get_aggregate_bvh ();
	double bvh_build_time = rt_time.msec_since_snap();
	std::cout << "Created BVH succesfully (build time: " 
		  << bvh_build_time << " msec, " 
		  << scene_bvh.nodeArraySize() << " nodes)." << std::endl;

	/*---------------------- Initialize SceneInfo ----------------------------*/
	if (!scene_info.initialize(scene,clinfo)) {
		std::cerr << "Failed to initialize scene info." << std::endl;
		exit(1);
	}
	std::cout << "Initialized scene info succesfully." << std::endl;

	/*---------------------- Set initial Camera paramaters -----------------------*/
	camera.set(makeVector(0,3,-30), makeVector(0,0,1), makeVector(0,1,0), M_PI/4.,
		   window_size[0] / (float)window_size[1]);


	/*---------------------------- Set tile size ------------------------------*/
	best_tile_size = clinfo.max_compute_units * clinfo.max_work_item_sizes[0];
	best_tile_size *= 32;

	/*---------------------- Initialize ray bundles -----------------------------*/
	int32_t ray_bundle_size = best_tile_size * 4;

	if (!ray_bundle_1.initialize(ray_bundle_size, clinfo)) {
		std::cerr << "Error initializing ray bundle 1" << std::endl;
		std::cerr.flush();
		exit(1);
	}

	if (!ray_bundle_2.initialize(ray_bundle_size, clinfo)) {
		std::cerr << "Error initializing ray bundle 2" << std::endl;
		std::cerr.flush();
		exit(1);
	}
	std::cout << "Initialized ray bundles succesfully" << std::endl;

	/*------------- Initialize device memory for primary hit info ----------------*/
	uint32_t hit_mem_size = sizeof(RayHitInfo) * ray_bundle_size;
	hit_mem_size *= 4; /* To reuse in order to compute reflection/refraction */


	if (create_empty_cl_mem(clinfo, 
				CL_MEM_READ_WRITE,
				hit_mem_size,
				&cl_hit_mem))
		exit(1);
	/*----------------------- Initialize cubemap ---------------------------*/
	
	if (!cubemap.initialize("textures/cubemap/Path/posx.jpg",
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
	if (!framebuffer.initialize(clinfo, window_size)) {
		std::cerr << "Error initializing framebuffer." << std::endl;
		exit(1);
	}
	std::cout << "Initialized framebuffer succesfully." << std::endl;

	/* ------------------ Initialize ray tracer kernel ----------------------*/

	if (!tracer.initialize(clinfo)){
		std::cerr << "Failed to initialize tracer." << std::endl;
		return 0;
	}
	std::cerr << "Initialized tracer succesfully." << std::endl;


	/* ------------------ Initialize Primary Ray Generator ----------------------*/

	if (!prim_ray_gen.initialize(clinfo)) {
		std::cerr << "Error initializing primary ray generator." << std::endl;
		exit(1);
	}
	std::cout << "Initialized primary ray generator succesfully." << std::endl;


	/* ------------------ Initialize Secondary Ray Generator ----------------------*/
	if (!sec_ray_gen.initialize(clinfo)) {
		std::cerr << "Error initializing secondary ray generator." << std::endl;
		exit(1);
	}
	sec_ray_gen.set_max_rays(ray_bundle_1.count());
	std::cout << "Initialized secondary ray generator succesfully." << std::endl;

	/*------------------------ Initialize RayShader ---------------------------*/
	if (!ray_shader.initialize(clinfo, scene_info, framebuffer, cl_hit_mem)) {
		std::cerr << "Error initializing ray shader." << std::endl;
		exit(1);
	}
	std::cout << "Initialized ray shader succesfully." << std::endl;

	/*---------------------- Print scene data ----------------------*/
	std::cerr << "\nScene stats: " << std::endl;
	std::cerr << "\tTriangle count: " << scene_mesh.triangleCount() << std::endl;
	std::cerr << "\tVertex count: " << scene_mesh.vertexCount() << std::endl;

	/*------------------------- Count mem usage -----------------------------------*/
	int32_t total_cl_mem = 0;
	total_cl_mem += pixel_count * 4; /* 4bpp texture */
	total_cl_mem += scene_info.size();
	total_cl_mem += cl_mem_size(ray_bundle_1.mem()) + cl_mem_size(ray_bundle_2.mem());
	total_cl_mem += cl_mem_size(cl_hit_mem);
	total_cl_mem += cl_mem_size(cubemap.positive_x_mem()) * 6;
	total_cl_mem += cl_mem_size(framebuffer.image_mem());

	std::cout << "\nMemory stats: " << std::endl;
	std::cout << "\tTotal opencl mem usage: " 
		  << total_cl_mem << " bytes." << std::endl;
	std::cout << "\tScene mem usage: " << scene_info.size() << " bytes." << std::endl;
	std::cout << "\tFramebuffer+Tex mem usage: " 
		  << cl_mem_size(framebuffer.image_mem()) + pixel_count * 4
		  << " bytes."<< std::endl;
	std::cout << "\tCubemap mem usage: " << cl_mem_size(cubemap.positive_x_mem()) * 6 
		  << " bytes."<< std::endl;
	std::cout << "\tRay info mem usage: " 
		  << cl_mem_size(ray_bundle_1.mem())*2 + cl_mem_size(cl_hit_mem)
		  << " bytes."<< std::endl;


        /* !! ---------------------- Test area ---------------- */
	std::cerr << std::endl;
	std::cerr << "Misc info: " << std::endl;

	std::cerr << "Tile size: " << best_tile_size << std::endl;
	std::cerr << "Tiles: " << pixel_count / (float)best_tile_size << std::endl;
	std::cerr << "color_cl size: "
		  << sizeof(color_cl)
		  << std::endl;

	std::cerr << "ray_cl size: "
		  << sizeof(ray_cl)
		  << std::endl;
	std::cerr << "ray_plus_cl size: "
		  << sizeof(ray_plus_cl)
		  << std::endl;

	/*------------------------ Set GLUT and misc functions -----------------------*/
	rt_time.snap_time();

	glutKeyboardFunc(gl_key);
	glutMotionFunc(gl_mouse);
	glutDisplayFunc(gl_loop);
	glutIdleFunc(gl_loop);
	glutMainLoop();	

	return 0;
}

