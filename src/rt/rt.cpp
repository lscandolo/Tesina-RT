#include <iostream>
#include <time.h>

#include <cl-gl/opengl-init.hpp>
#include <cl-gl/opencl-init.hpp>
#include <rt/math.hpp>
#include <rt/ray.hpp>
#include <rt/primary-ray-generator.hpp>

CLKernelInfo clkernelinfo;
cl_mem cl_ray_mem;
cl_mem cl_tex_mem;
GLuint gl_tex;
timespec tp;
#define STEPS 256

timespec compute_diff(timespec tp_begin, timespec tp_end)
{
	timespec tp_aux;
	if ((tp_end.tv_nsec - tp_begin.tv_nsec) < 0) {
		tp_aux.tv_sec = tp_end.tv_sec-tp_begin.tv_sec-1;
		tp_aux.tv_nsec = 1000000000+tp_end.tv_nsec-tp_begin.tv_nsec;
	} else {
		tp_aux.tv_sec = tp_end.tv_sec-tp_begin.tv_sec;
		tp_aux.tv_nsec = tp_end.tv_nsec-tp_begin.tv_nsec;
	}
	return tp_aux;
}

void gl_key(unsigned char key, int x, int y)
{
	if (key == 'q')
		std::cout << std::endl << "Exiting..." << std::endl;
		exit(1);
}


void gl_loop()
{
	static int i = 0;
	static int dir = 1;
	cl_int err;
	
	//////////// CL STUFF
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	cl_int arg = i%STEPS;

	// Set div argument
	err = clSetKernelArg(clkernelinfo.kernel,2,
			     sizeof(cl_int),&arg);
	if (error_cl(err, "clSetKernelArg 2"))
		exit(1);

	// Set ray buffer argument
	err = clSetKernelArg(clkernelinfo.kernel,1,
			     sizeof(cl_ray_mem),&cl_ray_mem);
	if (error_cl(err, "clSetKernelArg 1"))
		exit(1);



	execute_cl(clkernelinfo);
	
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
		timespec _tp, d;
		clock_gettime(CLOCK_MONOTONIC, &_tp);
		d = compute_diff(tp, _tp);
		double msec = d.tv_nsec/1000000. + d.tv_sec * 1000. ;
		std::cout << "Time elapsed: " 
			  << msec << " milliseconds " 
			  << "\t(" 
			  << int(STEPS / (msec/1000))
			  << " FPS)          \r" ;
		std::flush(std::cout);
		tp = _tp;
	}		
	glutSwapBuffers();
}

int main (int argc, char** argv)
{

	CLInfo clinfo;
	GLInfo glinfo;
	int window_size[] = {800, 600};
	cl_int err;
	
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

	////////////// Create gl_tex and cl_tex_mem /////////////////
	gl_tex = create_tex_gl(window_size[0],window_size[1]);
	print_gl_tex_2d_info(gl_tex);
	if (create_cl_mem_from_gl_tex(clinfo, gl_tex, &cl_tex_mem))
		exit(1);
	print_cl_image_2d_info(cl_tex_mem);
	/////////////////////////////////////////////

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


	/*---------------------- Create and set Camera ---------------------------*/
	Camera cam(vec3(0,0,0), vec3(0,0,1), vec3(0,1,0), M_PI/4., 1.f);
	/*---------------------- Create primary ray bundle -----------------------*/
	RayBundle bundle(window_size[0]*window_size[1]);
	if (!bundle.initialize()){
		std::cout << "Failed to initialize bundle";
		return 1;
	}
	/*------------------ Set values for the primary ray bundle-----------------*/
	PrimaryRayGenerator::set_rays(cam,bundle,window_size);

	for (uint32_t i = 0; i < bundle.size(); ++i){
		Ray& ray = bundle[i];
		if (ray.dir.x != 0 ||
		    ray.dir.y != 0 ||
		    ray.dir.z != 0)
			ray.dir.normalize();
	}

	if (create_filled_cl_mem(clinfo,CL_MEM_READ_WRITE,
				 bundle.size_in_bytes(),
				 (void*)bundle.ray_array(),
				 &cl_ray_mem))
		exit(1);

	print_cl_mem_info(cl_ray_mem);

	Ray* ray_mem = new Ray[bundle.size()];

	err = clEnqueueReadBuffer(clinfo.command_queue, //command_queue
				  cl_ray_mem, // cl_mem buffer,
				  true, // cl_bool blocking_read,
				  0, // size_t offset,
				  bundle.size_in_bytes(), // size_t cb,
				  (void*) ray_mem, // void * ptr,
				  0, // cl_uint num_events_in_wait_list,
				  NULL, // const cl_event * event_wait_list,
				  NULL //cl_event *event
		);

	for (uint32_t i = 0; i < window_size[0]*window_size[1]; ++i){
		// Ray& ray = bundle[i];
		Ray& orig_ray = bundle[i];
		Ray& cl_ray = ray_mem[i];
		// ray.dir.normalize();
		if (orig_ray.dir.z != cl_ray.dir.z) {
		std::cout << orig_ray.ori.x << " " 
			  << orig_ray.ori.y << " " 
			  << orig_ray.ori.z
			  << "   ->   "
			  << orig_ray.dir.x 
			  << " " << orig_ray.dir.y 
			  << " " << orig_ray.dir.z
			  << "    \t (" << i%window_size[0] 
			  << " , " << i /window_size[1] << ")" 
			  << std::endl;
		}
		// else
		// 	std::cout << "Ray " << i << " -> " 
		// 		  << cl_ray.dir.z << std::endl;
	}
	



	delete[] ray_mem;

	glutKeyboardFunc(gl_key);
	glutDisplayFunc(gl_loop);
	glutIdleFunc(gl_loop);
	glutMainLoop();	

	return 0;
}

