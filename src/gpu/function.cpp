#include <fstream>
#include <sstream>
#include <cstring>
#include <gpu/function.hpp>


DeviceFunction::DeviceFunction(CLInfo clinfo) 
{
        for (int8_t i = 0; i < 3; ++i) {
                m_local_size[i] = 0;
                m_global_size[i] = 0;
                m_global_offset[i] = 0;
        }
        m_clinfo = clinfo;
        m_work_dim = 0;
        m_initialized = false;
}

bool 
DeviceFunction::valid()
{
        return m_clinfo.initialized && m_initialized;
}

int32_t 
DeviceFunction::initialize(std::string file, std::string name)
{
	cl_int err;
        
        if (!m_clinfo.initialized)
                return -1;

	//Create a program from the kernel source code
	std::ifstream kernel_source_file(file.c_str());
	if (!kernel_source_file.good()){
		std::cerr << "Error: could not read kernel file." << std::endl;
		return -1;
	}

	std::streampos file_size;
	kernel_source_file.seekg (0, std::ios::beg);
	file_size = kernel_source_file.tellg();
	kernel_source_file.seekg (0, std::ios::end);
	file_size = kernel_source_file.tellg() - file_size;

	char *kernel_source = new char[(int)file_size+1];
	std::memset(kernel_source,0,file_size);

	kernel_source_file.seekg (0, std::ios::beg);
	kernel_source_file.read(kernel_source,file_size);
	kernel_source_file.close();
	kernel_source[file_size] = 0;
	
	m_program = clCreateProgramWithSource(m_clinfo.context,
                                              1,
                                              (const char**)&kernel_source,
                                              NULL,
                                              &err);

	delete[] kernel_source;
	if (error_cl(err, "clCreateProgramWithSource"))
		return -1;

	err = clBuildProgram(m_program,
                             0,
                             NULL,
                             NULL,
                             NULL,
                             NULL);
	if (error_cl(err, "clBuildProgram")){
		char build_log[8196];
		size_t bytes_returned;
		err = clGetProgramBuildInfo (m_program,
					     m_clinfo.device_id,
					     CL_PROGRAM_BUILD_LOG,
					     sizeof(build_log),
					     build_log,
					     &bytes_returned);

		if (!error_cl(err, "clGetProgramBuildInfo")){
			std::cerr << "Program build error log:" << std::endl
				  << build_log << std::endl;
		}
		return -1;
	}

	//specify which kernel from the program to execute
	m_kernel = clCreateKernel(m_program,
                                  name.c_str(),&err);
	if (error_cl(err, "clCreateKernel"))
		return -1;

        err = clGetKernelWorkGroupInfo(m_kernel,
                                       m_clinfo.device_id,
                                       CL_KERNEL_WORK_GROUP_SIZE,
                                       sizeof(size_t),
                                       &m_max_group_size,
                                       NULL);
	if (error_cl(err, "clGetKernelWorkGroupInfo CL_KERNEL_WORK_GROUP_SIZE"))
		return -1;

        size_t multiple;
        err = clGetKernelWorkGroupInfo(m_kernel,
                                       m_clinfo.device_id,
                                       CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE,
                                       sizeof(size_t),
                                       &multiple,
                                       NULL);
	if (error_cl(err, "clGetKernelWorkGroupInfo PREFERRED_WORK_GROUP_SIZE_MULTIPLE"))
		return -1;

        if (m_max_group_size > multiple)
                m_max_group_size =  (m_max_group_size / multiple) * multiple;

        m_initialized = true;
	return 0;
}



int32_t DeviceFunction::set_arg(int32_t arg_num, size_t arg_size, void* arg)
{
        if (!valid())
                return -1;

	cl_int err;
        std::stringstream err_str;
        err_str << "clSetKernelArg " << arg_num;

	err = clSetKernelArg(m_kernel,arg_num,arg_size,arg);
	if (error_cl(err, err_str.str()))
		return -1;

        return 0;
        
}

int32_t DeviceFunction::set_arg(int32_t arg_num, DeviceMemory& mem)
{
        if (!valid() || !mem.valid())
                return -1;

	cl_int err;
        std::stringstream err_str;
        err_str << "clSetKernelArg " << arg_num;

	err = clSetKernelArg(m_kernel,arg_num,sizeof(cl_mem),mem.ptr());
	if (error_cl(err, err_str.str()))
		return -1;

        return 0;
        
}

