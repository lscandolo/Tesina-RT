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

class GLInfo
{
private:

        static GLInfo* pinstance;

        bool m_initialized;
        

public:        

        static GLInfo* instance();
        GLInfo ();
        GLint  initialize(int argc, char** argv, const size_t* window_size, 
                         const std::string& title);
        bool   initialized();

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

GLuint create_tex_gl(uint32_t width, uint32_t height);
int32_t create_tex_gl_from_file(uint32_t& width, uint32_t& height, 
                                const char* file, GLuint* tex_id);
GLuint create_buf_gl(uint32_t buf_size);
void print_gl_info();
void print_gl_tex_2d_info(GLuint tex);


#endif // OPENGL_INIT_HPP
