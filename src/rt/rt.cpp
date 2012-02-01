#include <algorithm>
#include <iostream>

#include <cl-gl/opengl-init.hpp>
#include <cl-gl/opencl-init.hpp>
#include <rt/math.hpp>
#include <rt/ray.hpp>
#include <rt/timing.hpp>
#include <rt/camera.hpp>
#include <rt/primary-ray-generator.hpp>
#include <rt/secondary-ray-generator.hpp>
#include <rt/cubemap.hpp>
#include <rt/framebuffer.hpp>
#include <rt/tracer.hpp>
#include <rt/scene.hpp>
#include <rt/obj-loader.hpp>
#include <rt/scene.hpp>
#include <rt/bvh.hpp>
#include <rt/cl_aux.hpp>

#define MAX_BOUNCE 3

cl_mem cl_tex_mem;
cl_mem cl_hit_mem;

RayBundle ray_bundle_1,ray_bundle_2;
PrimaryRayGenerator prim_ray_gen;
SecondaryRayGenerator sec_ray_gen;
SceneInfo scene_info;
Cubemap cubemap;
FrameBuffer framebuffer;
Tracer tracer;
Camera camera;

CLInfo* cli;
GLuint gl_tex;
rt_time_t rt_time;
uint32_t window_size[] = {512, 512};

