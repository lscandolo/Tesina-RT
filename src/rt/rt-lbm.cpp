#include <algorithm>
#include <iostream>

#include <cl-gl/opengl-init.hpp>
#include <cl-gl/opencl-init.hpp>
#include <rt/rt.hpp>

#define		GRID_SIZE_X	  100							// Width Of Our Water Height Map (NEW)
#define		GRID_SIZE_Y	  100							// Width Of Our Water Height Map (NEW)
#define		STEP_SIZE	  1							    // Width And Height Of Each Quad (NEW)
#define		HEIGHT_RATIO  1.5//1.5							// Ratio That The Y Is Scaled According To The X And Z (NEW)

//Fluid Module
extern "C" {
        void __declspec(dllimport) __stdcall LBM_initialize(int,int,double,double);
        void __declspec(dllimport) __stdcall LBM_update();
        double __declspec(dllimport) __stdcall LBM_getRho(int,int);
        void __declspec(dllimport) __stdcall LBM_addEvent(int,int,int,int,double);
        int __declspec(dllimport) __stdcall LBM_getWidth();
        int __declspec(dllimport) __stdcall LBM_getHeight();
}

int32_t MAX_BOUNCE = 5;
int32_t GPU_BVH_BUILD = 1;
#define WAVE_HEIGHT 0.1

Renderer renderer;

void rainEvent();


int loging_state = 0;

size_t frame = 0;

bool is_raining = false;
bool show_boat = false;

memory_id tex_id;
mesh_id   grid_id;
mesh_id   boat_mesh_id;
object_id boat_id;
std::vector<object_id> boat_ids;

Scene                 scene;

GLuint gl_tex;
rt_time_t rt_time;
Log rt_log;

#define STEPS 16

uint32_t drop_click(vec3 raypos, vec3 raydir, vec3 v0, vec3 v1, vec3 v2, bool invert)
{
        vec3 e1 = v1 - v0;
        vec3 e2 = v2 - v0;
                
        vec3 h = cross(raydir, e2);
        float  a = e1 * h;
                
        if (a > -1e-26f && a < 1e-26f)
                /* if (a > -0.000001f && a < 0.00001f) */
                return -1;
                
        float  f = 1.f/a;
        vec3 s = raypos - v0;
        float  u = f * (s * h);
        if (u < 0.f || u > 1.f)
                return -1;
                
        vec3 q = cross(s,e1);
        float  v = f * (raydir * q);
        if (v < 0.f || u+v > 1.f)
                return -1;
                
        float t = f * (e2 * q);
        bool t_is_within_bounds = (t > 0 );


        if (t_is_within_bounds) {
                //std::cout << "HIT!" << std::endl;
                //std::cout << "uv:" << u << " " << v << std::endl;
                if (invert) {
                        v = 1.f-v;
                        u = 1.f-u;
                }
                LBM_addEvent(v*GRID_SIZE_X       ,u*GRID_SIZE_Y,
                            (v+0.02f)*GRID_SIZE_X,(u+0.02f)*GRID_SIZE_Y,
                             0.035f);
                return 0;
        }


        return -1;


}

bool moving = false;
bool splashing = false;

void gl_mouse_click(int button, int state, int x, int y)
{

        if (button == GLUT_LEFT_BUTTON) {
                if (state == GLUT_DOWN)
                        moving = true;
                if (state == GLUT_UP)
                        moving = false;
                return;
        }

        if (button == GLUT_RIGHT_BUTTON) {
                if (state == GLUT_DOWN)
                        splashing = true;
                if (state == GLUT_UP)
                        splashing = false;

                float xPosNDC = ((float)x)/renderer.get_framebuffer_w();
                float yPosNDC = 1.f - ((float)y)/renderer.get_framebuffer_w();
                vec3 raydir = (scene.camera.dir + scene.camera.right * (xPosNDC * 2.0f - 1.0f) +
                        scene.camera.up    * (yPosNDC * 2.0f - 1.0f)).normalized();
                vec3 raypos = scene.camera.pos;
                //std::cout << "Ray dir: " << raydir << std::endl;
                
                const vec3 v0 = makeVector(100.f,0.f,100.f);
                const vec3 v1 = makeVector(100.f,0.f,-100.f);
                const vec3 v2 = makeVector(-100.f,0.f,100.f);
                const vec3 v3 = makeVector(-100.f,0.f,-100.f);

                drop_click(raypos, raydir, v0,v1,v2,true);
                drop_click(raypos, raydir, v3,v2,v1,false);
        }
}

