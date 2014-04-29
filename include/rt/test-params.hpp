/* Boat */
vec3 boat_stats_camera_pos[] = {makeVector(20.0186, -5.49632, 71.8718) ,
				makeVector(-56.9387, -2.41959, 29.574) ,
				makeVector(68.6859, 5.18034, 13.6691) };
vec3 boat_stats_camera_dir[] = {makeVector(0.131732, 0.0845093, -0.987677),
				makeVector(0.927574, -0.22893, -0.295292) ,
				makeVector(-0.820819, -0.478219, -0.31235) };

void boat_set_scene(Scene& scene, size_t* window_size){

	mesh_id floor_mesh_id = 
                scene.load_obj_file_as_aggregate("models/obj/frame_water1.obj");
	//scene.load_obj_file_as_aggregate("models/obj/grid200.obj");
	object_id floor_obj_id  = scene.add_object(floor_mesh_id);
	Object& floor_obj = scene.object(floor_obj_id);
	floor_obj.geom.setScale(2.f);
	floor_obj.geom.setPos(makeVector(0.f,-8.f,0.f));
	floor_obj.mat.diffuse = White;
	floor_obj.mat.diffuse = Blue;
	floor_obj.mat.reflectiveness = 0.98f;
	floor_obj.mat.refractive_index = 1.333f;

	mesh_id boat_mesh_id = 
		scene.load_obj_file_as_aggregate("models/obj/frame_boat1.obj");
	object_id boat_obj_id = scene.add_object(boat_mesh_id);
	Object& boat_obj = scene.object(boat_obj_id);
	boat_obj.geom.setPos(makeVector(0.f,-8.f,0.f));
	boat_obj.geom.setRpy(makeVector(0.f,0.f,0.f));
	boat_obj.geom.setScale(2.f);
	//boat_obj.geom.setScale(0.005f);
	boat_obj.mat.diffuse = Red;
	boat_obj.mat.shininess = 1.f;
	boat_obj.mat.reflectiveness = 0.0f;

	directional_light_cl light;
	light.set_dir(0.05f, -1.f, -0.02f);
	light.set_color(0.7f,0.7f,0.7f);
	scene.set_dir_light(light);

	color_cl ambient;
	ambient[0] = ambient[1] = ambient[2] = 0.2f;
	scene.set_ambient_light(ambient);

        scene.camera.set(boat_stats_camera_pos[0],//pos 
                         boat_stats_camera_dir[0],//dir
                         makeVector(0,1,0), //up
                         M_PI/4.,
                         window_size[0] / (float)window_size[1]);
}

LinearCameraTrajectory boat_cam_traj;
void boat_set_cam_traj(){
        boat_cam_traj.set_look_at_pos(makeVector(30.f,0.f,0.f));
        boat_cam_traj.set_up_vec(makeVector(0.f,1.f,0.f));
        boat_cam_traj.set_waypoints(makeVector(50.f,30.f,-45.f),
                makeVector(-50.f,30.f,-15.f));
        boat_cam_traj.set_frames(100);
}


/* Hand */
vec3 hand_stats_camera_pos[] = {makeVector(-0.542773, 0.851275, 0.193901) ,
				makeVector(0.496716, 0.56714, 0.721966) ,
				makeVector(-0.563173, 1.41939, -0.134797) };
vec3 hand_stats_camera_dir[] = {makeVector(0.658816, -0.55455, -0.508365),
				makeVector(-0.472905, -0.0779194, -0.877661) ,
				makeVector(0.502332, -0.860415, -0.0857217) };

void hand_set_scene(Scene& scene, size_t* window_size){

	scene.load_obj_file_and_make_objs("models/obj/hand/hand_40.obj");

	directional_light_cl light;
	light.set_dir(0.05f, -1.f, -0.02f);
	light.set_color(0.7f,0.7f,0.7f);
	scene.set_dir_light(light);

	color_cl ambient;
	ambient[0] = ambient[1] = ambient[2] = 0.2f;
	scene.set_ambient_light(ambient);

        scene.camera.set(hand_stats_camera_pos[0],//pos 
                         hand_stats_camera_dir[0],//dir
                         makeVector(0,1,0), //up
                         M_PI/4.,
                         window_size[0] / (float)window_size[1]);
}

