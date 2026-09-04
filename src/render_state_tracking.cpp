#include "render_state_tracking.hpp"

namespace
{
using namespace reshade::api;

render_state_tracking &get_state(command_list *cmd_list)
{
	if (auto *const state = cmd_list->get_private_data<render_state_tracking>())
		return *state;

	return *cmd_list->create_private_data<render_state_tracking>();
}

void on_init_command_list(command_list *cmd_list)
{
	if (cmd_list->get_private_data<render_state_tracking>() == nullptr)
		cmd_list->create_private_data<render_state_tracking>();
}

void on_destroy_command_list(command_list *cmd_list)
{
	if (cmd_list->get_private_data<render_state_tracking>() != nullptr)
		cmd_list->destroy_private_data<render_state_tracking>();
}

void on_bind_render_targets_and_depth_stencil(
	command_list *cmd_list, uint32_t count, const resource_view *rtvs, resource_view dsv)
{
	auto &state = get_state(cmd_list);
	state.render_targets.assign(rtvs, rtvs + count);
	state.depth_stencil = dsv;
}

void on_bind_pipeline(command_list *cmd_list, pipeline_stage stages, pipeline pipeline)
{
	get_state(cmd_list).pipelines[stages] = pipeline;
}

void on_bind_pipeline_states(
	command_list *cmd_list, uint32_t count, const dynamic_state *states, const uint32_t *values)
{
	auto &state = get_state(cmd_list);
	for (uint32_t index = 0; index < count; ++index)
	{
		switch (states[index])
		{
		case dynamic_state::primitive_topology:
			state.primitive_topology = static_cast<primitive_topology>(values[index]);
			break;
		case dynamic_state::blend_constant:
			state.blend_constant = values[index];
			break;
		case dynamic_state::sample_mask:
			state.sample_mask = values[index];
			break;
		case dynamic_state::front_stencil_reference_value:
			state.front_stencil_reference_value = values[index];
			break;
		case dynamic_state::back_stencil_reference_value:
			state.back_stencil_reference_value = values[index];
			break;
		default:
			break;
		}
	}
}

void on_bind_viewports(command_list *cmd_list, uint32_t first, uint32_t count, const viewport *viewports)
{
	auto &state = get_state(cmd_list);
	if (state.viewports.size() < first + count)
		state.viewports.resize(first + count);

	for (uint32_t index = 0; index < count; ++index)
		state.viewports[first + index] = viewports[index];
}

void on_bind_scissor_rects(command_list *cmd_list, uint32_t first, uint32_t count, const rect *rects)
{
	auto &state = get_state(cmd_list);
	if (state.scissor_rects.size() < first + count)
		state.scissor_rects.resize(first + count);

	for (uint32_t index = 0; index < count; ++index)
		state.scissor_rects[first + index] = rects[index];
}

void on_bind_descriptor_tables(
	command_list *cmd_list,
	shader_stage stages,
	pipeline_layout layout,
	uint32_t first,
	uint32_t count,
	const descriptor_table *tables,
	uint32_t,
	const uint32_t *)
{
	auto &descriptor_state = get_state(cmd_list).descriptor_tables[stages];
	if (descriptor_state.first != layout)
		descriptor_state.second.clear();
	descriptor_state.first = layout;

	if (descriptor_state.second.size() < first + count)
		descriptor_state.second.resize(first + count);

	for (uint32_t index = 0; index < count; ++index)
		descriptor_state.second[first + index] = tables[index];
}

void on_reset_command_list(command_list *cmd_list)
{
	get_state(cmd_list).clear();
}
}

