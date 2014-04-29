std::string log_filename("rt-log-boat");

vec3 stats_camera_pos[] = {makeVector(20.0186, -5.49632, 71.8718) ,
			   makeVector(-56.9387, -2.41959, 29.574) ,
			   makeVector(68.6859, 5.18034, 13.6691) };
vec3 stats_camera_dir[] = {makeVector(0.131732, 0.0845093, -0.987677),
                           makeVector(0.927574, -0.22893, -0.295292) ,
                           makeVector(-0.820819, -0.478219, -0.31235) };

LinearCameraTrajectory cam_traj;