void gl_mouse(int x, int y)
{
        if (splashing) {
                gl_mouse_click(GLUT_RIGHT_BUTTON,GLUT_DOWN,x, y);
        } else if (moving) {

                float delta = 0.005f;
                float d_inc = delta * (renderer.get_framebuffer_h() *0.5f - y);/* y axis points downwards */
                float d_yaw = delta * (x - renderer.get_framebuffer_w() *0.5f);

                float max_motion = 0.3f;

                d_inc = std::min(std::max(d_inc, -max_motion), max_motion);
                d_yaw = std::min(std::max(d_yaw, -max_motion), max_motion);

                if (d_inc == 0.f && d_yaw == 0.f)
                        return;

                scene.camera.modifyAbsYaw(d_yaw);
                scene.camera.modifyPitch(d_inc);
                glutWarpPointer(renderer.get_framebuffer_w() * 0.5f, 
                                renderer.get_framebuffer_h() * 0.5f);
        }
}

void gl_key(unsigned char key, int x, int y)
{
	float delta = 2.f;
    static vec3 boat_pos = makeVector(0.f,5.5f,0.f);
    Object& boat_obj = scene.object(boat_id);
	const pixel_sample_cl samples1[] = {{ 0.f , 0.f, 1.f}};
	const pixel_sample_cl samples4[] = {{ 0.25f , 0.25f, 0.25f},
				      { 0.25f ,-0.25f, 0.25f},
				      {-0.25f , 0.25f, 0.25f},
				      {-0.25f ,-0.25f, 0.25f}};

    switch (key){
    case '+':
            MAX_BOUNCE = std::min(MAX_BOUNCE+1, 10);
            break;
    case '-':
            MAX_BOUNCE = std::max(MAX_BOUNCE-1, 0);
            break;
    case 'a':
            scene.camera.panRight(-delta);
            break;
    case 's':
            scene.camera.panForward(-delta);
            break;
	case 'w':
            scene.camera.panForward(delta);
            break;
    case 'd':
            scene.camera.panRight(delta);
            break;
    case 'z':
            std::cout << "Pos: " << scene.camera.pos << std::endl;
            std::cout << "Dir: " << scene.camera.dir << std::endl;
            break;
    case 'q':
            std::cout << std::endl << "Exiting..." << std::endl;
            exit(1);
            break;
    case 'b':
            rt_log.silent = !rt_log.silent;
            break;
    case 'u':
            if (!show_boat)
                    break;
            boat_pos[2] += 1.f;
            for (uint32_t i = 0; i < boat_ids.size(); ++i) {
                    Object& boat_part = scene.object(boat_ids[i]);
                    boat_part.geom.setPos(boat_pos);
                    scene.update_mesh_vertices(boat_ids[i]);
            }
                    //scene.update_bvh_roots();
            break;
    case 'j':
            if (!show_boat)
                    break;
            boat_pos[2] -= 1.f;
            for (uint32_t i = 0; i < boat_ids.size(); ++i) {
                    Object& boat_part = scene.object(boat_ids[i]);
                    boat_part.geom.setPos(boat_pos);
                    scene.update_mesh_vertices(boat_ids[i]);
            }

            //boat_obj.geom.setPos(boat_pos);
            //scene.update_mesh_vertices(boat_mesh_id);
            //scene.update_bvh_roots();
            break;
    case 'h':
            if (!show_boat)
                    break;
            boat_pos[0] -= 1.f;
            for (uint32_t i = 0; i < boat_ids.size(); ++i) {
                    Object& boat_part = scene.object(boat_ids[i]);
                    boat_part.geom.setPos(boat_pos);
                    scene.update_mesh_vertices(boat_ids[i]);
            }
            //boat_obj.geom.setPos(boat_pos);
            //scene.update_mesh_vertices(boat_mesh_id);
            //scene.update_bvh_roots();
            break;
    case 'k':
            if (!show_boat)
                    break;
            boat_pos[0] += 1.f;
            for (uint32_t i = 0; i < boat_ids.size(); ++i) {
                    Object& boat_part = scene.object(boat_ids[i]);
                    boat_part.geom.setPos(boat_pos);
                    scene.update_mesh_vertices(boat_ids[i]);
            }
            //boat_obj.geom.setPos(boat_pos);
            //scene.update_mesh_vertices(boat_mesh_id);
            //scene.update_bvh_roots();
            break;
    case 'e':
            LBM_addEvent(50,50,53,53,0.035f);
            break;
    case 'r':
            is_raining = !is_raining;
            break;
    case '1': /* Set 1 sample per pixel */
            if (renderer.set_samples_per_pixel(1,samples1)){
                    std::cerr << "Error seting spp" << "\n";
                    pause_and_exit(1);
            }
            break;
    case '4': /* Set 4 samples per pixel */
            if (renderer.set_samples_per_pixel(4,samples4)){
                    std::cerr << "Error seting spp" << "\n";
                    pause_and_exit(1);
            }
            break;
    }
}


