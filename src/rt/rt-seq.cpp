#include <algorithm>
#include <iostream>
#include <string>
#include <sstream>

#include <cl-gl/opengl-init.hpp>
#include <cl-gl/opencl-init.hpp>
#include <rt/rt.hpp>

uint32_t MAX_BOUNCE = 5;

uint32_t frames = 0;
uint32_t current_frame = 0;
uint32_t motion_rate = 1;

CLInfo clinfo;
GLInfo glinfo;
cl_mem cl_tex_mem;
cl_mem bvh_mem;

RayBundle                ray_bundle_1,ray_bundle_2;
HitBundle                hit_bundle;
PrimaryRayGenerator     prim_ray_gen;
SecondaryRayGenerator   sec_ray_gen;
RayShader                ray_shader;
std::vector<SceneInfo>  scene_info;
Cubemap                  cubemap;
FrameBuffer              framebuffer;
Tracer                   tracer;
Camera                   camera;

GLuint gl_tex;
rt_time_t rt_time;
rt_time_t seq_time;
uint32_t window_size[] = {512, 512};

int32_t pixel_count = window_size[0] * window_size[1];
int32_t best_tile_size;

#define STEPS 16

void gl_mouse(int x, int y)
{
	float delta = 0.001f;
	float d_inc = delta * (window_size[1]*0.5f - y);/* y axis points downwards */
	float d_yaw = delta * (x - window_size[0]*0.5f);

	float max_motion = 0.05f;

	d_inc = std::min(std::max(d_inc, -max_motion), max_motion);
	d_yaw = std::min(std::max(d_yaw, -max_motion), max_motion);

	if (d_inc == 0.f && d_yaw == 0.f)
		return;

	camera.modifyAbsYaw(d_yaw);
	camera.modifyPitch(d_inc);
	glutWarpPointer(window_size[0] * 0.5f, window_size[1] * 0.5f);
}

void gl_key(unsigned char key, int x, int y)
{
	float delta = 2.f;

	sample_cl samples1[] = {{ 0.f , 0.f, 1.f}};
	sample_cl samples4[] = {{ 0.25f , 0.25f, 0.25f},
				{ 0.25f ,-0.25f, 0.25f},
				{-0.25f , 0.25f, 0.25f},
				{-0.25f ,-0.25f, 0.25f}};
	switch (key){
	case 'a':
		camera.panRight(-delta);
		break;
	case 's':
		camera.panForward(-delta);
		break;
	case 'w':
		camera.panForward(delta);
		break;
	case 'd':
		camera.panRight(delta);
		break;
	case 'q':
		std::cout << std::endl << "Exiting..." << std::endl;
		exit(1);
		break;
	case '1':
		MAX_BOUNCE = 1;
		break;
	case '0':
		MAX_BOUNCE = 0;
		break;
	case '2':
		MAX_BOUNCE = 2;
		break;
	case '3':
		MAX_BOUNCE = 3;
		break;
	case '4':
		MAX_BOUNCE = 4;
		break;
	case '5':
		MAX_BOUNCE = 5;
		break;
	case '6':
		MAX_BOUNCE = 6;
		break;
	case '7':
		MAX_BOUNCE = 7;
		break;
	case '8':
		MAX_BOUNCE = 8;
		break;
	case '9':
		MAX_BOUNCE = 9;
		break;
	case 'p':
		motion_rate = 1-motion_rate;
		break;
	case 'o':
		camera.set(makeVector(0,3,-30), makeVector(0,0,1), makeVector(0,1,0), M_PI/4.,
			   window_size[0] / (float)window_size[1]);
		break;
	case 'i':
		camera.set(makeVector(0,5,-30), makeVector(0,-0.5,1), makeVector(0,1,0), M_PI/4.,
			   window_size[0] / (float)window_size[1]);
		break;
	case 'k':
		camera.set(makeVector(0,25,-60), makeVector(0,-0.5,1), makeVector(0,1,0), M_PI/4.,
			   window_size[0] / (float)window_size[1]);
		break;
	case 'l':
		camera.set(makeVector(0,120,0), makeVector(0,-1,0.01), makeVector(0,1,0), M_PI/4.,
			   window_size[0] / (float)window_size[1]);
		break;
	case 'r': /* Set 1 sample per pixel */
		if (!prim_ray_gen.set_spp(1,samples1)){
			std::cerr << "Error seting spp" << std::endl;
			exit(1);
		}
		break;
	case 't': /* Set 4 samples per pixel */
		if (!prim_ray_gen.set_spp(4,samples4)){
			std::cerr << "Error seting spp" << std::endl;
			exit(1);
		}
		break;
	}
}


