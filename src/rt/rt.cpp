#include <iostream>

#include <cl-gl/opengl-init.hpp>
#include <cl-gl/opencl-init.hpp>
#include <rt/math.hpp>
#include <rt/ray.hpp>
#include <rt/timing.hpp>
#include <rt/primary-ray-generator.hpp>
#include <rt/secondary-ray.hpp>
#include <rt/scene.hpp>
#include <rt/obj-loader.hpp>
#include <rt/scene.hpp>
#include <rt/bvh.hpp>
#include <rt/cl_aux.hpp>

#define MAX_BOUNCE 5

CLKernelInfo rt_clkernelinfo;
CLKernelInfo bounce_clkernelinfo;

CLKernelInfo shadow_clkernelinfo;

CLKernelInfo cm_clkernelinfo;
CLKernelInfo second_cm_clkernelinfo;

CLKernelInfo ray_clkernelinfo;

cl_mem cl_ray_mem;
cl_mem cl_vert_mem;
cl_mem cl_index_mem;
cl_mem cl_mat_list_mem;
cl_mem cl_mat_map_mem;
cl_mem cl_image_mem;
cl_mem cl_hit_mem;

cl_mem cl_ray_level[MAX_BOUNCE];
cl_mem cl_bounce_mem[MAX_BOUNCE];

GLuint gl_tex;
rt_time_t rt_time;
uint32_t window_size[] = {800, 600};
RayBundle primary_ray_bundle(window_size[0]*window_size[1]);

Camera rt_cam;

#define STEPS 16

int32_t setRays(const CLKernelInfo& clkernelinfo);


void gl_mouse(int x, int y)
{
	float delta = 0.001f;
	float d_inc = delta * (window_size[1]*0.5f - y);/* y axis points downwards */
	float d_yaw = delta * (x - window_size[0]*0.5f);

	d_inc = std::min(std::max(d_inc, -0.01f), 0.01f);
	d_yaw = std::min(std::max(d_yaw, -0.01f), 0.01f);

	if (d_inc == 0.f && d_yaw == 0.f)
		return;

	rt_cam.modifyAbsYaw(d_yaw);
	rt_cam.modifyPitch(d_inc);
	glutWarpPointer(window_size[0] * 0.5f, window_size[1] * 0.5f);
}

