#pragma once
#ifndef LOG_HPP
#define LOG_HPP

#include <string>
#include <fstream>
#include <iostream>

class Log
{
public:
    Log(){enabled = false; silent = false;}	
	~Log()
    {
        if (log.is_open())
            log.close();
    }

	bool initialize(std::string s = std::string("log"))
	{
                enabled = true;
                silent = false;
                log.open(s.c_str(),std::fstream::out);
                return !log.is_open();
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
static Log& operator<<(Log& l, T t)
{
    if(!l.silent)
        std::cout << t;
    if (l.enabled)
        l.o() << t;
    return l;
}

// // this is the type of std::cout
// typedef std::basic_ostream<char, std::char_traits<char> > CoutType;
// // this is the function signature of std::endl
// typedef CoutType& (*StandardEndLine)(CoutType&);
// //Tnx GManNickG from StackOverflow!

// static Log& operator<<(Log& l, StandardEndLine e)
// {
//     if(!l.silent)
//         std::cout << e;
//     if (l.enabled)
//         l.o() << e;
//     return l;		
// }


#endif /* LOG_HPP */
