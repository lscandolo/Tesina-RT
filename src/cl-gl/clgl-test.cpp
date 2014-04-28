#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif

#include <stdio.h>
#include <CL/cl.h>
#include <cl-gl/opengl-init.hpp>
#include <cl-gl/opencl-init.hpp>
#include <misc/log.hpp>

#include <FreeImage.h>
#include <FreeImagePlus.h>

#include <stdint.h>

#include <iostream>
#include <cmath>

#include <time.h>

#define TEX_WIDTH 800
#define TEX_HEIGHT 600

#define STEPS 256

CLKernelInfo clkernelinfo;
GLuint gl_tex, gl_buf;
cl_mem cl_tex_mem, cl_buf_mem;

#ifdef __linux__
timespec tp;

double compute_diff(timespec tp_begin, timespec tp_end)
{
	timespec tp_aux;
	if ((tp_end.tv_nsec - tp_begin.tv_nsec) < 0) {
		tp_aux.tv_sec = tp_end.tv_sec-tp_begin.tv_sec-1;
		tp_aux.tv_nsec = 1e9+tp_end.tv_nsec-tp_begin.tv_nsec;
	} else {
		tp_aux.tv_sec = tp_end.tv_sec-tp_begin.tv_sec;
		tp_aux.tv_nsec = tp_end.tv_nsec-tp_begin.tv_nsec;
	}
	double msec = tp_aux.tv_nsec/1e6 + tp_aux.tv_sec * 1e3 ;
	return msec;
}
#elif defined _WIN32
__int64 tp;

__int64 snap_time(){
 	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return (__int64)(li.QuadPart);
}

__int64 compute_diff(__int64 tp_begin, __int64 tp_end)
{
        LARGE_INTEGER fli;
	QueryPerformanceFrequency(&fli);
	double freq = double(fli.QuadPart) / 1e3;
        return (tp_end-tp_begin) / freq;
}


#endif



void gl_key(unsigned char key, int x, int y)
{
        (void)x;
        (void)y;

	if (key == 'q')
		std::cout << std::endl;
		exit(1);
}

void gl_loop()
{
	static int i = 0;
	static int dir = 1;
	cl_int err;
	
	//////////// CL STUFF
        acquire_gl_tex(cl_tex_mem);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	cl_int arg = i%STEPS;
	err = clSetKernelArg(clkernelinfo.kernel,1,sizeof(cl_int),&arg);
	if (error_cl(err, "clSetKernelArg 1"))
		exit(1);
	execute_cl(clkernelinfo);
	
	////////////////// Immediate mode textured quad
        release_gl_tex(cl_tex_mem);
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
#ifdef __linux__
		timespec _tp;
		clock_gettime(CLOCK_MONOTONIC, &_tp);
		double msec = compute_diff(tp, _tp);
#elif defined _WIN32
                __int64 _tp;
                _tp = snap_time();
                double msec = compute_diff(tp,_tp);
#endif
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

int main(int argc, char** argv)
{
	size_t window_size[] = {TEX_WIDTH, TEX_HEIGHT};
	cl_int err;

        GLInfo* glinfo = GLInfo::instance();

	if (glinfo->initialize(argc,argv, window_size, 
                               "OpenCL-OpenGL interop test") != 0){
		std::cerr << "Failed to initialize GL" << std::endl;
		exit(1);
	} else { 
		std::cout << "Initialized GL succesfully" << std::endl;
	}

        CLInfo* clinfo = CLInfo::instance();

	if (clinfo->initialize() != CL_SUCCESS){
		std::cerr << "Failed to initialize CL" << std::endl;
		exit(1);
	} else { 
		std::cout << "Initialized CL succesfully" << std::endl;
	}

	clinfo->print_info();

	////////////// Create gl_tex and buf /////////////////
	gl_tex = create_tex_gl(TEX_WIDTH,TEX_HEIGHT);
	gl_buf = create_buf_gl(1024);
	print_gl_tex_2d_info(gl_tex);
	///////////// Create empty cl_mem
	if (create_empty_cl_mem(CL_MEM_READ_WRITE, 1024, &cl_buf_mem)) {
		std::cerr << "*** Error creating OpenCL buffer" << std::endl;
	} else {
		std::cerr << "Created OpenCl buffer succesfully" << std::endl;
		print_cl_mem_info(cl_buf_mem);
	}
	print_cl_mem_info(cl_buf_mem);

	////////////// Create cl_mem from gl_tex
	if (create_cl_mem_from_gl_tex(gl_tex, &cl_tex_mem))
		exit(1);
	print_cl_image_2d_info(cl_tex_mem);
	
	if (init_cl_kernel("src/kernel/clgl-test.cl", "Grad", &clkernelinfo)){
		std::cerr << "Failed to initialize CL kernel" << std::endl;
		exit(1);
	} else {
		std::cerr << "Initialized CL kernel succesfully" << std::endl;
	}

	clkernelinfo.work_dim = 2;
	clkernelinfo.arg_count = 2;
	clkernelinfo.global_work_size[0] = TEX_WIDTH;
	clkernelinfo.global_work_size[1] = TEX_HEIGHT;

	std::cout << "Setting texture mem object argument for kernel" << std::endl;
	err = clSetKernelArg(clkernelinfo.kernel,0,sizeof(cl_mem),&cl_tex_mem);
	if (error_cl(err, "clSetKernelArg 0"))
		exit(1);

	glutKeyboardFunc(gl_key);
	glutDisplayFunc(gl_loop);
	glutIdleFunc(gl_loop);

#ifdef __linux__
	clock_gettime(CLOCK_MONOTONIC, &tp);
#elif defined _WIN32
        tp = snap_time();

#endif
	std::cout << std::endl;
	glutMainLoop();	

	return 0;
}
