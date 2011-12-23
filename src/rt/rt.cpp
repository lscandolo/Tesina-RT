#include <iostream>

#include <cl-gl/opengl-init.hpp>
#include <cl-gl/opencl-init.hpp>
#include <rt/math.hpp>
#include <rt/ray.hpp>
#include <rt/timing.hpp>
#include <rt/primary-ray-generator.hpp>
#include <rt/scene.hpp>
#include <rt/obj-loader.hpp>
#include <rt/bvh.hpp>
#include <rt/cl_aux.hpp>

CLKernelInfo rt_clkernelinfo;
CLKernelInfo ray_clkernelinfo;
cl_mem cl_ray_mem;
cl_mem cl_tex_mem;
cl_mem cl_vert_mem;
cl_mem cl_index_mem;
GLuint gl_tex;
rt_time_t rt_time;
uint32_t window_size[] = {800, 600};
RayBundle primary_ray_bundle(window_size[0]*window_size[1]);

bool camera_change = true;

Camera rt_cam;

#define STEPS 16

int32_t setRays(const CLKernelInfo& clkernelinfo);


void gl_mouse(int x, int y)
{
	float delta = 0.001f;
	float d_inc = delta * (window_size[1]*0.5f - y);//In opengl y points downwards
	float d_yaw = delta * (x - window_size[0]*0.5f);

	if (d_inc == 0.f && d_yaw == 0.f)
		return;

	rt_cam.modifyAbsYaw(d_yaw);
	rt_cam.modifyPitch(d_inc);
	glutWarpPointer(window_size[0] * 0.5f, window_size[1] * 0.5f);
	camera_change = true;
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
	camera_change = true;
}