LinearCameraTrajectory hand_cam_traj;
void hand_set_cam_traj(){
        hand_cam_traj.set_look_at_pos(makeVector(0.05f,0.5f,-0.3f));
        hand_cam_traj.set_up_vec(makeVector(0.f,1.f,0.f));
        hand_cam_traj.set_waypoints(makeVector(-0.5f,0.7f,-1.4f),
                makeVector(-1.f,0.6f,0.25f));
        hand_cam_traj.set_frames(100);
}

/* Ben */
vec3 ben_stats_camera_pos[] = {makeVector(-0.126448, 0.546994, 0.818811) ,
			       makeVector(0.231515, 0.555109, 0.274977) ,
			       makeVector(0.198037, 0.799047, 0.392632) };
vec3 ben_stats_camera_dir[] = {makeVector(0.157556,-0.121695, -0.979983),
			       makeVector(-0.745308, 0.139962, -0.651864) ,
			       makeVector(-0.329182, -0.636949, -0.697091) };

void ben_set_scene(Scene& scene, size_t* window_size){

	scene.load_obj_file_and_make_objs("models/obj/ben/ben_00.obj");

	directional_light_cl light;
	light.set_dir(0.05f, -1.f, -0.02f);
	light.set_color(0.7f,0.7f,0.7f);
	scene.set_dir_light(light);

	color_cl ambient;
	ambient[0] = ambient[1] = ambient[2] = 0.2f;
	scene.set_ambient_light(ambient);

        scene.camera.set(ben_stats_camera_pos[0],//pos 
                         ben_stats_camera_dir[0],//dir
                         makeVector(0,1,0), //up
                         M_PI/4.,
                         window_size[0] / (float)window_size[1]);
}

LinearCameraTrajectory ben_cam_traj;
void ben_set_cam_traj(){
        ben_cam_traj.set_look_at_pos(makeVector(0.015f,0.6f,0.09f));
        ben_cam_traj.set_up_vec(makeVector(0.f,1.f,0.f));
        ben_cam_traj.set_waypoints(makeVector(-0.5f,0.6f,0.45f),
                makeVector(0.5f,0.6f,0.45f));
        ben_cam_traj.set_frames(100);
}

/* Fairy */
vec3 fairy_stats_camera_pos[] = {makeVector(-0.118154, 0.417017, 1.14433) ,
				 makeVector(-0.464505, 0.717691, 0.279553) ,
				 makeVector(0.560874, 3.02102, 2.22842) };
vec3 fairy_stats_camera_dir[] = {makeVector(0.0700268, -0.095778, -0.992936),
				 makeVector(0.666486, -0.576098, -0.473188) ,
				 makeVector(-0.149838, -0.788657, -0.596295) };
				 
void fairy_set_scene(Scene& scene, size_t* window_size){
	
	scene.load_obj_file_and_make_objs("models/obj/fairy_forest/f000.obj");

	directional_light_cl light;
	light.set_dir(0.05f, -1.f, -0.02f);
	light.set_color(0.7f,0.7f,0.7f);
	scene.set_dir_light(light);

	color_cl ambient;
	ambient[0] = ambient[1] = ambient[2] = 0.2f;
	scene.set_ambient_light(ambient);

        scene.camera.set(fairy_stats_camera_pos[0],//pos 
                         fairy_stats_camera_dir[0],//dir
                         makeVector(0,1,0), //up
                         M_PI/4.,
                         window_size[0] / (float)window_size[1]);
}

