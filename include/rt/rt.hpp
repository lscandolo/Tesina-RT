#ifndef RT_HPP
#define RT_HPP

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

#include <gpu/interface.hpp>
#include <rt/math.hpp>
#include <rt/lu.hpp>
#include <rt/ray.hpp>
#include <rt/light.hpp>
#include <rt/timing.hpp>
#include <rt/camera.hpp>
#include <rt/primary-ray-generator.hpp>
#include <rt/secondary-ray-generator.hpp>
#include <rt/cubemap.hpp>
#include <rt/framebuffer.hpp>
#include <rt/ray-shader.hpp>
#include <rt/tracer.hpp>
#include <rt/scene.hpp>
#include <rt/obj-loader.hpp>
#include <rt/scene.hpp>
#include <rt/bvh.hpp>
#include <rt/cl_aux.hpp>

class Log
{
public:
    Log(){enabled = false; silent = false;}	
	~Log()
    {
        if (log.is_open())
            log.close();
    }

	int32_t initialize(std::string s = std::string("log"))
	{
        enabled = true;
        silent = false;
        log.open(s.c_str(),std::fstream::out);
        if (!log.is_open())
                return -1;
        return 0;
	}

	void out(std::string str)
	{
		log << str << std::endl;
	}

    std::ostream& o(){return log;}

    bool enabled;
    bool silent;

private:

    std::string log_file;
    std::fstream log;

};


template <typename T>
Log& operator<<(Log& l, T t)
{
    if(!l.silent)
        std::cout << t;
    if (l.enabled)
        l.o() << t;
    return l;
}

// this is the type of std::cout
typedef std::basic_ostream<char, std::char_traits<char> > CoutType;
// this is the function signature of std::endl
typedef CoutType& (*StandardEndLine)(CoutType&);
//Tnx GManNickG from StackOverflow!

Log& operator<<(Log& l, StandardEndLine e)
{
    if(!l.silent)
        std::cout << e;
    if (l.enabled)
        l.o() << e;
    return l;		
}

#endif /* RT_HPP */