void gl_loop()
{
	static int i = 0;
	static cl_float mangler_arg = 0;
	static cl_float last_mangler_arg = -1000;
	static int dir = 1;
	static int total_ray_count = 0;

    double bvh_build_time = 0;
    double prim_gen_time = 0;
	double sec_gen_time = 0;
	double prim_trace_time = 0;
	double sec_trace_time = 0;
	double prim_shadow_trace_time = 0;
	double sec_shadow_trace_time = 0;
	double shader_time = 0;
	double fb_clear_time = 0;
	double fb_copy_time = 0;

    DeviceInterface* device = DeviceInterface::instance();

	cl_int arg = i%STEPS;

	mangler_arg += 1.f;
	rt_time_t mangle_timer;

	if (loging_state){
		if(loging_state == 1){
			rt_log << "scene.camera configuration 'o' " << std::endl;
			scene.camera.set(makeVector(0,3,-30), makeVector(0,0,1), makeVector(0,1,0), M_PI/4.,
                                   renderer.get_framebuffer_w() / (float)renderer.get_framebuffer_h());
		} else if (loging_state == 3){
			rt_log << "scene.camera configuration 'i' " << std::endl;
			scene.camera.set(makeVector(0,5,-30), makeVector(0,-0.5,1), makeVector(0,1,0), M_PI/4.,
                                   renderer.get_framebuffer_w() / (float)renderer.get_framebuffer_h());
		} else if (loging_state == 5) {
			rt_log << "scene.camera configuration 'k' " << std::endl;
			scene.camera.set(makeVector(0,25,-60), makeVector(0,-0.5,1), makeVector(0,1,0), M_PI/4.,
                                   renderer.get_framebuffer_w() / (float)renderer.get_framebuffer_h());
		} else if (loging_state == 7) {
			rt_log << "scene.camera configuration 'l' " << std::endl;
			scene.camera.set(makeVector(0,120,0), makeVector(0,-1,0.01), makeVector(0,1,0), M_PI/4.,
                                   renderer.get_framebuffer_w() / (float)renderer.get_framebuffer_h());
		} else if (loging_state == 9) {
			rt_log.enabled = false;
			rt_log.silent = true;
            scene.camera.set(makeVector(0,3,-30), makeVector(0,0,1), makeVector(0,1,0), M_PI/4.,
                                            renderer.get_framebuffer_w() / (float)renderer.get_framebuffer_h());
            std::cout << "Done loging!"	<< std::endl;
            loging_state = 0;
		}
		if (loging_state)
			loging_state++;
	}

	/*-------------- Mangle verts in CPU ---------------------*/
	mangle_timer.snap_time();
	// Create new rain drop and update the lbm simulator
    if (is_raining)
            rainEvent();
	LBM_update();
	/////
	Mesh& mesh = scene.get_mesh(grid_id);
    for (size_t v = 0; v < mesh.vertexCount(); ++ v) {
		Vertex& vert = mesh.vertex(v);
		float x = (vert.position.s[0] - 5.f)/10.f;
		float z = (vert.position.s[2] - 5.f)/10.f;
		
		x = (x + 1) * GRID_SIZE_X;
		z = (z + 1) * GRID_SIZE_Y;
		
		if (x < 1.f)
			x = 1.f;
		if (z < 1.f)
			z = 1.f;

		//const float DROP_HEIGHT = 18.f;
		const float DROP_HEIGHT = 5.f;
		
		float y = LBM_getRho((int)x,(int)z);
		
		y = y - 0.05f;
		y = y * DROP_HEIGHT;
		
		vert.position.s[1] = y;

		vec3 normalx = makeVector(0.f,1.f,0.f);
        vec3 normalz = makeVector(0.f,1.f,0.f);
        
        normalx[0] = -10.f*(LBM_getRho(x+1,z) - LBM_getRho(x-1,z))  * 0.5f * DROP_HEIGHT;
		normalz[2] = -10.f*(LBM_getRho(x,z+1) - LBM_getRho(x,z-1))  * 0.5f * DROP_HEIGHT;
        //normalx[0] = std::min(std::max(normalx[0],-10.f),10.f);
        //normalz[2] = std::min(std::max(normalz[2],-10.f),10.f);
        vec3 normal = (normalx.normalized() + normalz.normalized());

		vert.normal = vec3_to_float3(normal.normalized());
		
		//if (fabsf(y) > WAVE_HEIGHT)
		//	std::cerr << "y: " << y << std::endl;
	}
    scene.update_mesh_vertices(grid_id);

    /*--------------------------------------*/
        double mangle_time = mangle_timer.msec_since_snap();
        //std::cerr << "Simulation time: "  << mangle_time << " msec." << std::endl;
        last_mangler_arg = mangler_arg;
	
        directional_light_cl light;
        //light.set_dir(0.05f * (arg - 8.f) , -0.6f, 0.2f);
        light.set_dir(0.05f * (1.f - 8.f) , -0.6f, 0.2f);
        //light.set_color(0.05f * (fabsf(arg)) + 0.1f, 0.2f, 0.05f * fabsf(arg+4.f));
        light.set_color(0.8f,0.8f,0.8f);
        scene.set_dir_light(light);
        color_cl ambient;
        ambient[0] = ambient[1] = ambient[2] = 0.1f;
        scene.set_ambient_light(ambient);

         //spot_light_cl spot;
         //spot.set_dir(0.05f,-1.f,-0.02f);
         //spot.set_pos(0.0f,15.f,0.0f);
         //spot.set_angle(M_PI/4.f);
         //spot.set_color(0.7f,0.7f,0.7f);
         //scene.set_spot_light(spot);

        //Acquire graphics library resources, clear framebuffer, 
        // clear counters and create bvh if needed
        if (renderer.set_up_frame(tex_id ,scene)) {
                std::cerr << "Error in setting up frame" << "\n";
                exit(-1);
        }

        renderer.render_to_framebuffer(scene);//!!
        
        renderer.copy_framebuffer();//!!

        if (renderer.conclude_frame(scene)) {
                std::cerr << "Error concluding frame" << "\n";
                exit(-1);
        }

        double total_msec = rt_time.msec_since_snap();

        ////////////////// Immediate mode textured quad
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
        rt_log<< "Time elapsed: " 
              << total_msec << " milliseconds " 
              << "\t" 
              << (1000.f / total_msec)
              << " FPS"
              << "\t"
              << total_ray_count
              << " rays casted "
              //<< "\t(" << pixel_count << " primary, " 
              //<< total_ray_count-pixel_count << " secondary)"
              << std::endl;
        if (rt_log.silent)
                std::cout<< "Time elapsed: "
                         << total_msec << " milliseconds "
                         << "\t"
                         << (1000.f / total_msec)
                         << " FPS"
                         << "                \r";
        std::flush(std::cout);
        rt_time.snap_time();
        total_ray_count = 0;

        if (frame == 10) 
                renderer.log.enabled = true;

        const FrameStats& stats = renderer.get_frame_stats();

        renderer.log << "Resolution:\t" << renderer.get_framebuffer_w() << "x" 
                     << renderer.get_framebuffer_h() << "\n";

        renderer.log << "Max ray bounces:\t" << renderer.get_max_bounces() << "\n";

        renderer.log << "\nBVH Build mean time:\t" << stats.get_stage_mean_time(BVH_BUILD) << "\n";
        renderer.log << "Prim Gen mean time: \t" << stats.get_stage_mean_time(PRIM_RAY_GEN)<< "\n";
        renderer.log << "Sec Gen mean time: \t" << stats.get_stage_mean_time(SEC_RAY_GEN)  << "\n";
        renderer.log << "Tracer mean time: \t" << stats.get_stage_mean_time(PRIM_TRACE) + stats.get_stage_mean_time(SEC_TRACE)
                << " (" <<  stats.get_stage_mean_time(PRIM_TRACE) << " - " << stats.get_stage_mean_time(SEC_TRACE)
                << ")" << "\n";
        renderer.log << "Shadow mean time: \t" << stats.get_stage_mean_time(PRIM_SHADOW_TRACE) + stats.get_stage_mean_time(SEC_SHADOW_TRACE)
                << " (" <<  stats.get_stage_mean_time(PRIM_SHADOW_TRACE)
                << " - " << stats.get_stage_mean_time(SEC_SHADOW_TRACE) << ")" << "\n";
        renderer.log << "Shader mean time: \t" << stats.get_stage_mean_time(SHADE) << "\n";
        renderer.log << "Fb clear mean time: \t" << stats.get_stage_mean_time(FB_CLEAR) << "\n";
        renderer.log << "Fb copy mean time: \t" << stats.get_stage_mean_time(FB_COPY) << "\n";
        renderer.log << std::endl;

        if (frame == 10) 
                renderer.log.enabled = false;


        glRasterPos2f(-0.95f,0.9f);
        std::stringstream ss;
        ss << "FPS: " << (1000.f / stats.get_frame_time());
        std::string fps_s = ss.str();
        for (uint32_t i = 0; i < fps_s.size(); ++i)
                glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *(fps_s.c_str()+i));


        glutSwapBuffers();

        frame++;

}