LinearCameraTrajectory fairy_cam_traj;
void fairy_set_cam_traj(){
        fairy_cam_traj.set_look_at_pos(makeVector(0.f,0.f,0.f));
        fairy_cam_traj.set_up_vec(makeVector(0.f,1.f,0.f));
        fairy_cam_traj.set_waypoints(makeVector(-0.118154, 0.417017, 1.14433),
                makeVector(0.560874, 3.02102, 2.22842));
        fairy_cam_traj.set_frames(100);
}

/* Dragon */
vec3 dragon_stats_camera_pos[] = {makeVector(-39.9102, 9.42978, 55.1501) ,
				  makeVector(-1.1457, -0.774464, 49.4609) ,
				  makeVector(16.6885, 22.435, 25.0718) };

vec3 dragon_stats_camera_dir[] = {makeVector(0.558171, -0.165305, -0.813093),
				  makeVector(-0.00931118, -0.14159, -0.989882) ,
				  makeVector(-0.402962, -0.631356, -0.66258) };

void dragon_set_scene(Scene& scene, size_t* window_size){

	mesh_id floor_mesh_id = 
                scene.load_obj_file_as_aggregate("models/obj/frame_water1.obj");
	object_id floor_obj_id  = scene.add_object(floor_mesh_id);
	Object& floor_obj = scene.object(floor_obj_id);
	floor_obj.geom.setScale(2.f);
	floor_obj.geom.setPos(makeVector(0.f,-8.f,0.f));
	floor_obj.mat.diffuse = Blue;
	floor_obj.mat.reflectiveness = 0.9f;
	floor_obj.mat.refractive_index = 1.333f;

	mesh_id dragon_mesh_id = 
		scene.load_obj_file_as_aggregate("models/obj/dragon.obj");
	object_id dragon_obj_id = scene.add_object(dragon_mesh_id);
	Object& dragon_obj = scene.object(dragon_obj_id);
	dragon_obj.geom.setPos(makeVector(0.f,-8.f,0.f));
	dragon_obj.geom.setRpy(makeVector(0.f,0.f,0.f));
	dragon_obj.geom.setScale(2.f);
	dragon_obj.mat.diffuse = Red;
	dragon_obj.mat.shininess = 0.1f;
	dragon_obj.mat.reflectiveness = 0.0f;
	dragon_obj.mat.shininess = 1.f;
	dragon_obj.mat.reflectiveness = 0.7f;

	directional_light_cl light;
	light.set_dir(0.05f, -1.f, -0.02f);
	light.set_color(0.7f,0.7f,0.7f);
	scene.set_dir_light(light);

	color_cl ambient;
	ambient[0] = ambient[1] = ambient[2] = 0.2f;
	scene.set_ambient_light(ambient);

        scene.camera.set(dragon_stats_camera_pos[0],//pos 
                         dragon_stats_camera_dir[0],//dir
                         makeVector(0,1,0), //up
                         M_PI/4.,
                         window_size[0] / (float)window_size[1]);
}

LinearCameraTrajectory dragon_cam_traj;
void dragon_set_cam_traj(){
        dragon_cam_traj.set_look_at_pos(makeVector(0.f,-3.f,0.f));
        dragon_cam_traj.set_up_vec(makeVector(0.f,1.f,0.f));
        dragon_cam_traj.set_waypoints(makeVector(-20.f,30.f,-30.f),
                makeVector(30.f,30.f,-60.f));
        dragon_cam_traj.set_frames(100);
}

/* Buddha */
vec3 buddha_stats_camera_pos[] = {makeVector(0.741773, -1.754, 4.95699) ,
				  makeVector(-56.9387, -2.41959, 29.574) ,
				  makeVector(68.6859, 5.18034, 13.6691) };
vec3 buddha_stats_camera_dir[] = {makeVector(-0.179715, -0.0878783, -0.979786),
				  makeVector(0.927574, -0.22893, -0.295292) ,
				  makeVector(-0.820819, -0.478219, -0.31235) };

