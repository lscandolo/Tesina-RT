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

CLKernelInfo clkernelinfo;
cl_mem cl_ray_mem;
cl_mem cl_tex_mem;
cl_mem cl_vert_mem;
cl_mem cl_index_mem;
GLuint gl_tex;
rt_time_t rt_time;

#define STEPS 16

void gl_key(unsigned char key, int x, int y)
{
	if (key == 'q')
		std::cout << std::endl << "Exiting..." << std::endl;
		exit(1);
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
	err = clSetKernelArg(clkernelinfo.kernel,6,
			     sizeof(cl_int),&arg);
	if (error_cl(err, "clSetKernelArg 6"))
		exit(1);

	cl_time.snap_time();
	execute_cl(clkernelinfo);
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
	uint32_t window_size[] = {800, 600};
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

	if (init_cl_kernel(&clinfo,"src/kernel/rt.cl", "trace", &clkernelinfo)){
		std::cerr << "Failed to initialize CL kernel" << std::endl;
		exit(1);
	} else {
		std::cerr << "Initialized CL kernel succesfully" << std::endl;
	}
	clkernelinfo.work_dim = 2;
	clkernelinfo.arg_count = 2;
	clkernelinfo.global_work_size[0] = window_size[0];
	clkernelinfo.global_work_size[1] = window_size[1];

	std::cout << "Setting texture mem object argument for kernel" << std::endl;
	err = clSetKernelArg(clkernelinfo.kernel,0,sizeof(cl_mem),&cl_tex_mem);
	if (error_cl(err, "clSetKernelArg 0"))
		exit(1);
	

	/*---------------------- Load model ---------------------------*/
	ModelOBJ obj;
	// if (!obj.import("models/obj/floor.obj")){
	// if (!obj.import("models/obj/teapot-low_res.obj")){
	// if (!obj.import("models/obj/teapot.obj")){
	if (!obj.import("models/obj/teapot2.obj")){
		std::cerr << "Error importing obj model." << std::endl;
		exit(1);
	}
	Mesh mesh;
	obj.toMesh(&mesh);

	/*---------------------- Print model data ----------------------*/
	
	// int triangles = obj.getNumberOfTriangles();
	int triangles = mesh.triangleCount();
	std::cerr << "Triangle count: " << triangles << std::endl;
	
	// int meshes = obj.getNumberOfModelMeshes();
	// std::cerr << "Mesh count: " << meshes << std::endl;
	
	// int vertices = obj.getNumberOfVertices();
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
	err = clSetKernelArg(clkernelinfo.kernel,2,sizeof(cl_mem),&cl_vert_mem);
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
	err = clSetKernelArg(clkernelinfo.kernel,3,sizeof(cl_mem),&cl_index_mem);
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
	err = clSetKernelArg(clkernelinfo.kernel,4,sizeof(cl_mem),&cl_bvh_nodes);
	if (error_cl(err, "clSetKernelArg 4"))
		exit(1);


	std::cout << "Moving bvh ordered triangles info to device memory." << std::endl;
	if (create_filled_cl_mem(clinfo,CL_MEM_READ_ONLY,
				 bvh.orderedTrianglesArraySize() * sizeof(tri_id),
				 bvh.orderedTrianglesArray(),
				 &cl_bvh_ordered_tri))
		exit(1);
	std::cout << "Setting bvh ordered triangles argument for kernel" << std::endl;
	err = clSetKernelArg(clkernelinfo.kernel,5,sizeof(cl_mem),&cl_bvh_ordered_tri);
	if (error_cl(err, "clSetKernelArg 5"))
		exit(1);


	/*---------------------- Create and set Camera ---------------------------*/
	Camera cam(makeVector(0,0,0), makeVector(0,0,1), makeVector(0,1,0), M_PI/4., 1.f);

	/*---------------------- Create primary ray bundle -----------------------*/
	RayBundle bundle(window_size[0]*window_size[1]);
	if (!bundle.initialize()){
		std::cout << "Failed to initialize bundle";
		return 1;
	}

	/*------------------ Set values for the primary ray bundle-----------------*/
	PrimaryRayGenerator::set_rays(cam,bundle,window_size);

	if (create_filled_cl_mem(clinfo,CL_MEM_READ_ONLY,
				 bundle.size_in_bytes(),
				 (void*)bundle.ray_array(),
				 &cl_ray_mem))
		exit(1);

	std::cerr << "Ray buffer info:" << std::endl;
	print_cl_mem_info(cl_ray_mem);

	/* Set ray buffer argument */
	std::cout << "Setting ray mem object argument for kernel" << std::endl;
	err = clSetKernelArg(clkernelinfo.kernel,1,
			     sizeof(cl_ray_mem),&cl_ray_mem);
	if (error_cl(err, "clSetKernelArg 1"))
		exit(1);


	/*------------------------ GLUT and misc functions --------------------*/

	rt_time.snap_time();

	glutKeyboardFunc(gl_key);
	glutDisplayFunc(gl_loop);
	glutIdleFunc(gl_loop);
	glutMainLoop();	

	return 0;
}




	// Ray* ray_mem = new Ray[bundle.size()];

	// err = clEnqueueReadBuffer(clinfo.command_queue, //command_queue
	// 			  cl_ray_mem, // cl_mem buffer,
	// 			  true, // cl_bool blocking_read,
	// 			  0, // size_t offset,
	// 			  bundle.size_in_bytes(), // size_t cb,
	// 			  (void*) ray_mem, // void * ptr,
	// 			  0, // cl_uint num_events_in_wait_list,
	// 			  NULL, // const cl_event * event_wait_list,
	// 			  NULL //cl_event *event
	// 	);

	// for (uint32_t i = 0; i < window_size[0]*window_size[1]; ++i){
	// 	// Ray& ray = bundle[i];
	// 	Ray& orig_ray = bundle[i];
	// 	Ray& cl_ray = ray_mem[i];
	// 	// ray.dir.normalize();
	// 	if (orig_ray.dir.z != cl_ray.dir.z) {
	// 	std::cout << orig_ray.ori.x << " " 
	// 		  << orig_ray.ori.y << " " 
	// 		  << orig_ray.ori.z
	// 		  << "   ->   "
	// 		  << orig_ray.dir.x 
	// 		  << " " << orig_ray.dir.y 
	// 		  << " " << orig_ray.dir.z
	// 		  << "    \t (" << i%window_size[0] 
	// 		  << " , " << i /window_size[1] << ")" 
	// 		  << std::endl;
	// 	}
	// 	// else
	// 	// 	std::cout << "Ray " << i << " -> " 
	// 	// 		  << cl_ray.dir.z << std::endl;
	// }

	// delete[] ray_mem;


	// std::cerr << "Vertex info:" << std::endl;

	// for (int i = 0 ; i < 6; i+= 3) {
	
	// 	float* v0 = obj.getVertexBuffer()[obj.getIndexBuffer()[i]].position;
	// 	float* v1 = obj.getVertexBuffer()[obj.getIndexBuffer()[i+1]].position;
	// 	float* v2 = obj.getVertexBuffer()[obj.getIndexBuffer()[i+2]].position;

	// 	float* n0 = obj.getVertexBuffer()[obj.getIndexBuffer()[i]].normal;
	// 	float* n1 = obj.getVertexBuffer()[obj.getIndexBuffer()[i+1]].normal;
	// 	float* n2 = obj.getVertexBuffer()[obj.getIndexBuffer()[i+2]].normal;

	// 	std::cerr << "v0: " << v0[0] << " " << v0[1] << " " << v0[2] << std::endl;
	// 	std::cerr << "v1: " << v1[0] << " " << v1[1] << " " << v1[2] << std::endl;
	// 	std::cerr << "v2: " << v2[0] << " " << v2[1] << " " << v2[2] << std::endl;

	// 	std::cerr << "n0: " << n0[0] << " " << n0[1] << " " << n0[2] << std::endl;
	// 	std::cerr << "n1: " << n1[0] << " " << n1[1] << " " << n1[2] << std::endl;
	// 	std::cerr << "n2: " << n2[0] << " " << n2[1] << " " << n2[2] << std::endl;

	// }