void gl_loop()
{
	static int i = 0;
	static int dir = 1;
	static int total_ray_count = 0;
	static double min_time=-1;
	static double max_time=-1;

	double prim_gen_time = 0;
	double sec_gen_time = 0;
	double prim_trace_time = 0;
	double sec_trace_time = 0;
	double prim_shadow_trace_time = 0;
	double sec_shadow_trace_time = 0;
	double shader_time = 0;
	double fb_clear_time = 0;
	double fb_copy_time = 0;

	glClear(GL_COLOR_BUFFER_BIT);
        acquire_gl_tex(cl_tex_mem,clinfo);

	if (!framebuffer.clear()) {
		std::cerr << "Failed to clear framebuffer." << std::endl;
		exit(1);
	}
	fb_clear_time = framebuffer.get_clear_exec_time();

	cl_int arg = i%STEPS;
	int32_t tile_size = best_tile_size;

	directional_light_cl light;
	light.set_dir(0.05f * (arg - 8.f) , -0.6f, 0.2f);
	light.set_color(0.05f * (fabsf(arg)) + 0.1f, 0.2f, 0.05f * fabsf(arg+4.f));

	SceneInfo &si = scene_info[current_frame];

	si.set_dir_light(light);
	color_cl ambient;
	ambient[0] = ambient[1] = ambient[2] = 0.1f;
	si.set_ambient_light(ambient);

	int32_t sample_count = pixel_count * prim_ray_gen.get_spp();
	for (int32_t offset = 0; offset < sample_count; offset+= tile_size) {

		
		RayBundle* ray_in =  &ray_bundle_1;
		RayBundle* ray_out = &ray_bundle_2;

		if (sample_count - offset < tile_size)
			tile_size = sample_count - offset;

		if (!prim_ray_gen.set_rays(camera, ray_bundle_1, window_size,
					   tile_size, offset)) {
			std::cerr << "Error seting primary ray bundle" << std::endl;
			exit(1);
		}
		prim_gen_time += prim_ray_gen.get_exec_time();

		total_ray_count += tile_size;

		// if (!tracer.trace(si, tile_size, *ray_in, hit_bundle)){
		if (!tracer.trace(si, bvh_mem, tile_size, *ray_in, hit_bundle)){
			std::cerr << "Failed to trace." << std::endl;
			exit(1);
		}
		prim_trace_time += tracer.get_trace_exec_time();

		// if (!tracer.shadow_trace(si, tile_size, *ray_in, hit_bundle)){
		if (!tracer.shadow_trace(si, bvh_mem, tile_size, *ray_in, hit_bundle)){
			std::cerr << "Failed to shadow trace." << std::endl;
			exit(1);
		}

		prim_shadow_trace_time += tracer.get_shadow_exec_time();

		if (!ray_shader.shade(*ray_in, hit_bundle, si,
				      cubemap, framebuffer, tile_size)){
			std::cerr << "Failed to update framebuffer." << std::endl;
			exit(1);
		}
		shader_time += ray_shader.get_exec_time();
		

		int32_t sec_ray_count = tile_size;
		for (uint32_t i = 0; i < MAX_BOUNCE; ++i) {

			sec_ray_gen.generate(si, *ray_in, sec_ray_count, 
					     hit_bundle, *ray_out, &sec_ray_count);
			sec_gen_time += sec_ray_gen.get_exec_time();

			std::swap(ray_in,ray_out);

			if (!sec_ray_count)
				break;
			if (sec_ray_count == ray_bundle_1.count())
				std::cerr << "Max sec rays reached!\n";

			total_ray_count += sec_ray_count;

			// tracer.trace(si, sec_ray_count, 
			// 	     *ray_in, hit_bundle);
			tracer.trace(si, bvh_mem, sec_ray_count, 
				     *ray_in, hit_bundle, true);
			sec_trace_time += tracer.get_trace_exec_time();


			// tracer.shadow_trace(si, sec_ray_count, 
			// 		    *ray_in, hit_bundle);
			tracer.shadow_trace(si, bvh_mem, sec_ray_count, 
					    *ray_in, hit_bundle, true);
			sec_shadow_trace_time += tracer.get_shadow_exec_time();

			if (!ray_shader.shade(*ray_in, hit_bundle, si,
					      cubemap, framebuffer, sec_ray_count)){
				std::cerr << "Ray shader failed execution." << std::endl;
				exit(1);
			}
			shader_time += ray_shader.get_exec_time();
		}

	}

	if (!framebuffer.copy(cl_tex_mem)){
		std::cerr << "Failed to copy framebuffer." << std::endl;
		exit(1);
	}
	fb_copy_time = framebuffer.get_copy_exec_time();
	double total_msec = rt_time.msec_since_snap();

	if (min_time < 0)
		min_time = total_msec;
	else
		min_time = std::min(min_time,total_msec);

	if (max_time < 0)
		max_time = total_msec;
	else
		max_time = std::max(max_time,total_msec);

	////////////////// Immediate mode textured quad
        release_gl_tex(cl_tex_mem,clinfo);
	glBindTexture(GL_TEXTURE_2D, gl_tex);

	glBegin(GL_TRIANGLE_STRIP);

	glTexCoord2f(1.0,1.0);
	glVertex2f(1.0,1.0);

	glTexCoord2f(1.0,0.0);
	glVertex2f(1.0,-1.0);

	glTexCoord2f(0.0,1.0);
	glVertex2f(-1.0,1.0);

	glTexCoord2f(0.0,0.0);
	glVertex2f(-1.0,-1.0);

	glEnd();
	////////////////////////////////////////////

	i += dir;
	if (!(i % (STEPS-1))){
		dir *= -1;
	}		
	std::cout << "Time elapsed: " 
		  << total_msec << " milliseconds " 
		  << "\t" 
		  << (1000.f / total_msec)
		  << " FPS"
		  << "\t"
		  << total_ray_count
		  << " rays casted "
		  << "\t(" << pixel_count << " primary, " 
		  << total_ray_count-pixel_count << " secondary)"
		  << "               \r" ;
	std::flush(std::cout);
	rt_time.snap_time();
	total_ray_count = 0;

	std::cout << "\nPrim Gen time: \t" << prim_gen_time  << std::endl;
	std::cout << "Sec Gen time: \t" << sec_gen_time << std::endl;
	std::cout << "Tracer time: \t" << prim_trace_time + sec_trace_time  
		  << " (" <<  prim_trace_time << " - " << sec_trace_time 
		  << ")" << std::endl;
	std::cout << "Shadow time: \t" << prim_shadow_trace_time + sec_shadow_trace_time 
		  << " (" <<  prim_shadow_trace_time 
		  << " - " << sec_shadow_trace_time << ")" << std::endl;
	std::cout << "Shader time: \t " << shader_time << std::endl;
	std::cout << "Fb clear time: \t" << fb_clear_time << std::endl;
	std::cout << "Fb copy time: \t" << fb_copy_time << std::endl;
	std::cout << std::endl;

	glutSwapBuffers();
	current_frame = (current_frame+motion_rate)%frames;
	if (current_frame == 0) {
		double t = seq_time.msec_since_snap();
		     
		std::cout << "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=" << std::endl;
		std::cout << "Sequence stats:" << std::endl << std::endl;
		std::cout << "Frames: " << frames << std::endl;
		std::cout << "Sequence render time: " << t << "msec\t(" 
			  << frames*1000.f/t << " FPS)" <<   std::endl;
		std::cout << "Mean / Min / Max render time: " 
			  <<  t/frames << " / " 
			  <<  min_time << " / " 
			  <<  max_time << " msec" << std::endl;
		std::cout << "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=" << std::endl;
		max_time = -1;
		min_time = -1;
		seq_time.snap_time();
	}

}

