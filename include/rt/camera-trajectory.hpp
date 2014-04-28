#pragma once
#ifndef CAMERA_TRAJECTORY_HPP
#define CAMERA_TRAJECTORY_HPP

#include <rt/vector.hpp>

class CameraTrajectory {

public:

        virtual void get_next_camera_params(vec3* cam_pos, vec3* cam_dir, vec3* cam_up)= 0;
        void reset() {current_frame = 0;}
        void set_frames(uint32_t f) {frames = f;}
        void set_repeating(bool r){repeating = r;}

protected:
        uint32_t frames;
        uint32_t current_frame;
        bool     repeating;
};

class LinearCameraTrajectory : public CameraTrajectory {

public:
        LinearCameraTrajectory(){
                frames = 0;
                current_frame = 0;
                repeating = false;
                up_vec = makeVector(0.f,1.f,0.f);
        }
        void set_look_at_pos(vec3 _look_at_pos){look_at_pos = _look_at_pos;};
        void set_waypoints(vec3 _start_pos, vec3 _end_pos){
                start_pos = _start_pos;
                end_pos = _end_pos;
        }
        void set_up_vec(vec3 up) {up_vec = up;} 
        void get_next_camera_params(vec3* cam_pos, vec3* cam_dir, vec3* cam_up);
private:

        vec3 look_at_pos;
        vec3 up_vec;
        vec3 start_pos;
        vec3 end_pos;
};

#endif /* CAMERA_TRAJECTORY_HPP */