void render_state_tracking::apply(reshade::api::command_list *cmd_list) const
{
	using namespace reshade::api;

	if (!render_targets.empty() || depth_stencil != 0)
		cmd_list->bind_render_targets_and_depth_stencil(
			static_cast<uint32_t>(render_targets.size()), render_targets.data(), depth_stencil);

	for (const auto &[stages, pipeline] : pipelines)
		cmd_list->bind_pipeline(stages, pipeline);

	if (primitive_topology != primitive_topology::undefined)
		cmd_list->bind_pipeline_state(dynamic_state::primitive_topology, static_cast<uint32_t>(primitive_topology));
	if (blend_constant != 0)
		cmd_list->bind_pipeline_state(dynamic_state::blend_constant, blend_constant);
	if (sample_mask != UINT32_MAX)
		cmd_list->bind_pipeline_state(dynamic_state::sample_mask, sample_mask);
	if (front_stencil_reference_value != 0)
		cmd_list->bind_pipeline_state(dynamic_state::front_stencil_reference_value, front_stencil_reference_value);
	if (back_stencil_reference_value != 0)
		cmd_list->bind_pipeline_state(dynamic_state::back_stencil_reference_value, back_stencil_reference_value);

	if (!viewports.empty())
		cmd_list->bind_viewports(0, static_cast<uint32_t>(viewports.size()), viewports.data());
	if (!scissor_rects.empty())
		cmd_list->bind_scissor_rects(0, static_cast<uint32_t>(scissor_rects.size()), scissor_rects.data());

	for (const auto &[stages, descriptor_state] : descriptor_tables)
	{
		cmd_list->bind_descriptor_tables(
			stages,
			descriptor_state.first,
			0,
			static_cast<uint32_t>(descriptor_state.second.size()),
			descriptor_state.second.data());
	}
}

void render_state_tracking::clear()
{
	render_targets.clear();
	depth_stencil = { 0 };
	pipelines.clear();
	primitive_topology = reshade::api::primitive_topology::undefined;
	blend_constant = 0;
	sample_mask = UINT32_MAX;
	front_stencil_reference_value = 0;
	back_stencil_reference_value = 0;
	viewports.clear();
	scissor_rects.clear();
	descriptor_tables.clear();
}

void render_state_tracking::register_events()
{
	reshade::register_event<reshade::addon_event::init_command_list>(on_init_command_list);
	reshade::register_event<reshade::addon_event::destroy_command_list>(on_destroy_command_list);
	reshade::register_event<reshade::addon_event::bind_render_targets_and_depth_stencil>(
		on_bind_render_targets_and_depth_stencil);
	reshade::register_event<reshade::addon_event::bind_pipeline>(on_bind_pipeline);
	reshade::register_event<reshade::addon_event::bind_pipeline_states>(on_bind_pipeline_states);
	reshade::register_event<reshade::addon_event::bind_viewports>(on_bind_viewports);
	reshade::register_event<reshade::addon_event::bind_scissor_rects>(on_bind_scissor_rects);
	reshade::register_event<reshade::addon_event::bind_descriptor_tables>(on_bind_descriptor_tables);
	reshade::register_event<reshade::addon_event::reset_command_list>(on_reset_command_list);
}

void render_state_tracking::unregister_events()
{
	reshade::unregister_event<reshade::addon_event::init_command_list>(on_init_command_list);
	reshade::unregister_event<reshade::addon_event::destroy_command_list>(on_destroy_command_list);
	reshade::unregister_event<reshade::addon_event::bind_render_targets_and_depth_stencil>(
		on_bind_render_targets_and_depth_stencil);
	reshade::unregister_event<reshade::addon_event::bind_pipeline>(on_bind_pipeline);
	reshade::unregister_event<reshade::addon_event::bind_pipeline_states>(on_bind_pipeline_states);
	reshade::unregister_event<reshade::addon_event::bind_viewports>(on_bind_viewports);
	reshade::unregister_event<reshade::addon_event::bind_scissor_rects>(on_bind_scissor_rects);
	reshade::unregister_event<reshade::addon_event::bind_descriptor_tables>(on_bind_descriptor_tables);
	reshade::unregister_event<reshade::addon_event::reset_command_list>(on_reset_command_list);
}

