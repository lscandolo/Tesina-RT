#ifndef RT_TIMING_HP
#define RT_TIMING_HP

#if   defined __linux__
  #include <time.h>


struct rt_time_t {

	timespec tp;
	void snap_time();
	double msec_since_snap() const;
	double nsec_since_snap() const;
	
};

#elif defined _WIN32
#define NOMINMAX
#include <Windows.h>

struct rt_time_t {
	void snap_time();
	double msec_since_snap() const;
	double nsec_since_snap() const;
	__int64 tp;
};
#else
  #error "UNKNOWN PLATFORM"
#endif


double msec_diff(const rt_time_t& begin, const rt_time_t& end);
double nsec_diff(const rt_time_t& begin, const rt_time_t& end);




#endif // RT_TIMING_HP
