#include <iostream>//!!
#include <rt/tracer.hpp>

bool Tracer::initialize(CLInfo& clinfo)
{

	if (init_cl_kernel(&clinfo,
			   "src/kernel/trace.cl", 
			   "trace", 
			   &tracer_clk))
		return false;

	tracer_clk.work_dim = 1;
	tracer_clk.arg_count = 5;


	if (init_cl_kernel(&clinfo,
			   "src/kernel/shadow-trace.cl", 
			   "shadow_trace", 
			   &shadow_clk))
		return false;

	shadow_clk.work_dim = 1;
	shadow_clk.arg_count = 6;

	timing = false;

	return true;
}


bool 
Tracer::trace(SceneInfo& scene_info, cl_mem& bvh_mem, int32_t ray_count, 
	      RayBundle& rays, HitBundle& hits, bool secondary)
{
	cl_int err;

	if (timing)
		tracer_timer.snap_time();

	err = clSetKernelArg(tracer_clk.kernel,0,sizeof(cl_mem),&hits.mem());
	if (error_cl(err, "clSetKernelArg 0"))
		return false;

	err = clSetKernelArg(tracer_clk.kernel,1,sizeof(cl_mem),&rays.mem());
	if (error_cl(err, "clSetKernelArg 1"))
		return false;

	err = clSetKernelArg(tracer_clk.kernel,2,sizeof(cl_mem),&scene_info.vertex_mem());
	if (error_cl(err, "clSetKernelArg 2"))
		return false;

	err = clSetKernelArg(tracer_clk.kernel,3,sizeof(cl_mem),&scene_info.index_mem());
	if (error_cl(err, "clSetKernelArg 3"))
		return false;

	err = clSetKernelArg(tracer_clk.kernel,4,sizeof(cl_mem),&bvh_mem);
	if (error_cl(err, "clSetKernelArg 4"))
		return false;

	bool ret;

	const int32_t  sec_size = 64;
	int32_t leftover = ray_count%sec_size;

	if (secondary) {
		if (ray_count >= sec_size) {
			tracer_clk.global_work_size[0] = ray_count - leftover;
			tracer_clk.local_work_size[0] = sec_size; 
			tracer_clk.global_work_offset[0] = 0;
			ret = !execute_cl(tracer_clk);
			if (leftover) {
				tracer_clk.global_work_size[0] = leftover;
				tracer_clk.local_work_size[0] = leftover; 
				tracer_clk.global_work_offset[0] = ray_count - leftover;
				ret = ret && !execute_cl(tracer_clk);
			}
		} else {
				tracer_clk.global_work_size[0] = ray_count;
				tracer_clk.local_work_size[0] = ray_count; 
				tracer_clk.global_work_offset[0] = 0;
				ret = ret && !execute_cl(tracer_clk);
		}
	} else {
		tracer_clk.global_work_offset[0] = 0;
		tracer_clk.global_work_size[0] = ray_count;
		tracer_clk.local_work_size[0] = 0;
		ret = !execute_cl(tracer_clk);
	}
 
	if (timing)
		tracer_time_ms = tracer_timer.msec_since_snap();

	return ret;
}

bool 
Tracer::trace(SceneInfo& scene_info, int32_t ray_count, 
	      RayBundle& rays, HitBundle& hits, bool secondary)
{
	cl_int err;

	if (timing)
		tracer_timer.snap_time();

	err = clSetKernelArg(tracer_clk.kernel,0,sizeof(cl_mem),&hits.mem());
	if (error_cl(err, "clSetKernelArg 0"))
		return false;

	err = clSetKernelArg(tracer_clk.kernel,1,sizeof(cl_mem),&rays.mem());
	if (error_cl(err, "clSetKernelArg 1"))
		return false;

	err = clSetKernelArg(tracer_clk.kernel,2,sizeof(cl_mem),&scene_info.vertex_mem());
	if (error_cl(err, "clSetKernelArg 2"))
		return false;

	err = clSetKernelArg(tracer_clk.kernel,3,sizeof(cl_mem),&scene_info.index_mem());
	if (error_cl(err, "clSetKernelArg 3"))
		return false;

	err = clSetKernelArg(tracer_clk.kernel,4,sizeof(cl_mem),&scene_info.bvh_mem());
	if (error_cl(err, "clSetKernelArg 4"))
		return false;

	// tracer_clk.global_work_size[0] = ray_count;
	
	bool ret;

	const int32_t  sec_size = 64;
	int32_t leftover = ray_count%sec_size;

	if (secondary) {
		if (ray_count >= sec_size) {
			tracer_clk.global_work_size[0] = ray_count - leftover;
			tracer_clk.local_work_size[0] = sec_size; 
			tracer_clk.global_work_offset[0] = 0;
			ret = !execute_cl(tracer_clk);
			if (leftover) {
				tracer_clk.global_work_size[0] = leftover;
				tracer_clk.local_work_size[0] = leftover; 
				tracer_clk.global_work_offset[0] = ray_count - leftover;
				ret = ret && !execute_cl(tracer_clk);
			}
		} else {
				tracer_clk.global_work_size[0] = ray_count;
				tracer_clk.local_work_size[0] = ray_count; 
				tracer_clk.global_work_offset[0] = 0;
				ret = ret && !execute_cl(tracer_clk);
		}
	} else {
		tracer_clk.global_work_offset[0] = 0;
		tracer_clk.global_work_size[0] = ray_count;
		tracer_clk.local_work_size[0] = 0;
		ret = !execute_cl(tracer_clk);
	}

	if (timing)
		tracer_time_ms = tracer_timer.msec_since_snap();

	return ret;
}

