#pragma once
#ifndef FRAMESTATS_HPP
#define FRAMESTATS_HPP

enum rt_stage {
        BVH_BUILD = 0,
        PRIM_RAY_GEN,
        SEC_RAY_GEN,
        PRIM_TRACE,
        PRIM_SHADOW_TRACE,
        SEC_TRACE,
        SEC_SHADOW_TRACE,
        SHADE,
        FB_CLEAR,
        FB_COPY,
        STAGE_COUNT
};

struct FrameStats {
        double   get_stage_time(rt_stage stage) const {
                return stage_times[stage];}
        double   get_stage_mean_time(rt_stage stage) const {
                if (acc_frames)
                        return stage_acc_times[stage]/acc_frames;
                return 0;
        }

        double   get_frame_time() const {
                return frame_time;}
        double   get_mean_frame_time() const {
                if (acc_frames)
                        return frame_acc_time/acc_frames;
                return 0;
        };

        size_t  get_ray_count() const {
               return total_ray_count;}
        size_t  get_secondary_ray_count() const {
               return total_sec_ray_count;}

        void clear_times() {
                for (uint32_t i = 0; i < STAGE_COUNT; ++i) {
                        stage_times[i] = 0;
                }
        }

        void clear_mean_times() {
                acc_frames = 0;
                for (uint32_t i = 0; i < STAGE_COUNT; ++i) {
                        stage_acc_times[i] = 0;
                }
        }

        double stage_times[STAGE_COUNT];
        double stage_acc_times[STAGE_COUNT];
        uint32_t acc_frames;

        double frame_time;
        double frame_acc_time;

        size_t total_ray_count;
        size_t total_sec_ray_count;

};

#endif // FRAMESTATS_HPP
