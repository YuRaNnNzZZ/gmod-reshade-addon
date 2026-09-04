#pragma once

namespace reshade::api
{
struct command_list;
}

class d3d11_constant_buffer_state
{
public:
	explicit d3d11_constant_buffer_state(reshade::api::command_list *cmd_list);
	~d3d11_constant_buffer_state();

	d3d11_constant_buffer_state(const d3d11_constant_buffer_state &) = delete;
	d3d11_constant_buffer_state &operator=(const d3d11_constant_buffer_state &) = delete;

	void restore();

private:
	struct implementation;
	implementation *_implementation = nullptr;
};

