#include <rt/camera-trajectory.hpp>

void 
LinearCameraTrajectory::get_next_camera_params(vec3* cam_pos, vec3* cam_dir, vec3* cam_up){

        if (current_frame == 0) {
                *cam_pos = start_pos;
        }
        else if(current_frame >= frames) {
                *cam_pos = end_pos;
        } else {
                float w = current_frame/(float)frames;
                *cam_pos = start_pos * (1.f-w) + end_pos * w;
        }

        *cam_dir = (look_at_pos - *cam_pos).normalized();
        *cam_up = up_vec;

        current_frame++;
        if (repeating && current_frame >= frames)
                current_frame = 0;
}