int main (int argc, char** argv)
{

	/*---------------------- Initialize OpenGL and OpenCL ----------------------*/

	if (init_gl(argc,argv,&glinfo, window_size, "RT") != 0){
		std::cerr << "Failed to initialize GL" << std::endl;
		exit(1);
	} else { 
		std::cout << "Initialized GL succesfully" << std::endl;
	}

	if (init_cl(glinfo,&clinfo) != CL_SUCCESS){
		std::cerr << "Failed to initialize CL" << std::endl;
		exit(1);
	} else { 
		std::cout << "Initialized CL succesfully" << std::endl;
	}
	print_cl_info(clinfo);

	/*---------------------- Create shared GL-CL texture ----------------------*/
	gl_tex = create_tex_gl(window_size[0],window_size[1]);
	if (create_cl_mem_from_gl_tex(clinfo, gl_tex, &cl_tex_mem))
		exit(1);

	/*---------------------- Set up scene ---------------------------*/
	frames = 15;
	const uint32_t bridge_parts = 5;
	const std::string pack = "pack1OBJ";
	const int32_t visual_frame = 9;
	// const std::string pack = "pack2OBJ";
	double wave_attenuation = 1.f;

	std::vector<Scene> scene(frames);
	scene_info = std::vector<SceneInfo>(frames);

	/* -=-=-=-=-=-=-=- Flat grid bvh construction -=-=-=-=-=-=- */
	Scene flat_scene;
	SceneInfo flat_scene_info;
	std::stringstream flat_grid_path;
	flat_grid_path << "models/obj/" << pack << "/gridFluid" << 1 << ".obj";
	mesh_id flat_grid_mid = flat_scene.load_obj_file(flat_grid_path.str());
	object_id flat_grid_oid = flat_scene.geometry.add_object(flat_grid_mid);
	Object& flat_grid = flat_scene.geometry.object(flat_grid_oid);
	flat_grid.geom.setScale(makeVector(1.f,0.f,1.f));
	// flat_grid.slack = makeVector(0.f,0.9f,0.f);
	flat_grid.slack = makeVector(0.f,wave_attenuation * 0.9f,0.f);

	/* ---- Solids ------ */
	// std::stringstream visual_path;
	// visual_path << "models/obj/" << pack << "/visual" << visual_frame << ".obj";
	// mesh_id visual_mid = flat_scene.load_obj_file(visual_path.str());
	// object_id visual_oid = flat_scene.geometry.add_object(visual_mid);
	// Object& visual = flat_scene.geometry.object(visual_oid);
	// // visual.slack = makeVector(0.f,0.55f,0.f);

	// for (uint32_t j = 0; j < bridge_parts ; ++j) {
	// 	std::stringstream bridge_path;
	// 	bridge_path << "models/obj/" << pack << "/bridge" << j << visual_frame << ".obj";
	// 	mesh_id bridge_mid = flat_scene.load_obj_file(bridge_path.str());
	// 	object_id bridge_oid = flat_scene.geometry.add_object(bridge_mid);
	// 	Object& bridge = flat_scene.geometry.object(bridge_oid);
	// 	// bridge.slack = makeVector(0.f,5.55f,0.f);
	// }

	// mesh_id teapot_mesh_id = flat_scene.load_obj_file("models/obj/teapot2.obj");
	mesh_id teapot_mesh_id = flat_scene.load_obj_file("models/obj/teapot-low_res.obj");
	object_id teapot_obj_id = flat_scene.geometry.add_object(teapot_mesh_id);
	Object& teapot_obj = flat_scene.geometry.object(teapot_obj_id);
	teapot_obj.geom.setPos(makeVector(-1.f,0.f,0.f));
	teapot_obj.geom.setScale(makeVector(3.f,3.f,3.f));

	/* ------------------*/

	flat_grid.geom.setScale(makeVector(1.f,0.f,1.f));
	flat_scene.create_aggregate();
	rt_time.snap_time();
	// flat_scene.create_bvh_with_slack(makeVector(0.f,.55f,0.f));
	flat_scene.create_bvh();
	BVH& flat_scene_bvh   = flat_scene.get_aggregate_bvh();
	double bvh_build_time = rt_time.msec_since_snap();
	std::cout << "Created BVH succesfully (build time: " 
		  << bvh_build_time << " msec, " 
		  << flat_scene_bvh.nodeArraySize() << " nodes)." << std::endl;
	if (!flat_scene_info.initialize(flat_scene,clinfo)) {
		std::cerr << "Failed to initialize flat scene info." << std::endl;
		exit(1);
	}
	bvh_mem = flat_scene_info.bvh_mem();
	/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */


	for (uint32_t i = 0; i < frames ; ++i) {

		std::stringstream grid_path,visual_path;
		grid_path << "models/obj/" << pack << "/gridFluid" << i+1 << ".obj";
		mesh_id grid_mid = scene[i].load_obj_file(grid_path.str());
		object_id grid_oid = scene[i].geometry.add_object(grid_mid);
		Object& grid = scene[i].geometry.object(grid_oid);
		grid.mat.diffuse = White;
		grid.mat.reflectiveness = 0.95f;
		grid.mat.refractive_index = 1.5f;
		grid.geom.setScale(makeVector(1.f,wave_attenuation,1.f));

		/* ---- Solids ------ */
		// visual_path << "models/obj/" << pack << "/visual" << visual_frame << ".obj";
		// mesh_id visual_mid = scene[i].load_obj_file(visual_path.str());
		// object_id visual_oid = scene[i].geometry.add_object(visual_mid);
		// Object& visual = scene[i].geometry.object(visual_oid);
		// visual.mat.diffuse = Red;

		// for (uint32_t j = 0; j < bridge_parts ; ++j) {
		// 	std::stringstream bridge_path;
		// 	bridge_path << "models/obj/" << pack << "/bridge" << j << i+1 << ".obj";
		// 	mesh_id bridge_mid = scene[i].load_obj_file(bridge_path.str());
		// 	object_id bridge_oid = scene[i].geometry.add_object(bridge_mid);
		// 	Object& bridge = scene[i].geometry.object(bridge_oid);
		// 	bridge.mat.diffuse = Green;
		// }

		// mesh_id teapot_mesh_id = scene[i].load_obj_file("models/obj/teapot2.obj");
		mesh_id teapot_mesh_id = scene[i].load_obj_file("models/obj/teapot-low_res.obj");
		object_id teapot_obj_id = scene[i].geometry.add_object(teapot_mesh_id);
		Object& teapot_obj = scene[i].geometry.object(teapot_obj_id);
		teapot_obj.geom.setPos(makeVector(-1.f,0.f,0.f));
		teapot_obj.geom.setScale(makeVector(3.f,3.f,3.f));
		teapot_obj.mat.diffuse = Green;
		teapot_obj.mat.shininess = 1.f;
		// teapot_obj.mat.reflectiveness = 0.3f;

		/* ------------------*/

		
		scene[i].create_aggregate();
		Mesh& scene_mesh = scene[i].get_aggregate_mesh();

		scene[i].reorderTriangles(flat_scene_bvh.m_ordered_triangles);

		/*---------------------- Initialize SceneInfo ----------------------------*/
		if (!scene_info[i].initialize(scene[i],clinfo)) {
			std::cerr << "Failed to initialize scene info." << std::endl;
			exit(1);
		}
		std::cout << "Initialized scene info succesfully." << std::endl;

		/*---------------------- Print scene data ----------------------*/
		std::cerr << "Scene " << i << " stats: " << std::endl;
		std::cerr << "\tTriangle count: " << scene_mesh.triangleCount() << std::endl;
		std::cerr << "\tVertex count: " << scene_mesh.vertexCount() << std::endl;
		std::cerr << std::endl;
	}


	/*---------------------- Set initial Camera paramaters -----------------------*/
	camera.set(makeVector(0,3,-30), makeVector(0,0,1), makeVector(0,1,0), M_PI/4.,
		   window_size[0] / (float)window_size[1]);

	/*---------------------------- Set tile size ------------------------------*/
	best_tile_size = clinfo.max_compute_units * clinfo.max_work_item_sizes[0];
	best_tile_size *= 64;
	best_tile_size = std::min(pixel_count, best_tile_size);

	/*---------------------- Initialize ray bundles -----------------------------*/
	int32_t ray_bundle_size = best_tile_size * 3;

	if (!ray_bundle_1.initialize(ray_bundle_size, clinfo)) {
		std::cerr << "Error initializing ray bundle 1" << std::endl;
		std::cerr.flush();
		exit(1);
	}

	if (!ray_bundle_2.initialize(ray_bundle_size, clinfo)) {
		std::cerr << "Error initializing ray bundle 2" << std::endl;
		std::cerr.flush();
		exit(1);
	}
	std::cout << "Initialized ray bundles succesfully" << std::endl;

	/*---------------------- Initialize hit bundle -----------------------------*/
	int32_t hit_bundle_size = ray_bundle_size;

	if (!hit_bundle.initialize(hit_bundle_size, clinfo)) {
		std::cerr << "Error initializing hit bundle" << std::endl;
		std::cerr.flush();
		exit(1);
	}
	std::cout << "Initialized hit bundle succesfully" << std::endl;

	/*----------------------- Initialize cubemap ---------------------------*/
	if (!cubemap.initialize("textures/cubemap/Sky/posx.jpg",
				"textures/cubemap/Sky/negx.jpg",
				"textures/cubemap/Sky/posy.jpg",
				"textures/cubemap/Sky/negy.jpg",
				"textures/cubemap/Sky/posz.jpg",
				"textures/cubemap/Sky/negz.jpg",
				clinfo)) {
		std::cerr << "Failed to initialize cubemap." << std::endl;
		exit(1);
	}
	std::cerr << "Initialized cubemap succesfully." << std::endl;

	/*------------------------ Initialize FrameBuffer ---------------------------*/
	if (!framebuffer.initialize(clinfo, window_size)) {
		std::cerr << "Error initializing framebuffer." << std::endl;
		exit(1);
	}
	std::cout << "Initialized framebuffer succesfully." << std::endl;

	/* ------------------ Initialize ray tracer kernel ----------------------*/
	if (!tracer.initialize(clinfo)){
		std::cerr << "Failed to initialize tracer." << std::endl;
		return 0;
	}
	std::cerr << "Initialized tracer succesfully." << std::endl;


	/* ------------------ Initialize Primary Ray Generator ----------------------*/
	if (!prim_ray_gen.initialize(clinfo)) {
		std::cerr << "Error initializing primary ray generator." << std::endl;
		exit(1);
	}
	std::cout << "Initialized primary ray generator succesfully." << std::endl;


	/* ------------------ Initialize Secondary Ray Generator ----------------------*/
	if (!sec_ray_gen.initialize(clinfo)) {
		std::cerr << "Error initializing secondary ray generator." << std::endl;
		exit(1);
	}
	sec_ray_gen.set_max_rays(ray_bundle_1.count());
	std::cout << "Initialized secondary ray generator succesfully." << std::endl;

	/*------------------------ Initialize RayShader ---------------------------*/
	if (!ray_shader.initialize(clinfo)) {
		std::cerr << "Error initializing ray shader." << std::endl;
		exit(1);
	}
	std::cout << "Initialized ray shader succesfully." << std::endl;

	/*----------------------- Enable timing in all clases -------------------*/
	framebuffer.enable_timing(true);
	prim_ray_gen.enable_timing(true);
	sec_ray_gen.enable_timing(true);
	tracer.enable_timing(true);
	ray_shader.enable_timing(true);

	/*------------------------- Count mem usage -----------------------------------*/
	int32_t total_cl_mem = 0;
	total_cl_mem += pixel_count * 4; /* 4bpp texture */
	// for (uint32_t i = 0; i < frames; ++i)  
	// 	total_cl_mem += scene_info[i].size();
	total_cl_mem += cl_mem_size(ray_bundle_1.mem()) + cl_mem_size(ray_bundle_2.mem());
	total_cl_mem += cl_mem_size(hit_bundle.mem());
	total_cl_mem += cl_mem_size(cubemap.positive_x_mem()) * 6;
	total_cl_mem += cl_mem_size(framebuffer.image_mem());

	std::cout << "\nMemory stats: " << std::endl;
	std::cout << "\tTotal opencl mem usage: " 
		  << total_cl_mem/1e6 << " MB." << std::endl;
	// for (uint32_t i = 0; i < frames; ++i)  
	// 	std::cout << "\tScene " << i << " mem usage: " << scene_info[i].size()/1e6 << " MB." << std::endl;

	std::cout << "\tFramebuffer+Tex mem usage: " 
		  << (cl_mem_size(framebuffer.image_mem()) + pixel_count * 4)/1e6
		  << " MB."<< std::endl;
	std::cout << "\tCubemap mem usage: " 
		  << (cl_mem_size(cubemap.positive_x_mem())*6)/1e6 
		  << " MB."<< std::endl;
	std::cout << "\tRay mem usage: " 
		  << (cl_mem_size(ray_bundle_1.mem())*2)/1e6
		  << " MB."<< std::endl;
	std::cout << "\tRay hit info mem usage: " 
		  << cl_mem_size(hit_bundle.mem())/1e6
		  << " MB."<< std::endl;

        /* !! ---------------------- Test area ---------------- */
	std::cerr << std::endl;
	std::cerr << "Misc info: " << std::endl;

	std::cerr << "Tile size: " << best_tile_size << std::endl;
	std::cerr << "Tiles: " << pixel_count / (float)best_tile_size << std::endl;
	std::cerr << "color_cl size: "
		  << sizeof(color_cl)
		  << std::endl;
	std::cerr << "directional_light_cl size: "
		  << sizeof(directional_light_cl)
		  << std::endl;

	std::cerr << "ray_cl size: "
		  << sizeof(ray_cl)
		  << std::endl;
	std::cerr << "ray_plus_cl size: "
		  << sizeof(ray_plus_cl)
		  << std::endl;
	std::cerr << "ray_hit_info_cl size: "
		  << sizeof(ray_hit_info_cl)
		  << std::endl;

	/*------------------------ Set GLUT and misc functions -----------------------*/
	rt_time.snap_time();
	seq_time.snap_time();

	glutKeyboardFunc(gl_key);
	glutMotionFunc(gl_mouse);
	glutDisplayFunc(gl_loop);
	glutIdleFunc(gl_loop);
	glutMainLoop();	

	clinfo.release_resources();

	return 0;
}

