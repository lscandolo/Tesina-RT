#include <rt/timing.hpp>

#if   defined __linux__

void rt_time_t::snap_time(){
	clock_gettime(CLOCK_MONOTONIC, &tp);
}

double rt_time_t::msec_since_snap() const {
	rt_time_t t_aux;
	t_aux.snap_time();
	return msec_diff(*this, t_aux);
}


double msec_diff(const rt_time_t& begin, const rt_time_t& end){

	timespec tp_aux;
	const timespec& tp_end = end.tp;
	const timespec& tp_begin = begin.tp;
	if ((tp_end.tv_nsec - tp_begin.tv_nsec) < 0) {
		tp_aux.tv_sec = tp_end.tv_sec-tp_begin.tv_sec-1;
		tp_aux.tv_nsec = 1000000000+tp_end.tv_nsec-tp_begin.tv_nsec;
	} else {
		tp_aux.tv_sec = tp_end.tv_sec-tp_begin.tv_sec;
		tp_aux.tv_nsec = tp_end.tv_nsec-tp_begin.tv_nsec;
	}
	double msec = tp_aux.tv_nsec/1000000. + tp_aux.tv_sec * 1000. ;
	return msec;
}


#elif defined _WIN32
  #error "TIMING NOT DEFINED FOR WINDOWS"
#else
  #error "UNKNOWN PLATFORM"
#endif
