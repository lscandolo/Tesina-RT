#ifndef OPENGL_INIT_HPP
#define OPENGL_INIT_HPP

#ifdef _WIN32
#define NOMINMAX
  #include <Windows.h>
  #include <GL/glew.h>
  #include <gl/GL.h>
  #include <gl/GLU.h>
  #include <GL/glut.h>
#elif defined __linux__
  #include <GL/glew.h>
  #include <GL/gl.h>
  #include <GL/glut.h>
  #include <GL/glx.h>
#else
  #error "UNKNOWN PLATFORM"
#endif

#include <string>
#include <stdint.h>

struct GLInfo
{
	GLint window_id;
	GLint window_width;
	GLint window_height;
#ifdef _WIN32
	HWND windowHandle;
	HDC deviceContext;
	HGLRC renderingContext;
#elif defined __linux__
	Display *  renderingDisplay;
	GLXContext renderingContext;
#else
#error "UNKNOWN PLATFORM"
#endif

	GLuint vbuf;
	GLuint ebuf;
	GLuint tbuf;
};

GLint init_gl(int argc, char** argv, GLInfo* glinfo, 
	      const uint32_t* window_size, const std::string title = std::string());
GLuint create_tex_gl(uint32_t width, uint32_t height);
GLuint create_tex_gl_from_jpeg(uint32_t& width, uint32_t& height, const char* file);
GLuint create_buf_gl(uint32_t buf_size);
void print_gl_info();
void print_gl_tex_2d_info(GLuint tex);


#endif // OPENGL_INIT_HPP