void buddha_set_scene(Scene& scene, size_t* window_size){
	std::vector<mesh_id> box_meshes = 
		scene.load_obj_file("models/obj/box-no-ceil.obj");
	std::vector<object_id> box_objs = scene.add_objects(box_meshes);

	for (uint32_t i = 0; i < box_objs.size(); ++i) {
		Object& obj = scene.object(box_objs[i]);
		obj.geom.setRpy(makeVector(0.f,0.f,0.4f));
		if (obj.mat.texture > 0)
			obj.mat.reflectiveness = 0.8f;
		// obj.geom.setPos(makeVector(0.f,-30.f,0.f));
	}
	mesh_id buddha_mesh_id = 
		scene.load_obj_file_as_aggregate("models/obj/buddha.obj");
	object_id buddha_obj_id = scene.add_object(buddha_mesh_id);
	Object& buddha_obj = scene.object(buddha_obj_id);
	buddha_obj.geom.setPos(makeVector(0.f,-4.f,0.f));
	buddha_obj.geom.setRpy(makeVector(0.f,0.f,0.f));
	buddha_obj.geom.setScale(0.3f);
	buddha_obj.mat.diffuse = White;
	buddha_obj.mat.shininess = 1.f;

	directional_light_cl light;
	light.set_dir(0.05f, -1.f, -0.02f);
	light.set_color(0.7f,0.7f,0.7f);
	scene.set_dir_light(light);

	color_cl ambient;
	ambient[0] = ambient[1] = ambient[2] = 0.2f;
	scene.set_ambient_light(ambient);

        scene.camera.set(buddha_stats_camera_pos[0],//pos 
                         buddha_stats_camera_dir[0],//dir
                         makeVector(0,1,0), //up
                         M_PI/4.,
                         window_size[0] / (float)window_size[1]);
}

LinearCameraTrajectory buddha_cam_traj;
void buddha_set_cam_traj(){
        buddha_cam_traj.set_look_at_pos(makeVector(0.2f,-2.3f,0.1f));
        buddha_cam_traj.set_up_vec(makeVector(0.f,1.f,0.f));
        buddha_cam_traj.set_waypoints(makeVector(2.6f,-2.4f,2.6f),
                makeVector(-3.7f,3.f,2.f));
        buddha_cam_traj.set_frames(100);
}

void teapot_set_scene(Scene& scene, size_t* window_size){
	std::vector<mesh_id> box_meshes = 
		scene.load_obj_file("models/obj/box-no-ceil.obj");
	std::vector<object_id> box_objs = scene.add_objects(box_meshes);

	for (uint32_t i = 0; i < box_objs.size(); ++i) {
		Object& obj = scene.object(box_objs[i]);
		obj.geom.setRpy(makeVector(0.f,0.f,0.4f));
		if (obj.mat.texture > 0)
			obj.mat.reflectiveness = 0.8f;
		// obj.geom.setPos(makeVector(0.f,-30.f,0.f));
	}
	mesh_id teapot_mesh_id = 
		scene.load_obj_file_as_aggregate("models/obj/teapot2.obj");
	object_id teapot_obj_id = scene.add_object(teapot_mesh_id);
	Object& teapot_obj = scene.object(teapot_obj_id);
	teapot_obj.geom.setPos(makeVector(0.f,-2.f,0.f));
	teapot_obj.geom.setRpy(makeVector(0.f,0.f,0.f));
	// teapot_obj.geom.setScale(0.3f);
	teapot_obj.geom.setScale(0.2f);
	teapot_obj.mat.diffuse = Blue;
	teapot_obj.mat.shininess = 0.5f;
	teapot_obj.mat.reflectiveness = 0.3f;
	teapot_obj.mat.refractive_index = 1.333f;

	directional_light_cl light;
	light.set_dir(0.05f, -1.f, -0.02f);
	light.set_color(0.7f,0.7f,0.7f);
	scene.set_dir_light(light);

	color_cl ambient;
	ambient[0] = ambient[1] = ambient[2] = 0.2f;
	scene.set_ambient_light(ambient);

}