cl_int max_secondary_ray_count = window_size[0] * window_size[1]*4;
int pixel_count = window_size[0] * window_size[1];


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

	int tile_size = cli->max_compute_units * cli->max_work_item_sizes[0];
	tile_size *= 32;

	// std::cerr << "Tiles in screen: " << ((float)pixel_count)/tile_size << std::endl;

	glClear(GL_COLOR_BUFFER_BIT);

	if (!framebuffer.clear()) {
		std::cerr << "Failed to clear framebuffer." << std::endl;
		exit(1);
	}

	cl_int arg = i%STEPS;

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

		ray_count += tile_size;
		tracer.trace(scene_info, tile_size, *ray_in, cl_hit_mem);
		tracer.shadow_trace(scene_info, tile_size, *ray_in, cl_hit_mem, arg);

		if (!framebuffer.update(*ray_in, cubemap, tile_size, arg)){
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

			ray_count += secondary_ray_count;
			tracer.trace(scene_info, secondary_ray_count, 
				     *ray_in, cl_hit_mem);
			tracer.shadow_trace(scene_info, secondary_ray_count, 
					    *ray_in, cl_hit_mem, arg);

			if (!framebuffer.update(*ray_in, cubemap, 
						secondary_ray_count, arg)){
				std::cerr << "Failed to update framebuffer." << std::endl;
				exit(1);
			}

			sec_ray_gen.generate(scene_info, *ray_in, secondary_ray_count,
					     cl_hit_mem, *ray_out,&secondary_ray_count);

		}

	}

	if (!framebuffer.copy(cl_tex_mem)){
		std::cerr << "Failed to copy framebuffer." << std::endl;
		exit(1);
	}

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
	cli = &clinfo;

	/*---------------------- Create shared GL-CL texture ----------------------*/
	gl_tex = create_tex_gl(window_size[0],window_size[1]);
	print_gl_tex_2d_info(gl_tex);
	if (create_cl_mem_from_gl_tex(clinfo, gl_tex, &cl_tex_mem))
		exit(1);
	print_cl_image_2d_info(cl_tex_mem);


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
	std::cout << "Initialized secondary ray generator succesfully." << std::endl;


	/* !! ---------------------- Test area ---------------- */
	std::cerr << "color_cl size: "
		  << sizeof(color_cl)
		  << std::endl;

	std::cerr << "ray_cl size: "
		  << sizeof(ray_cl)
		  << std::endl;

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

	// mesh_id teapot_mesh_id = scene.load_obj_file("models/obj/teapot2.obj");
	// mesh_id teapot_mesh_id = scene.load_obj_file("models/obj/teapot-low_res.obj");
	// object_id teapot_obj_id = scene.geometry.add_object(teapot_mesh_id);
	// Object& teapot_obj = scene.geometry.object(teapot_obj_id);
	// teapot_obj.geom.setPos(makeVector(-8.f,-5.f,0.f));
	// teapot_obj.mat.diffuse = Green;
	// teapot_obj.mat.shininess = 1.f;
	// teapot_obj.mat.reflectiveness = 0.5f;

	// object_id teapot_obj_id_2 = scene.geometry.add_object(teapot_mesh_id);
	// Object& teapot_obj_2 = scene.geometry.object(teapot_obj_id_2);
	// teapot_obj_2.mat.diffuse = Red;
	// teapot_obj_2.mat.shininess = 1.f;
	// teapot_obj_2.geom.setPos(makeVector(8.f,5.f,0.f));
	// teapot_obj_2.geom.setRpy(makeVector(0.2f,0.1f,0.3f));
	// teapot_obj_2.geom.setScale(0.3f);
	// teapot_obj_2.mat.reflectiveness = 0.5f;

	mesh_id boat_mesh_id = scene.load_obj_file("models/obj/frame_boat1.obj");
	object_id boat_obj_id = scene.geometry.add_object(boat_mesh_id);
	Object& boat_obj = scene.geometry.object(boat_obj_id);
	boat_obj.geom.setPos(makeVector(0.f,-18.f,0.f));
	boat_obj.mat.diffuse = Red;
	boat_obj.mat.shininess = 1.f;
	boat_obj.mat.reflectiveness = 0.0f;

	scene.create_aggregate();
	Mesh& scene_mesh = scene.get_aggregate_mesh();

	std::cout << "Building BVH." << std::endl;
	rt_time.snap_time();
	scene.create_bvh();
	BVH& scene_bvh   = scene.get_aggregate_bvh ();
	double bvh_build_time = rt_time.msec_since_snap();
	std::cout << "Built BVH (build time: " 
		  << bvh_build_time << " msec, " 
		  << scene_bvh.nodeArraySize() << " nodes)" << std::endl;

	/*---------------------- Print scene data ----------------------*/
	std::cerr << "Triangle count: " << scene_mesh.triangleCount() << std::endl;
	std::cerr << "Vertex count: " << scene_mesh.vertexCount() << std::endl;

	/*---------------------- Initialize SceneInfo ----------------------------*/
	if (!scene_info.initialize(scene,clinfo)) {
		std::cerr << "Failed to initialize scene info." << std::endl;
		exit(1);
	}
	
	std::cout << "Succesfully initialized scene info." << std::endl;

	/*---------------------- Set initial Camera paramaters -----------------------*/
	camera.set(makeVector(0,3,-30), makeVector(0,0,1), makeVector(0,1,0), M_PI/4.,
		   window_size[0] / (float)window_size[1]);


	/*------------- Initialize device memory for primary hit info ----------------*/
	uint32_t hit_mem_size = sizeof(RayHitInfo) * window_size[0] * window_size[1];
	hit_mem_size *= 4; /* To reuse in order to compute reflection/refraction */


	if (create_empty_cl_mem(clinfo, 
				CL_MEM_READ_WRITE,
				hit_mem_size,
				&cl_hit_mem))
		exit(1);
	std::cerr << "Primary rays hit buffer info:" << std::endl;
	print_cl_mem_info(cl_hit_mem);

	/*---------------------- Initialize ray bundles -----------------------------*/

	if (!ray_bundle_1.initialize(pixel_count*4, clinfo)) {
		std::cerr << "Error initializing ray bundle 1" << std::endl;
		std::cerr.flush();
		exit(1);
	}

	if (!ray_bundle_2.initialize(pixel_count*4, clinfo)) {
		std::cerr << "Error initializing ray bundle 2" << std::endl;
		std::cerr.flush();
		exit(1);
	}

	std::cout << "Initialized ray bundles succesfully" << std::endl;

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
	if (!framebuffer.initialize(clinfo, 
				    window_size, 
				    scene_info, 
				    cl_hit_mem)) {
		std::cerr << "Error initializing framebuffer." << std::endl;
		exit(1);
	}

	std::cerr << "Initialized framebuffer succesfully." << std::endl;


	/*------------------------ Set GLUT and misc functions -----------------------*/
	rt_time.snap_time();

	glutKeyboardFunc(gl_key);
	glutMotionFunc(gl_mouse);
	glutDisplayFunc(gl_loop);
	glutIdleFunc(gl_loop);
	glutMainLoop();	

	return 0;
}

