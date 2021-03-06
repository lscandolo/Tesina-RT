#include <FreeImagePlus.h>

#include <cl-gl/opengl-init.hpp>
#include <cl-gl/opencl-init.hpp>

#include <iostream>

#include <GL/glew.h>

GLInfo* GLInfo::pinstance = 0;

GLInfo* GLInfo::instance()
{
        if (pinstance == 0)
                pinstance = new GLInfo();
        return pinstance;
}

GLInfo::GLInfo () :
 m_initialized(false)
{
}

bool GLInfo::initialized() 
{
        return m_initialized;
}

GLint GLInfo::initialize(int argc, char** argv, const size_t* window_size, 
                         const std::string& title) 
{
	if (window_size[0] <= 0 || window_size[1] <= 0)
		return 1;

	glutInit(&argc,argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowPosition(100,100);
	glutInitWindowSize(window_size[0], window_size[1]);
	if ((window_id = glutCreateWindow(title.c_str()) == 0))
		return 1;

	if (glewInit() != GLEW_OK)
		return 1;

#ifdef _WIN32
	windowHandle = FindWindow(NULL,title.c_str());
	deviceContext = GetDC(windowHandle);
	renderingContext = wglGetCurrentContext();
#elif defined __linux__
	renderingDisplay = glXGetCurrentDisplay();
	renderingContext = glXGetCurrentContext();
#else
#error "UNKNOWN PLATFORM"
#endif

	glClearColor(0.f,0.f,0.f,0.0f);
	glClearDepth(1.f);
	glShadeModel(GL_SMOOTH);
	
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);	
	glPixelStorei(GL_UNPACK_ALIGNMENT,1);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_TEXTURE_2D);

        m_initialized = true;

	return 0;
}

int32_t GLInfo::resize_window(const size_t* window_size)
{
        if (!m_initialized || window_size[0] == 0 || window_size[1] == 0)
                return  -1;

        glutReshapeWindow(window_size[0], window_size[1]);
        glutPostRedisplay();

        return 0;
}

GLuint create_tex_gl(uint32_t width, uint32_t height)
{
	GLuint tex;
	uint8_t* tex_data = new uint8_t[4 * width * height];

	glGenTextures(1,&tex);
	glBindTexture(GL_TEXTURE_2D, tex);

	// select modulate to mix texture with color for shading
	// glTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL );

	// when texture area is small, get closest pixel val
	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
			 GL_LINEAR);

	// when texture area is large, get closest pixel val
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, 
			GL_NEAREST);


	// if wrap is true, the texture wraps over at the edges (repeat)
	//       ... false, the texture ends at the edges (clamp)
	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,GL_REPEAT);
	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,GL_REPEAT);

	// This resizes the texture to be the required height/width
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 
		     width, height, 0, GL_RGBA, 
		     GL_UNSIGNED_BYTE, NULL);	

	// (NO MIPMAPS)

	glBindTexture(GL_TEXTURE_2D, 0);
	delete[] tex_data;

	return tex;
}

void delete_tex_gl(GLuint id)
{
        glDeleteTextures(1,&id);
}

GLuint create_buf_gl(uint32_t buf_size)
{
	GLuint buf;
	uint32_t* buf_data = new uint32_t[buf_size];
	glGenBuffers(1,&buf);
	glBindBuffer(GL_ARRAY_BUFFER,buf);
	glBufferData(GL_ARRAY_BUFFER, buf_size * sizeof(uint32_t), 
		     buf_data, GL_DYNAMIC_READ);
	glBindBuffer(GL_ARRAY_BUFFER, 0);	
	delete[] buf_data;
	return buf;
}

void delete_buf_gl(GLuint id)
{
        glDeleteBuffers(1,&id);
}


int32_t create_tex_gl_from_file(uint32_t& width, uint32_t& height, 
                                const char* file, GLuint* tex_id){
	
	fipImage img;
	if (!img.load(file)) 
		return -1;

	width = img.getWidth();
	height = img.getHeight();
	
	// if (!img.convertTo24Bits())
	// 	return -1;

	RGBQUAD pixels;
	
	uint8_t* tex_data = new uint8_t[width*height*4*sizeof(uint8_t)] ;
	for (uint32_t y = 0; y < height; ++y) {
		for (uint32_t x = 0; x < width; ++x) {
			uint8_t* p = &(tex_data[(x+y*width)*4]);
			if (!img.getPixelColor(x,y,&pixels)) {
				delete[] tex_data;
				return -1;
			}
			p[0] = pixels.rgbRed;
			p[1] = pixels.rgbGreen;
			p[2] = pixels.rgbBlue;
			p[3] = 255;
		}
	}

        glGenTextures(1,tex_id);
	glBindTexture(GL_TEXTURE_2D, *tex_id);

	// select modulate to mix texture with color for shading
	glTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL );

	// when texture area is small, get closest pixel val
	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
			 GL_NEAREST);

	// when texture area is large, get closest pixel val
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, 
			GL_NEAREST);


	// if wrap is true, the texture wraps over at the edges (repeat)
	//       ... false, the texture ends at the edges (clamp)
	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,GL_REPEAT);
	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,GL_REPEAT);

	// This resizes the texture to be the required height/width
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 
		     width, height, 0, GL_RGBA, 
		     GL_UNSIGNED_BYTE, tex_data);	

	// (NO MIPMAPS)

	glBindTexture(GL_TEXTURE_2D, 0);
	delete[] tex_data;

	return 0;

}

void print_gl_info(){
	const GLubyte *vendor, *renderer, *version; 
	vendor = glGetString(GL_VENDOR);
	renderer = glGetString(GL_RENDERER);
	version = glGetString(GL_VERSION);

	std::cerr << "OpenGL Info:" << std::endl;

	std::cerr << "\tOpenGL Vendor: " << vendor << std::endl;
	std::cerr << "\tOpenGL Version: " << version << std::endl;
	std::cerr << "\tOpenGL Renderer: " << renderer << std::endl;

	std::cerr << std::endl;

	return;
}

void print_gl_tex_2d_info(GLuint tex)
{
	glBindTexture(GL_TEXTURE_2D, tex);
	
	// Query created texture
	int tex_width, tex_height;
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &tex_width);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &tex_height);

	std::cerr << "OpenGL texture info:" << std::endl;
	std::cerr << "\tTexture width: " << tex_width << std::endl;
	std::cerr << "\tTexture height: " << tex_height << std::endl;
	std::cerr << std::endl;
}
