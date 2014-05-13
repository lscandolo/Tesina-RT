#include <rt/tester.hpp>

int main (int argc, char** argv) 
{


        int32_t ini_err;
        INIReader ini;
        ini_err = ini.load_file("rt.ini");
        if (ini_err < 0)
                std::cout << "Error at ini file line: " << -ini_err << "\n";
        else if (ini_err > 0)
                std::cout << "Unable to open ini file" << "\n";

        // Initialize OpenGL and OpenCL (with simple value)
        size_t window_size[] = {512,512};
        GLInfo* glinfo = GLInfo::instance();

        if (glinfo->initialize(argc,argv, window_size, "RT") != 0){
                std::cerr << "Failed to initialize GL" << "\n";
                pause_and_exit(1);
        } else { 
                std::cout << "Initialized GL succesfully" << "\n";
        }

        CLInfo* clinfo = CLInfo::instance();

        if (clinfo->initialize(1) != CL_SUCCESS){
                std::cerr << "Failed to initialize CL" << "\n";
                pause_and_exit(1);
        } else { 
                std::cout << "Initialized CL succesfully" << "\n";
        }
        clinfo->print_info();

        // Initialize device interface and generic gpu library
        DeviceInterface* device = DeviceInterface::instance();
        if (device->initialize()) {
                std::cerr << "Failed to initialize device interface" << "\n";
                pause_and_exit(1);
        }

        if (DeviceFunctionLibrary::instance()->initialize()) {
                std::cerr << "Failed to initialize function library" << "\n";
                pause_and_exit(1);
        }

        /* Initialize renderer */
        Tester* tester = Tester::getSingletonPtr();
        tester->initialize();

        CLInfo::instance()->set_sync(false);

        // /* Set callbacks */
        // glutKeyboardFunc(gl_key);
        // glutMotionFunc(gl_mouse);
        // glutDisplayFunc(gl_loop);
        // glutIdleFunc(gl_loop);
        // glutMainLoop(); 
        // CLInfo::instance()->release_resources();

        tester->loop();


        return 0;
}