void gl_loop()
{
	rt_time_t cl_time;
	static int i = 0;
	static int dir = 1;
	cl_int err;
	
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	cl_int arg = i%STEPS;
	// Set div argument
	err = clSetKernelArg(rt_clkernelinfo.kernel,6,
			     sizeof(cl_int),&arg);
	if (error_cl(err, "clSetKernelArg 6"))
		exit(1);

	// Set rays
	// rt_cam.pos[2] = -30-arg;
	// rt_cam.pos[1] = 3;
	// rt_cam.modifyPitch(0.01f);
	// rt_cam.panForward((8.f-arg)/16.f);

	if (camera_change) {
		if (setRays(ray_clkernelinfo)){
			std::cerr << "Error seting ray cl_mem object" << std::endl;
			exit(1);
		}
		camera_change = false;
	}

	cl_time.snap_time();
	execute_cl(rt_clkernelinfo);
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

	if (init_gl(argc,argv,&glinfo, window_size) != 0){
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

	/*---------------------- Create shared GL-CL texture ----------------------*/
	gl_tex = create_tex_gl(window_size[0],window_size[1]);
	print_gl_tex_2d_info(gl_tex);
	if (create_cl_mem_from_gl_tex(clinfo, gl_tex, &cl_tex_mem))
		exit(1);
	print_cl_image_2d_info(cl_tex_mem);

	/* ------------------ Initialize main opencl kernel ----------------------*/

	if (init_cl_kernel(&clinfo,"src/kernel/rt.cl", "trace", &rt_clkernelinfo)){
		std::cerr << "Failed to initialize CL kernel" << std::endl;
		exit(1);
	} else {
		std::cerr << "Initialized main CL kernel succesfully" << std::endl;
	}
	rt_clkernelinfo.work_dim = 2;
	rt_clkernelinfo.arg_count = 7;
	rt_clkernelinfo.global_work_size[0] = window_size[0];
	rt_clkernelinfo.global_work_size[1] = window_size[1];

	std::cout << "Setting texture mem object argument for kernel" << std::endl;
	err = clSetKernelArg(rt_clkernelinfo.kernel,0,sizeof(cl_mem),&cl_tex_mem);
	if (error_cl(err, "clSetKernelArg 0"))
		exit(1);

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

	/*---------------------- Load model ---------------------------*/
	ModelOBJ obj;
	// if (!obj.import("models/obj/floor.obj")){
	// if (!obj.import("models/obj/teapot-low_res.obj")){
	// if (!obj.import("models/obj/teapot.obj")){
	if (!obj.import("models/obj/teapot2.obj")){
	// if (!obj.import("models/obj/frame_boat1.obj")){
	// if (!obj.import("models/obj/frame_others1.obj")){
	// if (!obj.import("models/obj/frame_water1.obj")){
		std::cerr << "Error importing obj model." << std::endl;
		exit(1);
	}
	Mesh mesh;
	obj.toMesh(&mesh);

	/*---------------------- Print model data ----------------------*/
	
	int triangles = mesh.triangleCount();
	std::cerr << "Triangle count: " << triangles << std::endl;
	
	int vertices = mesh.vertexCount();
	std::cerr << "Vertex count: " << vertices << std::endl;


	/*---------------------- Move model data to OpenCL device -----------------*/
	if (create_filled_cl_mem(clinfo,CL_MEM_READ_ONLY,
				 vertices * sizeof(Vertex),
				 mesh.vertexArray(),
				 &cl_vert_mem))
		exit(1);
	std::cerr << "Vertex buffer info:" << std::endl;
	print_cl_mem_info(cl_vert_mem);


	std::cout << "Setting vertex mem object argument for kernel" << std::endl;
	err = clSetKernelArg(rt_clkernelinfo.kernel,2,sizeof(cl_mem),&cl_vert_mem);
	if (error_cl(err, "clSetKernelArg 2"))
		exit(1);

	if (create_filled_cl_mem(clinfo,CL_MEM_READ_ONLY,
				 triangles * 3 * sizeof(uint32_t),
				 mesh.triangleArray(),
				 &cl_index_mem))
		exit(1);
	std::cout << "Index buffer info:" << std::endl;
	print_cl_mem_info(cl_index_mem);
	std::cout << "Setting index mem object argument for kernel" << std::endl;
	err = clSetKernelArg(rt_clkernelinfo.kernel,3,sizeof(cl_mem),&cl_index_mem);
	if (error_cl(err, "clSetKernelArg 3"))
		exit(1);


	/*--------------------- Create Acceleration structure --------------------*/
	std::cout << "Building BVH." << std::endl;
	rt_time.snap_time();
	BVH bvh(mesh);
	bvh.construct();
	double bvh_build_time = rt_time.msec_since_snap();
	std::cout << "Built BVH (build time: " 
		  << bvh_build_time << " msec, " 
		  << bvh.nodeArraySize() << " nodes)" << std::endl;

	/*--------------------- Create mem for bvh structures ---------------------*/
	cl_mem cl_bvh_ordered_tri, cl_bvh_nodes;
	std::cout << "Moving bvh nodes to device memory." << std::endl;
	if (create_filled_cl_mem(clinfo,CL_MEM_READ_ONLY,
				 bvh.nodeArraySize() * sizeof(BVHNode),
				 bvh.nodeArray(),
				 &cl_bvh_nodes))
		exit(1);
	std::cout << "Setting bvh node array  argument for kernel" << std::endl;
	err = clSetKernelArg(rt_clkernelinfo.kernel,4,sizeof(cl_mem),&cl_bvh_nodes);
	if (error_cl(err, "clSetKernelArg 4"))
		exit(1);


	std::cout << "Moving bvh ordered triangles info to device memory." << std::endl;
	if (create_filled_cl_mem(clinfo,CL_MEM_READ_ONLY,
				 bvh.orderedTrianglesArraySize() * sizeof(tri_id),
				 bvh.orderedTrianglesArray(),
				 &cl_bvh_ordered_tri))
		exit(1);
	std::cout << "Setting bvh ordered triangles argument for kernel" << std::endl;
	err = clSetKernelArg(rt_clkernelinfo.kernel,5,sizeof(cl_mem),&cl_bvh_ordered_tri);
	if (error_cl(err, "clSetKernelArg 5"))
		exit(1);


	/*---------------------- Set initial Camera paramaters -----------------------*/
	rt_cam.set(makeVector(0,3,-30), makeVector(0,0,1), makeVector(0,1,0), M_PI/4.,
		   window_size[0] / (float)window_size[1]);

	/*---------------------- Initialize primary ray bundle -----------------------*/
	if (!primary_ray_bundle.initialize()){
		std::cout << "Failed to initialize bundle";
		return 1;
	}

	/*---------------------- Initialize cl_mem for rays -------------------------*/

	uint32_t ray_mem_size = 
		RayBundle::expected_size_in_bytes(window_size[0]*window_size[1]);
	if (create_empty_cl_mem(clinfo, 
				CL_MEM_READ_WRITE,
				ray_mem_size,
				&cl_ray_mem))
		exit(1);
	std::cerr << "Ray buffer info:" << std::endl;
	print_cl_mem_info(cl_ray_mem);

	/* Set ray buffer argument */
	std::cout << "Setting ray mem object argument for kernel" << std::endl;
	err = clSetKernelArg(rt_clkernelinfo.kernel,1,
			     sizeof(cl_ray_mem),&cl_ray_mem);
	if (error_cl(err, "clSetKernelArg 1"))

	/*------------------------ GLUT and misc functions --------------------*/
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

	if (execute_cl(ray_clkernelinfo))
		std::cerr << "Error executing ray kernel" << std::endl;

#endif
	return 0;
}