bool 
Tracer::shadow_trace(SceneInfo& si, int32_t ray_count, 
		     RayBundle& rays, HitBundle& hits, bool secondary)
{
	cl_int err;

	if (timing)
		shadow_timer.snap_time();

	err = clSetKernelArg(shadow_clk.kernel,0,sizeof(cl_mem),&hits.mem());
	if (error_cl(err, "clSetKernelArg 0"))
		return false;

	err = clSetKernelArg(shadow_clk.kernel,1,sizeof(cl_mem),&rays.mem());
	if (error_cl(err, "clSetKernelArg 1"))
		return false;

	err = clSetKernelArg(shadow_clk.kernel,2,sizeof(cl_mem),&si.vertex_mem());
	if (error_cl(err, "clSetKernelArg 2"))
		return false;

	err = clSetKernelArg(shadow_clk.kernel,3,sizeof(cl_mem),&si.index_mem());
	if (error_cl(err, "clSetKernelArg 3"))
		return false;

	err = clSetKernelArg(shadow_clk.kernel,4,sizeof(cl_mem),&si.bvh_mem());
	if (error_cl(err, "clSetKernelArg 4"))
		return false;

	err = clSetKernelArg(shadow_clk.kernel,5,sizeof(cl_mem),&si.light_mem());
	if (error_cl(err, "clSetKernelArg 5"))
		return false;

	bool ret;

	const int32_t  sec_size = 64;
	int32_t leftover = ray_count%sec_size;

	if (secondary) {
		if (ray_count >= sec_size) {
			shadow_clk.global_work_size[0] = ray_count - leftover;
			shadow_clk.local_work_size[0] = sec_size; 
			shadow_clk.global_work_offset[0] = 0;
			ret = !execute_cl(shadow_clk);
			if (leftover) {
				shadow_clk.global_work_size[0] = leftover;
				shadow_clk.local_work_size[0] = leftover; 
				shadow_clk.global_work_offset[0] = ray_count - leftover;
				ret = ret && !execute_cl(shadow_clk);
			}
		} else {
				shadow_clk.global_work_size[0] = ray_count;
				shadow_clk.local_work_size[0] = ray_count; 
				shadow_clk.global_work_offset[0] = 0;
				ret = ret && !execute_cl(shadow_clk);
		}
	} else {
		shadow_clk.global_work_offset[0] = 0;
		shadow_clk.global_work_size[0] = ray_count;
		shadow_clk.local_work_size[0] = 0;
		ret = !execute_cl(shadow_clk);
	}

	if (timing)
		shadow_time_ms = shadow_timer.msec_since_snap();

	return ret;

}

bool 
Tracer::shadow_trace(SceneInfo& si, cl_mem& bvh_mem, int32_t ray_count, 
		     RayBundle& rays, HitBundle& hits, bool secondary)
{
	cl_int err;

	if (timing)
		shadow_timer.snap_time();

	err = clSetKernelArg(shadow_clk.kernel,0,sizeof(cl_mem),&hits.mem());
	if (error_cl(err, "clSetKernelArg 0"))
		return false;

	err = clSetKernelArg(shadow_clk.kernel,1,sizeof(cl_mem),&rays.mem());
	if (error_cl(err, "clSetKernelArg 1"))
		return false;

	err = clSetKernelArg(shadow_clk.kernel,2,sizeof(cl_mem),&si.vertex_mem());
	if (error_cl(err, "clSetKernelArg 2"))
		return false;

	err = clSetKernelArg(shadow_clk.kernel,3,sizeof(cl_mem),&si.index_mem());
	if (error_cl(err, "clSetKernelArg 3"))
		return false;

	err = clSetKernelArg(shadow_clk.kernel,4,sizeof(cl_mem),&bvh_mem);
	if (error_cl(err, "clSetKernelArg 4"))
		return false;

	err = clSetKernelArg(shadow_clk.kernel,5,sizeof(cl_mem),&si.light_mem());
	if (error_cl(err, "clSetKernelArg 5"))
		return false;

	bool ret;

	const int32_t  sec_size = 64;
	int32_t leftover = ray_count%sec_size;

	if (secondary) {
		if (ray_count >= sec_size) {
			shadow_clk.global_work_size[0] = ray_count - leftover;
			shadow_clk.local_work_size[0] = sec_size; 
			shadow_clk.global_work_offset[0] = 0;
			ret = !execute_cl(shadow_clk);
			if (leftover) {
				shadow_clk.global_work_size[0] = leftover;
				shadow_clk.local_work_size[0] = leftover; 
				shadow_clk.global_work_offset[0] = ray_count - leftover;
				ret = ret && !execute_cl(shadow_clk);
			}
		} else {
				shadow_clk.global_work_size[0] = ray_count;
				shadow_clk.local_work_size[0] = ray_count; 
				shadow_clk.global_work_offset[0] = 0;
				ret = ret && !execute_cl(shadow_clk);
		}
	} else {
		shadow_clk.global_work_offset[0] = 0;
		shadow_clk.global_work_size[0] = ray_count;
		shadow_clk.local_work_size[0] = 0;
		ret = !execute_cl(shadow_clk);
	}

	if (timing)
		shadow_time_ms = shadow_timer.msec_since_snap();

	return ret;

}

void
Tracer::enable_timing(bool b)
{
	timing = b;
}

double 
Tracer::get_trace_exec_time()
{
	return tracer_time_ms;
}

double 
Tracer::get_shadow_exec_time()
{
	return shadow_time_ms;
}