int32_t DeviceFunction::set_dims(uint8_t dims)
{
        if (!valid())
                return -1;

        m_work_dim = dims;
        return 0;
}

int32_t DeviceFunction::set_global_size(size_t size[3])
{
        if (!valid())
                return -1;

        for (int8_t i = 0; i < 3; ++i){ 
                m_global_size[i] = size[i];
        }

        return 0;
}

int32_t DeviceFunction::set_global_offset(size_t offset[3])
{
        if (!valid())
                return -1;

        for (int8_t i = 0; i < 3; ++i){ 
                m_global_offset[i] = offset[i];
        }
        return 0;
}

int32_t DeviceFunction::set_local_size(size_t size[3])
{
        if (!valid())
                return -1;

        for (int8_t i = 0; i < 3; ++i){ 
                m_local_size[i] = size[i];
        }
        return 0;
}

int32_t DeviceFunction::execute()
{
        if (!valid())
                return -1;

	cl_int err;
	
	//enqueue the kernel command for execution

	if (m_work_dim < 1 || m_work_dim > 3) {
		std::cerr << "Kernel work dimensions not set" << std::endl;
		return -1;
	}

	for (int8_t i = 0; i < m_work_dim; ++i) {
		if (m_global_size[i] <= 0) {
			std::cerr << "Kernel global work size not set" << std::endl;
			return -1;
		}
	}

	bool local_size_set = true;

	for (int8_t i = 0; i < m_work_dim; ++i)
		local_size_set = local_size_set && (m_local_size[i] > 0);

	// Enqueueing the kernel command for execution
	if (local_size_set)
		err = clEnqueueNDRangeKernel(m_clinfo.command_queue,
					     m_kernel,
					     m_work_dim,
					     m_global_offset,
					     m_global_size,
					     m_local_size,
					     0,NULL,NULL);
	else
		err = clEnqueueNDRangeKernel(m_clinfo.command_queue,
					     m_kernel,
					     m_work_dim,
					     m_global_offset,
					     m_global_size,
					     NULL,
					     0,NULL,NULL);

	if (error_cl(err, "clEnqueueNDRangeKernel"))
		return -1;

	/* finish command queue */
	err = clFinish(m_clinfo.command_queue);
	if (error_cl(err, "clFinish"))
		return -1;

	return 0;	
}

int32_t 
DeviceFunction::execute_single_dim(size_t global_size, size_t local_size, 
                                   size_t global_offset) 
{

        if (!valid() || global_size == 0) {
                return -1;
        }

        if (local_size == 0) {
                local_size = m_max_group_size;
        }

        size_t leftover = global_size > local_size? global_size % local_size : 0;


        size_t gsize[3] = {global_size - leftover,0,0};
        size_t goffset[3] = {global_offset,0,0};
        size_t lsize[3] = {std::min(local_size,gsize[0]),0,0};

        size_t leftover_gsize[3] = {leftover,0,0};
        size_t leftover_goffset[3] = {gsize[0]+global_offset,0,0};
        size_t leftover_lsize[3] = {leftover,0,0};

        cl_int err;
        err = clEnqueueNDRangeKernel(m_clinfo.command_queue,
                                     m_kernel,
                                     1,
                                     goffset,
                                     gsize,
                                     lsize,
                                     0,NULL,NULL);
	if (error_cl(err, "clEnqueueNDRangeKernel"))
		return -1;

        if (leftover) {
                err = clEnqueueNDRangeKernel(m_clinfo.command_queue,
                                             m_kernel,
                                             1,
                                             leftover_goffset,
                                             leftover_gsize,
                                             leftover_lsize,
                                             0,NULL,NULL);
                if (error_cl(err, "clEnqueueNDRangeKernel"))
                        return -1;
        }

	/* finish command queue */
	err = clFinish(m_clinfo.command_queue);
	if (error_cl(err, "clFinish"))
		return -1;


        return 0;
}

