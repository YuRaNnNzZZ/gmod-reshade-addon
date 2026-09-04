#pragma once

#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

#include <reshade.hpp>

struct __declspec(uuid("E290207A-24D4-4C98-904D-8400827E4BCE")) render_state_tracking
{
	void apply(reshade::api::command_list *cmd_list) const;
	void clear();

	static void register_events();
	static void unregister_events();

	std::vector<reshade::api::resource_view> render_targets;
	reshade::api::resource_view depth_stencil = { 0 };
	std::unordered_map<reshade::api::pipeline_stage, reshade::api::pipeline> pipelines;
	reshade::api::primitive_topology primitive_topology = reshade::api::primitive_topology::undefined;
	uint32_t blend_constant = 0;
	uint32_t sample_mask = UINT32_MAX;
	uint32_t front_stencil_reference_value = 0;
	uint32_t back_stencil_reference_value = 0;
	std::vector<reshade::api::viewport> viewports;
	std::vector<reshade::api::rect> scissor_rects;
	std::unordered_map<
		reshade::api::shader_stage,
		std::pair<reshade::api::pipeline_layout, std::vector<reshade::api::descriptor_table>>>
		descriptor_tables;
};