void gl_key(unsigned char key, int x, int y)
{
	float delta = 2.f;

	switch (key){
	case 'a':
		rt_cam.panRight(-delta);
		break;
	case 's':
		rt_cam.panForward(-delta);
		break;
	case 'w':
		rt_cam.panForward(delta);
		break;
	case 'd':
		rt_cam.panRight(delta);
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
	cl_int err;
	
	glClear(GL_COLOR_BUFFER_BIT);

	cl_int arg = i%STEPS;

	// Set div argument
	err = clSetKernelArg(shadow_clkernelinfo.kernel,10,
			     sizeof(cl_int),&arg);
	if (error_cl(err, "clSetKernelArg 10"))
		exit(1);

	/* Set rays if camera has moved */
	if (setRays(ray_clkernelinfo)){
		std::cerr << "Error seting ray cl_mem object" << std::endl;
		exit(1);
	}

	cl_time.snap_time();

	execute_cl(rt_clkernelinfo);
	execute_cl(shadow_clkernelinfo);
	execute_cl(cm_clkernelinfo);

	// for (uint32_t i = 0; i < MAX_BOUNCE; ++i) {

	for (uint32_t i = 0; i < 1; ++i) {

		err = clSetKernelArg(second_cm_clkernelinfo.kernel,1,
				     sizeof(cl_mem),&cl_ray_level[i]);
		if (error_cl(err, "clSetKernelArg 1"))
			exit(1);

		err = clSetKernelArg(second_cm_clkernelinfo.kernel,2,
				     sizeof(cl_mem),&cl_bounce_mem[i]);
		if (error_cl(err, "clSetKernelArg 2"))
			exit(1);

		err = clSetKernelArg(bounce_clkernelinfo.kernel,0,
				     sizeof(cl_mem),&cl_bounce_mem[i]);
		if (error_cl(err, "clSetKernelArg 0"))
			exit(1);

		execute_cl(bounce_clkernelinfo);
		execute_cl(second_cm_clkernelinfo);
		execute_cl(shadow_clkernelinfo);
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
			  << "\t(" 
			  << int(STEPS / (msec/1000))
			  << " FPS)          \r" ;
		std::flush(std::cout);
		rt_time.snap_time();
	}		
	glutSwapBuffers();
}

int main (int argc, char** argv)
{

	CLInfo clinfo;
	GLInfo glinfo;
	cl_int err;
	
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
	print_gl_tex_2d_info(gl_tex);
	if (create_cl_mem_from_gl_tex(clinfo, gl_tex, &cl_image_mem))
		exit(1);
	print_cl_image_2d_info(cl_image_mem);


	/* ------------------ Initialize primary trace opencl kernel -----------------*/

	if (init_cl_kernel(&clinfo,
			   "src/kernel/trace.cl", 
			   "trace", 
			   &rt_clkernelinfo)){
		std::cerr << "Failed to initialize CL kernel" << std::endl;
		exit(1);
	} else {
		std::cerr << "Initialized primary trace CL kernel succesfully" << std::endl;
	}
	rt_clkernelinfo.work_dim = 2;
	rt_clkernelinfo.arg_count = 8;
	rt_clkernelinfo.global_work_size[0] = window_size[0];
	rt_clkernelinfo.global_work_size[1] = window_size[1];

	/* ------------------ Initialize primary shadow ray opencl kernel --------------*/

	if (init_cl_kernel(&clinfo,
			   "src/kernel/trace_any.cl", 
			   "trace_any", 
			   &shadow_clkernelinfo)){
		std::cerr << "Failed to initialize CL kernel" << std::endl;
		exit(1);
	} else {
		std::cerr << "Initialized primary shadow CL kernel succesfully" 
			  << std::endl;
	}
	shadow_clkernelinfo.work_dim = 2;
	shadow_clkernelinfo.arg_count = 8;
	shadow_clkernelinfo.global_work_size[0] = window_size[0];
	shadow_clkernelinfo.global_work_size[1] = window_size[1];

	/* ------------------ Initialize secondary trace opencl kernel -----------------*/

	if (init_cl_kernel(&clinfo,
			   "src/kernel/trace_bounce.cl", 
			   "trace", 
			   &bounce_clkernelinfo)){
		std::cerr << "Failed to initialize CL kernel" << std::endl;
		exit(1);
	} else {
		std::cerr << "Initialized secondary trace CL kernel succesfully" 
			  << std::endl;
	}
	bounce_clkernelinfo.work_dim = 2;
	bounce_clkernelinfo.arg_count = 6;
	bounce_clkernelinfo.global_work_size[0] = window_size[0];
	bounce_clkernelinfo.global_work_size[1] = window_size[1];

	/* ------------------ Initialize ray opencl kernel ----------------------*/

	if (init_cl_kernel(&clinfo,"src/kernel/ray.cl", "create_ray", &ray_clkernelinfo)){
		std::cerr << "Failed to initialize CL kernel" << std::endl;
		exit(1);
	} else {
		std::cerr << "Initialized ray CL kernel succesfully" << std::endl;
	}
	ray_clkernelinfo.work_dim = 2;
	ray_clkernelinfo.arg_count = 5;
	ray_clkernelinfo.global_work_size[0] = window_size[0];
	ray_clkernelinfo.global_work_size[1] = window_size[1];

	/* !! ---------------------- Test area ---------------- */
	std::cerr << "Screen info size: "
		  << screen_shading_info::size_in_bytes(800,600,5) / 1e6 
		  << " MB"
		  << std::endl;
	std::cerr << "color_cl size: "
		  << sizeof(color_cl)
		  << std::endl;

	std::cerr << "ray_level_cl size: "
		  << sizeof(ray_level_cl)
		  << std::endl;

	std::cerr << "cl_char: "
		  << sizeof(cl_char)
		  << std::endl;

	std::cerr << "cl_int size: "
		  << sizeof(cl_int)
		  << std::endl;

	std::cerr << "bounce_cl size: "
		  << sizeof(bounce_cl)
		  << std::endl;

	std::cerr << "ray_cl size: "
		  << sizeof(ray_cl)
		  << std::endl;

	/*---------------------- Set up scene ---------------------------*/
	Scene scene;
	mesh_id teapot_mesh_id = scene.load_obj_file("models/obj/teapot2.obj");
	// mesh_id teapot_mesh_id = scene.load_obj_file("models/obj/teapot-low_res.obj");
	// mesh_id floor_mesh_id = scene.load_obj_file("models/obj/floor.obj");
	mesh_id floor_mesh_id = scene.load_obj_file("models/obj/frame_water1.obj");
	// mesh_id boat_mesh_id = scene.load_obj_file("models/obj/frame_boat1.obj");

	/* Other models 
	models/obj/floor.obj
	models/obj/teapot-low_res.obj
	models/obj/teapot.obj
	models/obj/teapot2.obj
	models/obj/frame_boat1.obj
	models/obj/frame_others1.obj
	models/obj/frame_water1.obj
	 */

	object_id teapot_obj_id = scene.geometry.add_object(teapot_mesh_id);
	object_id teapot_obj_id_2 = scene.geometry.add_object(teapot_mesh_id);

	object_id floor_obj_id  = scene.geometry.add_object(floor_mesh_id);
	Object& floor_obj = scene.geometry.object(floor_obj_id);
	floor_obj.geom.setScale(2.f);
	floor_obj.geom.setPos(makeVector(0.f,-8.f,0.f));
	floor_obj.mat.diffuse = Blue;
	floor_obj.mat.reflectiveness = 1.f;

	Object& teapot_obj = scene.geometry.object(teapot_obj_id);
	teapot_obj.geom.setPos(makeVector(-8.f,-5.f,0.f));
	teapot_obj.mat.diffuse = Green;
	teapot_obj.mat.shininess = 1.f;
	teapot_obj.mat.reflectiveness = 1.f;

	Object& teapot_obj_2 = scene.geometry.object(teapot_obj_id_2);
	teapot_obj_2.mat.diffuse = Red;
	teapot_obj_2.mat.shininess = 1.f;
	teapot_obj_2.geom.setPos(makeVector(8.f,5.f,0.f));
	teapot_obj_2.geom.setRpy(makeVector(0.2f,0.1f,0.3f));
	teapot_obj_2.geom.setScale(0.3f);
	teapot_obj_2.mat.reflectiveness = 1.f;

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
	int triangles = scene_mesh.triangleCount();
	std::cerr << "Triangle count: " << triangles << std::endl;
	
	int vertices = scene_mesh.vertexCount();
	std::cerr << "Vertex count: " << vertices << std::endl;



	/*---------------------- Move model data to OpenCL device -----------------*/
	if (create_filled_cl_mem(clinfo,CL_MEM_READ_ONLY,
				 vertices * sizeof(Vertex),
				 scene_mesh.vertexArray(),
				 &cl_vert_mem))
		exit(1);
	std::cerr << "Vertex buffer info:" << std::endl;
	print_cl_mem_info(cl_vert_mem);



	if (create_filled_cl_mem(clinfo,CL_MEM_READ_ONLY,
				 triangles * 3 * sizeof(uint32_t),
				 scene_mesh.triangleArray(),
				 &cl_index_mem))
		exit(1);
	std::cout << "Index buffer info:" << std::endl;
	print_cl_mem_info(cl_index_mem);

	/*---------------------- Move material data to OpenCL device ------------*/

	std::vector<material_cl>& mat_list = scene.get_material_list();
	std::vector<cl_int>& mat_map = scene.get_material_map();
	
	void* mat_list_ptr = &(mat_list[0]);
	void* mat_map_ptr  = &(mat_map[0]);

	size_t mat_list_size = sizeof(material_cl) * mat_list.size();
	size_t mat_map_size  = sizeof(cl_int)      * mat_map.size();

	if (create_filled_cl_mem(clinfo,CL_MEM_READ_ONLY,
				 mat_list_size,
				 mat_list_ptr,
				 &cl_mat_list_mem))
		exit(1);
	std::cout << "Material list buffer info:" << std::endl;
	print_cl_mem_info(cl_mat_list_mem);

	if (create_filled_cl_mem(clinfo,CL_MEM_READ_ONLY,
				 mat_map_size,
				 mat_map_ptr,
				 &cl_mat_map_mem))
		exit(1);
	std::cout << "Material map buffer info:" << std::endl;
	print_cl_mem_info(cl_mat_map_mem);

	/*--------------------- Move bvh to device memory ---------------------*/
	cl_mem cl_bvh_nodes;
	std::cout << "Moving bvh nodes to device memory." << std::endl;
	if (create_filled_cl_mem(clinfo,CL_MEM_READ_ONLY,
				 scene_bvh.nodeArraySize() * sizeof(BVHNode),
				 scene_bvh.nodeArray(),
				 &cl_bvh_nodes))
		exit(1);

	/*---------------------- Set initial Camera paramaters -----------------------*/
	rt_cam.set(makeVector(0,3,-30), makeVector(0,0,1), makeVector(0,1,0), M_PI/4.,
		   window_size[0] / (float)window_size[1]);


#if 0
	/*---------------------- Initialize primary ray bundle -----------------------*/
	if (!primary_ray_bundle.initialize()){
		std::cout << "Failed to initialize bundle";
		return 1;
	}
#endif

	/*------------- Initialize device memory for primary hit info ----------------*/
	uint32_t hit_mem_size = sizeof(RayHitInfo) * window_size[0] * window_size[1];
	hit_mem_size *= 2; /* To reuse in order to compute reflection/refraction */


	if (create_empty_cl_mem(clinfo, 
				CL_MEM_READ_WRITE,
				hit_mem_size,
				&cl_hit_mem))
		exit(1);
	std::cerr << "Primary rays hit buffer info:" << std::endl;
	print_cl_mem_info(cl_hit_mem);

	/*---------------------- Initialize device memory for rays -------------------*/
	uint32_t ray_mem_size = 
		RayBundle::expected_size_in_bytes(window_size[0]*window_size[1]);
	ray_mem_size *= 2; /*We double it to store reflection/refraction rays*/
	
	std::cerr << "Ray buffer info:" << std::endl;
	if (create_empty_cl_mem(clinfo, 
				CL_MEM_READ_WRITE,
				ray_mem_size,
				&cl_ray_mem))
		exit(1);
	std::cerr << "Ray buffer info:" << std::endl;
	print_cl_mem_info(cl_ray_mem);

	/*---------------------- Initialize device memory ray bounce array ------------*/
	uint32_t bounce_mem_size = 
		ray_bounce_info::size_in_bytes(window_size[0],
					       window_size[1], 
					       1);
	for (uint32_t i = 0; i < MAX_BOUNCE; ++i) {

		if (create_empty_cl_mem(clinfo, 
					CL_MEM_READ_WRITE,
					bounce_mem_size,
					&cl_bounce_mem[i]))
			exit(1);
	}

	std::cerr << "Bounce mem info: size " << bounce_mem_size << std::endl;
	print_cl_mem_info(cl_bounce_mem[0]);
	

	/*---------------------- Initialize device memory ray level array ------------*/
	uint32_t ray_level_mem_size = 
		screen_shading_info::size_in_bytes(window_size[0],
						   window_size[1], 
						   1);
	for (uint32_t i = 0; i < MAX_BOUNCE; ++i) {

		if (create_empty_cl_mem(clinfo, 
					CL_MEM_READ_WRITE,
					ray_level_mem_size,
					&cl_ray_level[i]))
			exit(1);
	}

	std::cerr << "Ray trace level mem info: " << ray_level_mem_size << std::endl;
	print_cl_mem_info(cl_ray_level[0]);

	/*----------------------- Set primary trace kernel arguments -----------------*/

	std::cout << "Setting rays hit mem object argument for trace kernel" << std::endl;
	err = clSetKernelArg(rt_clkernelinfo.kernel,0,
			     sizeof(cl_mem),&cl_hit_mem);
	if (error_cl(err, "clSetKernelArg 0"))
		exit(1);

	std::cout << "Setting ray mem object argument for trace kernel" << std::endl;
	err = clSetKernelArg(rt_clkernelinfo.kernel,1,
			     sizeof(cl_mem),&cl_ray_mem);
	if (error_cl(err, "clSetKernelArg 1"))
		exit(1);

	std::cout << "Setting vertex mem object argument for trace kernel" << std::endl;
	err = clSetKernelArg(rt_clkernelinfo.kernel,2,sizeof(cl_mem),&cl_vert_mem);
	if (error_cl(err, "clSetKernelArg 2"))
		exit(1);

	std::cout << "Setting index mem object argument for trace kernel" << std::endl;
	err = clSetKernelArg(rt_clkernelinfo.kernel,3,sizeof(cl_mem),&cl_index_mem);
	if (error_cl(err, "clSetKernelArg 3"))
		exit(1);

	std::cout << "Setting bvh node array  argument for trace kernel" << std::endl;
	err = clSetKernelArg(rt_clkernelinfo.kernel,4,sizeof(cl_mem),&cl_bvh_nodes);
	if (error_cl(err, "clSetKernelArg 4"))
		exit(1);

	/*----------------------- Set primary shadow kernel arguments -----------------*/

	std::cout << "Setting texture mem object argument for shadow kernel" << std::endl;
	err = clSetKernelArg(shadow_clkernelinfo.kernel,0,sizeof(cl_mem),&cl_image_mem);
	if (error_cl(err, "clSetKernelArg 0"))
		exit(1);

	std::cout << "Setting rays hit mem object argument for shadow kernel" << std::endl;
	err = clSetKernelArg(shadow_clkernelinfo.kernel,1,
			     sizeof(cl_mem),&cl_hit_mem);
	if (error_cl(err, "clSetKernelArg 1"))
		exit(1);

	std::cout << "Setting ray mem object argument for shadow kernel" << std::endl;
	err = clSetKernelArg(shadow_clkernelinfo.kernel,2,
			     sizeof(cl_mem),&cl_ray_mem);
	if (error_cl(err, "clSetKernelArg 2"))
		exit(1);

	std::cout << "Setting vertex mem object argument for shadow kernel" << std::endl;
	err = clSetKernelArg(shadow_clkernelinfo.kernel,3,sizeof(cl_mem),&cl_vert_mem);
	if (error_cl(err, "clSetKernelArg 3"))
		exit(1);

	std::cout << "Setting index mem object argument for shadow kernel" << std::endl;
	err = clSetKernelArg(shadow_clkernelinfo.kernel,4,sizeof(cl_mem),&cl_index_mem);
	if (error_cl(err, "clSetKernelArg 4"))
		exit(1);

	std::cout << "Setting bvh node array  argument for shadow kernel" << std::endl;
	err = clSetKernelArg(shadow_clkernelinfo.kernel,5,sizeof(cl_mem),&cl_bvh_nodes);
	if (error_cl(err, "clSetKernelArg 5"))
		exit(1);

	std::cout << "Setting material list array argument for shadow kernel" << std::endl;
	err = clSetKernelArg(shadow_clkernelinfo.kernel,6,sizeof(cl_mem),&cl_mat_list_mem);
	if (error_cl(err, "clSetKernelArg 6"))
		exit(1);

	std::cout << "Setting material map array argument for shadow kernel" << std::endl;
	err = clSetKernelArg(shadow_clkernelinfo.kernel,7,sizeof(cl_mem),&cl_mat_map_mem);
	if (error_cl(err, "clSetKernelArg 7"))
		exit(1);

	std::cout << "Setting ray level array shadow kernel" << std::endl;
	err= clSetKernelArg(shadow_clkernelinfo.kernel,8,sizeof(cl_mem),&cl_ray_level[0]);
	if (error_cl(err, "clSetKernelArg 8"))
		exit(1);

	std::cout << "Setting ray bounce array for shadow kernel" << std::endl;
	err = clSetKernelArg(shadow_clkernelinfo.kernel,9,sizeof(cl_mem),&cl_bounce_mem[0]);
	if (error_cl(err, "clSetKernelArg 9"))
		exit(1);

	/*----------------------- Set secondary trace kernel arguments -----------------*/

	std::cout << "Setting rays hit mem object argument for secondary trace kernel" 
		  << std::endl;
	err = clSetKernelArg(bounce_clkernelinfo.kernel,1,
			     sizeof(cl_mem),&cl_hit_mem);
	if (error_cl(err, "clSetKernelArg 1"))
		exit(1);

	std::cout << "Setting vertex mem object argument for secondary trace kernel" 
		  << std::endl;
	err = clSetKernelArg(bounce_clkernelinfo.kernel,2,
			     sizeof(cl_mem),&cl_vert_mem);
	if (error_cl(err, "clSetKernelArg 2"))
		exit(1);

	std::cout << "Setting index mem object argument for secondary trace kernel" 
		  << std::endl;
	err = clSetKernelArg(bounce_clkernelinfo.kernel,3,
			     sizeof(cl_mem),&cl_index_mem);
	if (error_cl(err, "clSetKernelArg 3"))
		exit(1);

	std::cout << "Setting bvh node array  argument for secondary trace kernel" 
		  << std::endl;
	err = clSetKernelArg(bounce_clkernelinfo.kernel,4,
			     sizeof(cl_mem),&cl_bvh_nodes);
	if (error_cl(err, "clSetKernelArg 4"))
		exit(1);

	std::cout << "Setting ray mem object argument for secondary trace kernel" 
		  << std::endl;
	err = clSetKernelArg(bounce_clkernelinfo.kernel,5,
			     sizeof(cl_mem),&cl_ray_mem);
	if (error_cl(err, "clSetKernelArg 5"))
		exit(1);

	/*------------------------ Set up cubemap kernel info ---------------------*/

	if (init_cl_kernel(&clinfo,"src/kernel/cubemap.cl", "cubemap", 
			   &cm_clkernelinfo)){
		std::cerr << "Failed to initialize CL kernel" << std::endl;
		exit(1);
	} else {
		std::cerr << "Initialized cubemap CL kernel succesfully" << std::endl;
	}
	cm_clkernelinfo.work_dim = 2;
	cm_clkernelinfo.arg_count = 9;
	cm_clkernelinfo.global_work_size[0] = window_size[0];
	cm_clkernelinfo.global_work_size[1] = window_size[1];
	
	/*----------------------- Load cubemap textures ---------------------------*/

	GLuint gl_posx_tex,gl_negx_tex;
	GLuint gl_posy_tex,gl_negy_tex;
	GLuint gl_posz_tex,gl_negz_tex;

	cl_mem cl_cm_posx_mem,cl_cm_negx_mem;
	cl_mem cl_cm_posy_mem,cl_cm_negy_mem;
	cl_mem cl_cm_posz_mem,cl_cm_negz_mem;
	cl_cm_negz_mem = cl_cm_posz_mem;

	uint32_t tex_width,tex_height;
	gl_posx_tex = create_tex_gl_from_jpeg(tex_width,tex_height,
					      "textures/cubemap/Path/posx.jpg");
	gl_negx_tex = create_tex_gl_from_jpeg(tex_width,tex_height,
					      "textures/cubemap/Path/negx.jpg");
	gl_posy_tex = create_tex_gl_from_jpeg(tex_width,tex_height,
					      "textures/cubemap/Path/posy.jpg");
	gl_negy_tex = create_tex_gl_from_jpeg(tex_width,tex_height,
					      "textures/cubemap/Path/negy.jpg");
	gl_posz_tex = create_tex_gl_from_jpeg(tex_width,tex_height,
					      "textures/cubemap/Path/posz.jpg");
	gl_negz_tex = create_tex_gl_from_jpeg(tex_width,tex_height,
					      "textures/cubemap/Path/negz.jpg");

	std::cerr << "width: " << tex_width << "\theight: " << tex_height << std::endl;

	if (gl_posx_tex < 0 || gl_negx_tex < 0 || 
	    gl_posy_tex < 0 || gl_negy_tex < 0 || 
	    gl_posz_tex < 0 || gl_negz_tex < 0) {
		std::cerr << "Error loading cubmap textures." << std::endl;
		exit(1);
	}

	if (create_cl_mem_from_gl_tex(clinfo, gl_posx_tex, &cl_cm_posx_mem))
		exit(1);
	if (create_cl_mem_from_gl_tex(clinfo, gl_negx_tex, &cl_cm_negx_mem))
		exit(1);
	if (create_cl_mem_from_gl_tex(clinfo, gl_posy_tex, &cl_cm_posy_mem))
		exit(1);
	if (create_cl_mem_from_gl_tex(clinfo, gl_negy_tex, &cl_cm_negy_mem))
		exit(1);
	if (create_cl_mem_from_gl_tex(clinfo, gl_posz_tex, &cl_cm_posz_mem))
		exit(1);
	if (create_cl_mem_from_gl_tex(clinfo, gl_negz_tex, &cl_cm_negz_mem))
		exit(1);
	

	/* ------------------- Set up secondary cubmeap kernel ------------------- */
	if (init_cl_kernel(&clinfo,"src/kernel/secondary_cubemap.cl", "cubemap", 
			   &second_cm_clkernelinfo)){
		std::cerr << "Failed to initialize CL kernel" << std::endl;
		exit(1);
	} else {
		std::cerr << "Initialized secondary cubemap CL kernel succesfully" 
			  << std::endl;
	}
	second_cm_clkernelinfo.work_dim = 2;
	second_cm_clkernelinfo.arg_count = 10;
	second_cm_clkernelinfo.global_work_size[0] = window_size[0];
	second_cm_clkernelinfo.global_work_size[1] = window_size[1];

	/*------------------------ Set up cubemap kernel arguments -------------------*/

	std::cout << "Setting output mem object argument for cubemap kernel" << std::endl;
	err = clSetKernelArg(cm_clkernelinfo.kernel,0,sizeof(cl_mem),&cl_image_mem);
	if (error_cl(err, "clSetKernelArg 0"))
		exit(1);

	std::cout << "Setting rays hit mem object argument for cubemap kernel" << std::endl;
	err = clSetKernelArg(cm_clkernelinfo.kernel,1,
			     sizeof(cl_mem),&cl_hit_mem);
	if (error_cl(err, "clSetKernelArg 1"))
		exit(1);

	std::cout << "Setting ray mem object argument for cubemap kernel" << std::endl;
	err = clSetKernelArg(cm_clkernelinfo.kernel,2,
			     sizeof(cl_mem),&cl_ray_mem);
	if (error_cl(err, "clSetKernelArg 2"))
		exit(1);

	std::cout << "Setting cubemap positive x texture for cubemap kernel" << std::endl;
	err = clSetKernelArg(cm_clkernelinfo.kernel,3,
			     sizeof(cl_mem),&cl_cm_posx_mem);
	if (error_cl(err, "clSetKernelArg 3"))
		exit(1);

	std::cout << "Setting cubemap negative x texture for cubemap kernel" << std::endl;
	err = clSetKernelArg(cm_clkernelinfo.kernel,4,
			     sizeof(cl_mem),&cl_cm_negx_mem);
	if (error_cl(err, "clSetKernelArg 4"))
		exit(1);

	std::cout << "Setting cubemap positive y texture for cubemap kernel" << std::endl;
	err = clSetKernelArg(cm_clkernelinfo.kernel,5,
			     sizeof(cl_mem),&cl_cm_posy_mem);
	if (error_cl(err, "clSetKernelArg 5"))
		exit(1);

	std::cout << "Setting cubemap negative y texture for cubemap kernel" << std::endl;
	err = clSetKernelArg(cm_clkernelinfo.kernel,6,
			     sizeof(cl_mem),&cl_cm_negy_mem);
	if (error_cl(err, "clSetKernelArg 6"))
		exit(1);

	std::cout << "Setting cubemap positive z texture for cubemap kernel" << std::endl;
	err = clSetKernelArg(cm_clkernelinfo.kernel,7,
			     sizeof(cl_mem),&cl_cm_posz_mem);
	if (error_cl(err, "clSetKernelArg 7"))
		exit(1);

	std::cout << "Setting cubemap negative z texture for cubemap kernel" << std::endl;
	err = clSetKernelArg(cm_clkernelinfo.kernel,8,
			     sizeof(cl_mem),&cl_cm_negz_mem);
	if (error_cl(err, "clSetKernelArg 8"))
		exit(1);

	/*------------------------ Set secondary cubemap kernel arguments -------------*/

	std::cout << "Setting output mem object argument for secondary cubemap kernel" 
		  << std::endl;
	err = clSetKernelArg(second_cm_clkernelinfo.kernel,0,sizeof(cl_mem),&cl_image_mem);
	if (error_cl(err, "clSetKernelArg 0"))
		exit(1);

	std::cout << "Setting ray level mem object argument for secondary cubemap kernel" 
		  << std::endl;
	err = clSetKernelArg(second_cm_clkernelinfo.kernel,1,
			     sizeof(cl_mem),&cl_ray_level[0]);
	if (error_cl(err, "clSetKernelArg 1"))
		exit(1);

	std::cout << "Setting bounce info object argument for secondary cubemap kernel" 
		  << std::endl;
	err = clSetKernelArg(second_cm_clkernelinfo.kernel,2,
			     sizeof(cl_mem),&cl_bounce_mem[0]);
	if (error_cl(err, "clSetKernelArg 2"))
		exit(1);

	std::cout << "Setting cubemap positive x texture for secondary cubemap kernel" 
		  << std::endl;
	err = clSetKernelArg(second_cm_clkernelinfo.kernel,3,
			     sizeof(cl_mem),&cl_cm_posx_mem);
	if (error_cl(err, "clSetKernelArg 3"))
		exit(1);

	std::cout << "Setting cubemap negative x texture for secondary cubemap kernel" 
		  << std::endl;
	err = clSetKernelArg(second_cm_clkernelinfo.kernel,4,
			     sizeof(cl_mem),&cl_cm_negx_mem);
	if (error_cl(err, "clSetKernelArg 4"))
		exit(1);

	std::cout << "Setting cubemap positive y texture for secondary cubemap kernel" 
		  << std::endl;
	err = clSetKernelArg(second_cm_clkernelinfo.kernel,5,
			     sizeof(cl_mem),&cl_cm_posy_mem);
	if (error_cl(err, "clSetKernelArg 5"))
		exit(1);

	std::cout << "Setting cubemap negative y texture for secondary cubemap kernel" 
		  << std::endl;
	err = clSetKernelArg(second_cm_clkernelinfo.kernel,6,
			     sizeof(cl_mem),&cl_cm_negy_mem);
	if (error_cl(err, "clSetKernelArg 6"))
		exit(1);

	std::cout << "Setting cubemap positive z texture for secondary cubemap kernel" 
		  << std::endl;
	err = clSetKernelArg(second_cm_clkernelinfo.kernel,7,
			     sizeof(cl_mem),&cl_cm_posz_mem);
	if (error_cl(err, "clSetKernelArg 7"))
		exit(1);

	std::cout << "Setting cubemap negative z texture for secondary cubemap kernel" 
		  << std::endl;
	err = clSetKernelArg(second_cm_clkernelinfo.kernel,8,
			     sizeof(cl_mem),&cl_cm_negz_mem);
	if (error_cl(err, "clSetKernelArg 8"))
		exit(1);

	/*------------------------ Set GLUT and misc functions -----------------------*/
	rt_time.snap_time();

	glutKeyboardFunc(gl_key);
	glutMotionFunc(gl_mouse);
	glutDisplayFunc(gl_loop);
	glutIdleFunc(gl_loop);
	glutMainLoop();	

	return 0;
}


int32_t setRays(const CLKernelInfo& clkernelinfo)
{

	cl_int err;

#if 0

	/*--------------- Set values for the primary ray bundle-------------*/
	PrimaryRayGenerator::set_rays(rt_cam,primary_ray_bundle,window_size);

	if (copy_to_cl_mem(*rt_clkernelinfo.clinfo,
			   primary_ray_bundle.size_in_bytes(),
			   (void*)primary_ray_bundle.ray_array(),
			   cl_ray_mem))
		return 1;

	/* Set ray buffer argument */
	// std::cout << "Setting ray mem object argument for kernel" << std::endl;
	err = clSetKernelArg(rt_clkernelinfo.kernel,1,
			     sizeof(cl_ray_mem),&cl_ray_mem);
	if (error_cl(err, "clSetKernelArg 1"))
		return 1;
#else

	/*-------------- Set cam parameters as arguments ------------------*/
	/*-- My OpenCL implementation cannot handle using float3 as arguments!--*/
	cl_float4 cam_pos = vec3_to_float4(rt_cam.pos);
	cl_float4 cam_dir = vec3_to_float4(rt_cam.dir);
	cl_float4 cam_right = vec3_to_float4(rt_cam.right);
	cl_float4 cam_up = vec3_to_float4(rt_cam.up);

	err = clSetKernelArg(ray_clkernelinfo.kernel, 1, 
			     sizeof(cl_float4), &cam_pos);
	if (error_cl(err, "clSetKernelArg 1"))
		return 1;

	err = clSetKernelArg(ray_clkernelinfo.kernel, 2, 
			     sizeof(cl_float4), &cam_dir);
	if (error_cl(err, "clSetKernelArg 2"))
		return 1;

	err = clSetKernelArg(ray_clkernelinfo.kernel, 3, 
			     sizeof(cl_float4), &cam_right);
	if (error_cl(err, "clSetKernelArg 3"))
		return 1;

	err = clSetKernelArg(ray_clkernelinfo.kernel, 4, 
			     sizeof(cl_float4), &cam_up);
	if (error_cl(err, "clSetKernelArg 4"))
		return 1;

	/*------------------ Set ray mem object as argument ------------*/

	err = clSetKernelArg(ray_clkernelinfo.kernel, 0,
			     sizeof(cl_mem),&cl_ray_mem);

	if (error_cl(err, "clSetKernelArg 4"))
		return 1;

	/*------------------- Execute kernel to create rays ------------*/
	if (execute_cl(ray_clkernelinfo))
		std::cerr << "Error executing ray kernel" << std::endl;

#endif
	return 0;
}