int32_t 
DeviceFunction::enqueue_single_dim(size_t global_size, size_t local_size,
                                   size_t global_offset) 
{

        if (!valid() || global_size == 0) {
                return -1;
        }

        if (local_size == 0) {
                local_size = m_max_group_size;
        }

        size_t leftover = global_size > local_size? global_size % local_size : 0;


        size_t gsize[3] = {global_size - leftover,0,0};
        size_t goffset[3] = {global_offset,0,0};
        size_t lsize[3] = {std::min(local_size,gsize[0]),0,0};

        size_t leftover_gsize[3] = {leftover,0,0};
        size_t leftover_goffset[3] = {gsize[0]+global_offset,0,0};
        size_t leftover_lsize[3] = {leftover,0,0};

        cl_int err;
        err = clEnqueueNDRangeKernel(m_clinfo.command_queue,
                                     m_kernel,
                                     1,
                                     goffset,
                                     gsize,
                                     lsize,
                                     0,NULL,NULL);
	if (error_cl(err, "clEnqueueNDRangeKernel"))
		return -1;

        if (leftover) {
                err = clEnqueueNDRangeKernel(m_clinfo.command_queue,
                                             m_kernel,
                                             1,
                                             leftover_goffset,
                                             leftover_gsize,
                                             leftover_lsize,
                                             0,NULL,NULL);
                if (error_cl(err, "clEnqueueNDRangeKernel"))
                        return -1;
        }

	/* finish command queue */
        if (!m_clinfo.sync()) {
        	err = clFlush(m_clinfo.command_queue);
                if (error_cl(err, "clFlush"))
                        return -1;
        } else {
        	err = clFinish(m_clinfo.command_queue);
                if (error_cl(err, "clFinish"))
                        return -1;
        }
        return 0;
}


int32_t DeviceFunction::enqueue()
{
        if (!valid())
                return -1;

	cl_int err;
	
	//enqueue the kernel command for execution

	if (m_work_dim < 1 || m_work_dim > 3) {
		std::cerr << "Kernel work dimensions not set" << std::endl;
		return -1;
	}

	for (int8_t i = 0; i < m_work_dim; ++i) {
		if (m_global_size[i] <= 0) {
			std::cerr << "Kernel global work size not set" << std::endl;
			return -1;
		}
	}

	bool local_size_set = true;

	for (int8_t i = 0; i < m_work_dim; ++i)
		local_size_set = local_size_set && (m_local_size[i] > 0);

	// Enqueueing the kernel command for execution
	if (local_size_set)
		err = clEnqueueNDRangeKernel(m_clinfo.command_queue,
					     m_kernel,
					     m_work_dim,
					     m_global_offset,
					     m_global_size,
					     m_local_size,
					     0,NULL,NULL);
	else
		err = clEnqueueNDRangeKernel(m_clinfo.command_queue,
					     m_kernel,
					     m_work_dim,
					     m_global_offset,
					     m_global_size,
					     NULL,
					     0,NULL,NULL);

	if (error_cl(err, "clEnqueueNDRangeKernel"))
		return -1;

	/* finish command queue */
        if (!m_clinfo.sync()) {
        	err = clFlush(m_clinfo.command_queue);
                if (error_cl(err, "clFlush"))
                        return -1;
        } else {
        	err = clFinish(m_clinfo.command_queue);
                if (error_cl(err, "clFinish"))
                        return -1;
        }

	return 0;	
}

int32_t
DeviceFunction::release()
{
        if (!valid())
                return -1;

        cl_int err;
        err = clReleaseKernel(m_kernel);
        if (error_cl(err, "clReleaseKernel"))
                return -1;
        err = clReleaseProgram(m_program);
        if (error_cl(err, "clReleaseProgram"))
                return -1;

        return 0;
}

int32_t 
DeviceFunction::initialize(std::string name)
{
	cl_int err;

        if (!m_clinfo.initialized)
                return -1;

	//specify which kernel from the program to execute
	m_kernel = clCreateKernel(m_program,
                                  name.c_str(),&err);
	if (error_cl(err, "clCreateKernel"))
		return -1;

        err = clGetKernelWorkGroupInfo(m_kernel,
                                       m_clinfo.device_id,
                                       CL_KERNEL_WORK_GROUP_SIZE,
                                       sizeof(size_t),
                                       &m_max_group_size,
                                       NULL);
	if (error_cl(err, "clGetKernelWorkGroupInfo CL_KERNEL_WORK_GROUP_SIZE"))
		return -1;

        size_t multiple;
        err = clGetKernelWorkGroupInfo(m_kernel,
                                       m_clinfo.device_id,
                                       CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE,
                                       sizeof(size_t),
                                       &multiple,
                                       NULL);
	if (error_cl(err, "clGetKernelWorkGroupInfo PREFERRED_WORK_GROUP_SIZE_MULTIPLE"))
		return -1;
        if (m_max_group_size > multiple)
                m_max_group_size =  (m_max_group_size / multiple) * multiple;

        m_initialized = true;
	return 0;
}

size_t
DeviceFunction::max_group_size()
{
        return m_max_group_size;
}