int main (int argc, char** argv)
{

        /*---------------------- Attempt to read INI File --------------------------*/

        renderer.initialize_from_ini_file("rt.ini");

        INIReader ini;
        int32_t ini_err;
        ini_err = ini.load_file("rt.ini");
        if (ini_err < 0)
                std::cout << "Error at ini file line: " << -ini_err << std::endl;
        else if (ini_err > 0)
                std::cout << "Unable to open ini file" << std::endl;

        size_t window_size[] = {renderer.get_framebuffer_w(), renderer.get_framebuffer_h()};
        size_t pixel_count = window_size[0] * window_size[1];

        rt_log.enabled = true;
        rt_log.silent = false;

	/*---------------------- Initialize OpenGL and OpenCL ----------------------*/

        // Initialize OpenGL and OpenCL
        GLInfo* glinfo = GLInfo::instance();

        if (glinfo->initialize(argc,argv, window_size, "RT-LBM")){
                std::cerr << "Failed to initialize GL" << std::endl;
                pause_and_exit(1);
        } else { 
                std::cout << "Initialized GL succesfully" << std::endl;
        }

        CLInfo* clinfo = CLInfo::instance();

        if (clinfo->initialize(2) != CL_SUCCESS){
                std::cerr << "Failed to initialize CL" << std::endl;
                pause_and_exit(1);
        } else { 
                std::cout << "Initialized CL succesfully" << std::endl;
        }
        clinfo->print_info();

    // Initialize device interface and generic gpu library
    DeviceInterface* device = DeviceInterface::instance();
    if (device->initialize()) {
            std::cerr << "Failed to initialize device interface" << std::endl;
            pause_and_exit(1);
    }

    if (DeviceFunctionLibrary::instance()->initialize()) {
            std::cerr << "Failed to initialize function library" << std::endl;
            pause_and_exit(1);
    }

    /*---------------------- Initialize lbm simulator ----------------------*/
    LBM_initialize(GRID_SIZE_X, GRID_SIZE_Y, 0.05f, 0.77f);
    //LBM_initialize(GRID_SIZE_X, GRID_SIZE_Y, 0.03f, 0.5f);
        //LBM_initialize(GRID_SIZE_X, GRID_SIZE_Y, 0.05f, 0.77f);
	//LBM_initialize(GRID_SIZE_X, GRID_SIZE_Y, 0.01f, 0.2f);

	/*---------------------- Create shared GL-CL texture ----------------------*/
	gl_tex = create_tex_gl(window_size[0],window_size[1]);
	tex_id = device->new_memory();
	DeviceMemory& tex_mem = device->memory(tex_id);
	if (tex_mem.initialize_from_gl_texture(gl_tex)) {
		std::cerr << "Failed to create memory object from gl texture" << std::endl;
		pause_and_exit(1);
	}

	/*---------------------- Set up scene ---------------------------*/
	if (scene.initialize()) {
		std::cerr << "Failed to initialize scene." << std::endl;
		pause_and_exit(1);
	}
	std::cout << "Initialized scene succesfully." << std::endl;

    /////////// Water
    mesh_id floor_mesh_id = scene.load_obj_file_as_aggregate("models/obj/grid100.obj");
    object_id floor_obj_id  = scene.add_object(floor_mesh_id);
	Object& floor_obj = scene.object(floor_obj_id);
    floor_obj.geom.setPos(makeVector(0.f,0.f,0.f));
 	floor_obj.geom.setScale(20.f);
	floor_obj.mat.diffuse = White;
	floor_obj.mat.reflectiveness = 0.95f;
	floor_obj.mat.refractive_index = 1.5f;
    /*Set slack for bvh*/
    vec3 slack = makeVector(0.f,WAVE_HEIGHT,0.f);
    scene.get_mesh(floor_mesh_id).set_global_slack(slack);


    ///////////// Boat
    if (show_boat) {
            std::vector<mesh_id> boat_parts =  scene.load_obj_file(
                    // "models/obj/NYERSEY/NYERSEY.obj");
                    "models/obj/ROMANSHP/ROMANSHP2.obj");
            boat_ids = scene.add_objects(boat_parts);
            for (uint32_t i = 0 ; i < boat_parts.size(); ++i){
                    Object& part = scene.object(boat_ids[i]);
                    part.geom.setScale(0.017f);
                    part.geom.setPos(makeVector(0.f,5.5f,0.f));
                    part.geom.setRpy(makeVector(M_PI/2.f,-M_PI/2.f,0.f));
            }
    }

    //boat_mesh_id = scene.load_obj_file_as_aggregate("models/obj/frame_boat1.obj");
    //object_id boat_obj_id = scene.add_object(boat_mesh_id);
    //Object& boat_obj = scene.object(boat_obj_id);
    //boat_id = boat_obj_id; 
    ////boat_obj.geom.setPos(makeVector(0.f,-9.f,0.f));
    //boat_obj.geom.setPos(makeVector(0.f,-0.5f,0.f));
    //boat_obj.geom.setRpy(makeVector(0.0f,0.f,0.f));
    //boat_obj.geom.setScale(2.f);
    //boat_obj.mat.diffuse = Red;
    //boat_obj.mat.shininess = 0.3f;
    //boat_obj.mat.reflectiveness = 0.0f;

         if (scene.create_aggregate_mesh()) { 
                std::cerr << "Failed to create aggregate mesh" << std::endl;
                pause_and_exit(1);
         } else {
                 std::cout << "Created aggregate mesh succesfully" << std::endl;
         }

         scene.set_accelerator_type(BVH_ACCELERATOR);

         clinfo->set_sync(false);

         if (GPU_BVH_BUILD) {
                 if (scene.transfer_aggregate_mesh_to_device()) {
                         std::cerr << "Failed to transfer aggregate mesh to device memory" 
                                   << std::endl;
                         pause_and_exit(1);
                 } else {
                         std::cout << "Transfered aggregate mesh to device succesfully" 
                                   << std::endl;
                 }

         } else {

                 if (scene.create_bvhs()) {
                         std::cerr << "Failed to create bvhs." << std::endl;
                         pause_and_exit(1);
                 }
                 std::cout << "Created bvhs" << std::endl;

                 if (scene.transfer_meshes_to_device()) {
                         std::cerr << "Failed to transfer meshes to device." 
                                 << std::endl;
                         pause_and_exit(1);
                 }
                 std::cout << "Transfered meshes to device" << std::endl;

                 if (scene.transfer_bvhs_to_device()) {
                         std::cerr << "Failed to transfer bvhs to device." 
                                 << std::endl;
                         pause_and_exit(1);
                 }
                 std::cout << "Transfered bvhs to device" << std::endl;
                 grid_id = floor_mesh_id;
         }


	/*---------------------- Set initial scene.camera paramaters -----------------------*/
    scene.camera.set(makeVector(-28,62,-179), makeVector(0.095,-0.417,0.903),
            makeVector(0,1,0), M_PI/4., window_size[0] / (float)window_size[1]);
	//scene.camera.set(makeVector(0,3,-30), makeVector(0,0,1), makeVector(0,1,0), M_PI/4.,
	//	   window_size[0] / (float)window_size[1]);

	/*----------------------- Initialize scene.cubemap ---------------------------*/
	
	if (scene.cubemap.initialize("textures/cubemap/Path/posx.jpg",
                               "textures/cubemap/Path/negx.jpg",
                               "textures/cubemap/Path/posy.jpg",
                               "textures/cubemap/Path/negy.jpg",
                               "textures/cubemap/Path/posz.jpg",
                               "textures/cubemap/Path/negz.jpg")) {
		std::cerr << "Failed to initialize cubemap." << std::endl;
		pause_and_exit(1);
	}
	std::cerr << "Initialized scene.cubemap succesfully." << std::endl;


	/*---------------------- Print scene data ----------------------*/
	Mesh& scene_mesh = scene.get_aggregate_mesh();
	std::cerr << "\nScene stats: " << std::endl;
	std::cerr << "\tTriangle count: " << scene_mesh.triangleCount() << std::endl;
	std::cerr << "\tVertex count: " << scene_mesh.vertexCount() << std::endl;


        /* !! ---------------------- Test area ---------------- */
	std::cerr << std::endl;
	std::cerr << "Misc info: " << std::endl;

	std::cerr << "color_cl size: "
		  << sizeof(color_cl)
		  << std::endl;
	std::cerr << "directional_light_cl size: "
		  << sizeof(directional_light_cl)
		  << std::endl;

	std::cerr << "ray_cl size: "
		  << sizeof(ray_cl)
		  << std::endl;
	std::cerr << "sample_cl size: "
		  << sizeof(sample_cl)
		  << std::endl;
	std::cerr << "sample_trace_info_cl size: "
		  << sizeof(sample_trace_info_cl)
		  << std::endl;

	/*------------------------ Set GLUT and misc functions -----------------------*/
	rt_time.snap_time();
	rt_log.enabled = false;
	rt_log.silent = true;

    /* Initialize renderer */
    renderer.initialize("rt-lbm-log");
    int32_t max_bounces = 9;
    ini.get_int_value("RT", "max_bounces", max_bounces);
    max_bounces = std::min(std::max(max_bounces, 0), 9);
    renderer.set_max_bounces(max_bounces);
    renderer.log.silent = true;

    glutKeyboardFunc(gl_key);
    glutMotionFunc(gl_mouse);
	glutMouseFunc(gl_mouse_click);
	glutDisplayFunc(gl_loop);
	glutIdleFunc(gl_loop);
	glutMainLoop();	

	clinfo->release_resources();

	return 0;
}


void rainEvent(){
	int x,y,size;
	int XMin = 3;
	int XMax = GRID_SIZE_X-5;
	
	int YMin = 3;
	int YMax = GRID_SIZE_Y-5;

	double r = double(rand())/(double(RAND_MAX)+1.0); // random double in range 0.0 to 1.0 (non inclusive)
        x = int(XMin + r*(XMax - XMin)); // transform to wanted range
    
	r = double(rand())/(double(RAND_MAX)+1.0); // random double in range 0.0 to 1.0 (non inclusive)
	y = int(YMin + r*(YMax - YMin)); // transform to wanted range
	
	XMin = 1;
	XMax = 5;

        size = int(XMin + r*(XMax - XMin)); // transform to wanted range

        LBM_addEvent(x,y,x+size,y+size,0.035f);
	//LBM_addEvent(x,y,x+size,y+size,0.035f);
}
