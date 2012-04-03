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

        for (int8_t i = 0; i < m_work_dim; ++i){ 
                m_global_size[i] = size[i];
        }

        return 0;
}

int32_t DeviceFunction::set_global_offset(size_t offset[3])
{
        if (!valid())
                return -1;

        for (int8_t i = 0; i < m_work_dim; ++i){ 
                m_global_offset[i] = offset[i];
        }
        return 0;
}

int32_t DeviceFunction::set_local_size(size_t size[3])
{
        if (!valid())
                return -1;

        for (int8_t i = 0; i < m_work_dim; ++i){ 
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
	if (error_cl(err, "clFinsish"))
		return -1;

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
