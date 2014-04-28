#pragma once
#ifndef BVH_BUILDER_HPP
#define BVH_BUILDER_HPP

#include <stdint.h>

#include <misc/log.hpp>
#include <rt/bvh.hpp>
#include <rt/timing.hpp>
#include <rt/scene.hpp>
#include <rt/renderer-config.hpp>
#include <gpu/interface.hpp>
#include <iostream>


class BVHBuilder {

public:

	int32_t initialize();
	int32_t build_lbvh(Scene& scene, size_t cq_i = 0);
	int32_t refit_lbvh(Scene& scene, size_t cq_i = 0);
        void    set_log(Log* log);

	void    timing(bool b);
	void    logging(bool l);
	double  get_exec_time();
        void    update_configuration(const RendererConfig& conf);

private:
        
        Log*          m_log;
        bool          m_logging;
        bool          m_timing;
	rt_time_t     m_timer;
	double        m_time_ms;

        function_id  primitive_bbox_builder_id;
        function_id  morton_encoder_fixed_id;
        function_id  morton_encoder_id;

        function_id  morton_sorter_2_id;
        function_id  morton_sorter_4_id;
        function_id  morton_sorter_8_id;
        function_id  morton_sorter_16_id;

        function_id  splits_build_id;
        function_id  treelet_maps_init_id;
        function_id  treelet_init_id;
        function_id  segment_map_init_id;
        function_id  segment_heads_init_id;
        function_id  treelet_build_id;
        function_id  node_counter_id;
        function_id  node_build_id;
        function_id  node_bbox_builder_id;
        function_id  leaf_bbox_builder_id;
        function_id  max_local_bbox_id;

        function_id  process_task_id;

        function_id index_rearranger_id;
        function_id map_rearranger_id;
        function_id morton_rearranger_id;

        memory_id    triangles_mem_id;

        memory_id    outputCounter_mem_id;
        memory_id    processedCounter_mem_id;

        cl_uint      outputCounter[64];
        cl_uint      processedCounter[64];
        cl_uint      node_count;
        cl_uint      bvh_depth;
        cl_uint      bvh_min_leaf_size;

public:        
        bool         bvh_created;
};


#endif /* BVH_BUILDER_HPP */
