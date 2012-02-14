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

	return true;
}


bool 
Tracer::trace(SceneInfo& scene_info, int32_t ray_count, 
	      RayBundle& rays, HitBundle& hits)
{
	cl_int err;

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

	tracer_clk.global_work_size[0] = ray_count;

	return !execute_cl(tracer_clk);
}

bool 
Tracer::shadow_trace(SceneInfo& si, int32_t ray_count, 
		     RayBundle& rays, HitBundle& hits)
{
	cl_int err;

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

	shadow_clk.global_work_size[0] = ray_count;

	return !execute_cl(shadow_clk);
}


