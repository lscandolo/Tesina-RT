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
CLKernelInfo shadow_clkernelinfo;
CLKernelInfo ray_clkernelinfo;
CLKernelInfo cm_clkernelinfo;

cl_mem cl_ray_mem;
cl_mem cl_vert_mem;
cl_mem cl_index_mem;
cl_mem cl_tex_mem;
cl_mem cl_prim_hit_mem;

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

	d_inc = std::min(std::max(d_inc, -0.01f), 0.01f);
	d_yaw = std::min(std::max(d_yaw, -0.01f), 0.01f);

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
	
	glClear(GL_COLOR_BUFFER_BIT);

	cl_int arg = i%STEPS;

	// Set div argument
	err = clSetKernelArg(shadow_clkernelinfo.kernel,6,
			     sizeof(cl_int),&arg);
	if (error_cl(err, "clSetKernelArg 6"))
		exit(1);

	/* Set rays if camera has moved */
	if (camera_change) {
		if (setRays(ray_clkernelinfo)){
			std::cerr << "Error seting ray cl_mem object" << std::endl;
			exit(1);
		}
		camera_change = false;
	}

	cl_time.snap_time();
	execute_cl(rt_clkernelinfo);
	execute_cl(shadow_clkernelinfo);
	execute_cl(cm_clkernelinfo);

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
	print_cl_info(clinfo);

	/*---------------------- Create shared GL-CL texture ----------------------*/
	gl_tex = create_tex_gl(window_size[0],window_size[1]);
	print_gl_tex_2d_info(gl_tex);
	if (create_cl_mem_from_gl_tex(clinfo, gl_tex, &cl_tex_mem))
		exit(1);
	print_cl_image_2d_info(cl_tex_mem);


	/* ------------------ Initialize primary trace opencl kernel -----------------*/

	if (init_cl_kernel(&clinfo,"src/kernel/trace.cl", "trace", &rt_clkernelinfo)){
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

	if (init_cl_kernel(&clinfo,"src/kernel/trace_any.cl", "trace_any", 
			   &shadow_clkernelinfo)){
		std::cerr << "Failed to initialize CL kernel" << std::endl;
		exit(1);
	} else {
		std::cerr << "Initialized primary shadow CL kernel succesfully" << std::endl;
	}
	shadow_clkernelinfo.work_dim = 2;
	shadow_clkernelinfo.arg_count = 8;
	shadow_clkernelinfo.global_work_size[0] = window_size[0];
	shadow_clkernelinfo.global_work_size[1] = window_size[1];

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


	/*--------------------- Create Acceleration structure --------------------*/
	std::cout << "Building BVH." << std::endl;
	rt_time.snap_time();
	BVH bvh(mesh);
	bvh.construct();
	double bvh_build_time = rt_time.msec_since_snap();
	std::cout << "Built BVH (build time: " 
		  << bvh_build_time << " msec, " 
		  << bvh.nodeArraySize() << " nodes)" << std::endl;

	/*---------------------- Move model data to OpenCL device -----------------*/
	if (create_filled_cl_mem(clinfo,CL_MEM_READ_ONLY,
				 vertices * sizeof(Vertex),
				 mesh.vertexArray(),
				 &cl_vert_mem))
		exit(1);
	std::cerr << "Vertex buffer info:" << std::endl;
	print_cl_mem_info(cl_vert_mem);



	if (create_filled_cl_mem(clinfo,CL_MEM_READ_ONLY,
				 triangles * 3 * sizeof(uint32_t),
				 mesh.triangleArray(),
				 &cl_index_mem))
		exit(1);
	std::cout << "Index buffer info:" << std::endl;
	print_cl_mem_info(cl_index_mem);

	/*--------------------- Move bvh to device memory ---------------------*/
	cl_mem cl_bvh_nodes;
	std::cout << "Moving bvh nodes to device memory." << std::endl;
	if (create_filled_cl_mem(clinfo,CL_MEM_READ_ONLY,
				 bvh.nodeArraySize() * sizeof(BVHNode),
				 bvh.nodeArray(),
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
	uint32_t prim_hit_size = sizeof(RayHitInfo) * window_size[0] * window_size[1];

	if (create_empty_cl_mem(clinfo, 
				CL_MEM_READ_WRITE,
				prim_hit_size,
				&cl_prim_hit_mem))
		exit(1);
	std::cerr << "Primary rays hit buffer info:" << std::endl;
	print_cl_mem_info(cl_prim_hit_mem);

	/*---------------------- Initialize device memory for rays -------------------*/
	uint32_t ray_mem_size = 
		RayBundle::expected_size_in_bytes(window_size[0]*window_size[1]);
	if (create_empty_cl_mem(clinfo, 
				CL_MEM_READ_WRITE,
				ray_mem_size,
				&cl_ray_mem))
		exit(1);
	std::cerr << "Ray buffer info:" << std::endl;
	print_cl_mem_info(cl_ray_mem);


	/*----------------------- Set primary trace kernel arguments -----------------*/

	std::cout << "Setting rays hit mem object argument for trace kernel" << std::endl;
	err = clSetKernelArg(rt_clkernelinfo.kernel,0,
			     sizeof(cl_mem),&cl_prim_hit_mem);
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
	err = clSetKernelArg(shadow_clkernelinfo.kernel,0,sizeof(cl_mem),&cl_tex_mem);
	if (error_cl(err, "clSetKernelArg 0"))
		exit(1);

	std::cout << "Setting rays hit mem object argument for shadow kernel" << std::endl;
	err = clSetKernelArg(shadow_clkernelinfo.kernel,1,
			     sizeof(cl_mem),&cl_prim_hit_mem);
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
	

	/*------------------------ Set up cubemap kernel arguments -------------------*/

	std::cout << "Setting output mem object argument for cubemap kernel" << std::endl;
	err = clSetKernelArg(cm_clkernelinfo.kernel,0,sizeof(cl_mem),&cl_tex_mem);
	if (error_cl(err, "clSetKernelArg 0"))
		exit(1);

	std::cout << "Setting rays hit mem object argument for cubemap kernel" << std::endl;
	err = clSetKernelArg(cm_clkernelinfo.kernel,1,
			     sizeof(cl_mem),&cl_prim_hit_mem);
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
