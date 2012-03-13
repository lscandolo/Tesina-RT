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

double rt_time_t::nsec_since_snap() const {
	rt_time_t t_aux;
	t_aux.snap_time();
	return nsec_diff(*this, t_aux);
}


double msec_diff(const rt_time_t& begin, const rt_time_t& end){

	timespec tp_aux;
	const timespec& tp_end = end.tp;
	const timespec& tp_begin = begin.tp;
	if ((tp_end.tv_nsec - tp_begin.tv_nsec) < 0) {
		tp_aux.tv_sec = tp_end.tv_sec-tp_begin.tv_sec-1;
		tp_aux.tv_nsec = 1e9 + tp_end.tv_nsec-tp_begin.tv_nsec;
	} else {
		tp_aux.tv_sec = tp_end.tv_sec-tp_begin.tv_sec;
		tp_aux.tv_nsec = tp_end.tv_nsec-tp_begin.tv_nsec;
	}
	double msec = tp_aux.tv_nsec/1e6 + tp_aux.tv_sec * 1e3 ;
	return msec;
}

double nsec_diff(const rt_time_t& begin, const rt_time_t& end){

	timespec tp_aux;
	const timespec& tp_end = end.tp;
	const timespec& tp_begin = begin.tp;
	if ((tp_end.tv_nsec - tp_begin.tv_nsec) < 0) {
		tp_aux.tv_sec = tp_end.tv_sec-tp_begin.tv_sec-1;
		tp_aux.tv_nsec = 1e9 + tp_end.tv_nsec-tp_begin.tv_nsec;
	} else {
		tp_aux.tv_sec = tp_end.tv_sec-tp_begin.tv_sec;
		tp_aux.tv_nsec = tp_end.tv_nsec-tp_begin.tv_nsec;
	}
	double nsec = tp_aux.tv_nsec + tp_aux.tv_sec * 1e9 ;
	return nsec;
}


#elif defined _WIN32
void rt_time_t::snap_time(){
 	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	tp = li.QuadPart;
}

double rt_time_t::msec_since_snap() const {
	LARGE_INTEGER tli,fli;
	QueryPerformanceCounter(&tli);
	QueryPerformanceFrequency(&fli);
	double freq = double(fli.QuadPart) / 1e3;

	return (double)(tli.QuadPart - tp) / freq;
}

double rt_time_t::nsec_since_snap() const {
	LARGE_INTEGER tli,fli;
	QueryPerformanceCounter(&tli);
	QueryPerformanceFrequency(&fli);
	double freq = double(fli.QuadPart) / 1e9;

	return (double)(tli.QuadPart - tp) / freq;
}

#else
  #error "UNKNOWN PLATFORM"
#endif
