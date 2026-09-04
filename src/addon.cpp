#include <Windows.h>

#include <atomic>
#include <array>
#include <cstdint>
#include <vector>

#include <GarrysMod/Lua/Interface.h>
#include <imgui.h>
#include <reshade.hpp>

#include "d3d11_constant_buffer_state.hpp"
#include "render_state_tracking.hpp"

namespace
{
using GarrysMod::Lua::ILuaBase;
using reshade::api::command_list;
using reshade::api::device;
using reshade::api::effect_runtime;
using reshade::api::resource;
using reshade::api::resource_view;

constexpr unsigned char lua_client_realm = 0;
constexpr char config_section[] = "GModReShade";
constexpr char block_automatic_rendering_key[] = "BlockAutomaticEffectRendering";

class lua_shared_interface
{
public:
	virtual ~lua_shared_interface() = 0;
	virtual void init(void *interface_factory, bool unknown, void *steam_context, void *filesystem) = 0;
	virtual void shutdown() = 0;
	virtual void dump_stats() = 0;
	virtual ILuaBase *create_lua_interface(unsigned char realm, bool unknown) = 0;
	virtual void close_lua_interface(ILuaBase *lua) = 0;
	virtual ILuaBase *get_lua_interface(unsigned char realm) = 0;
};

using create_interface = void *(*)(const char *name, int *return_code);

std::atomic<effect_runtime *> s_runtime = nullptr;
std::atomic<ILuaBase *> s_injected_lua = nullptr;
std::atomic_bool s_block_automatic_rendering = true;

struct __declspec(uuid("78019D35-AB48-4578-BF83-A0BEA7A01A07")) runtime_data
{
	std::vector<std::array<resource_view, 2>> back_buffer_targets;
};

void destroy_runtime_data(effect_runtime *runtime)
{
	runtime_data *const data = runtime->get_private_data<runtime_data>();
	if (data == nullptr)
		return;

	device *const device = runtime->get_device();
	for (const auto &targets : data->back_buffer_targets)
	{
		device->destroy_resource_view(targets[0]);
		device->destroy_resource_view(targets[1]);
	}

	runtime->destroy_private_data<runtime_data>();
}

void create_runtime_data(effect_runtime *runtime)
{
	if (runtime->get_private_data<runtime_data>() != nullptr)
		destroy_runtime_data(runtime);

	auto *const data = runtime->create_private_data<runtime_data>();
	device *const device = runtime->get_device();
	const uint32_t back_buffer_count = runtime->get_back_buffer_count();
	data->back_buffer_targets.resize(back_buffer_count);

	for (uint32_t index = 0; index < back_buffer_count; ++index)
	{
		const resource back_buffer = runtime->get_back_buffer(index);
		const reshade::api::resource_desc desc = device->get_resource_desc(back_buffer);
		const reshade::api::resource_view_type view_type = desc.texture.samples > 1
			? reshade::api::resource_view_type::texture_2d_multisample
			: reshade::api::resource_view_type::texture_2d;

		auto &targets = data->back_buffer_targets[index];
		const bool linear_created = device->create_resource_view(
			back_buffer,
			reshade::api::resource_usage::render_target,
			reshade::api::resource_view_desc(
				view_type, reshade::api::format_to_default_typed(desc.texture.format, 0), 0, 1, 0, 1),
			&targets[0]);
		const bool srgb_created = device->create_resource_view(
			back_buffer,
			reshade::api::resource_usage::render_target,
			reshade::api::resource_view_desc(
				view_type, reshade::api::format_to_default_typed(desc.texture.format, 1), 0, 1, 0, 1),
			&targets[1]);

		if (!linear_created || !srgb_created)
		{
			device->destroy_resource_view(targets[0]);
			device->destroy_resource_view(targets[1]);
			targets = {};
		}
	}
}

ILuaBase *get_client_lua()
{
	const HMODULE lua_shared_module = GetModuleHandleW(L"lua_shared.dll");
	if (lua_shared_module == nullptr)
		return nullptr;

	const auto create_interface_fn = reinterpret_cast<create_interface>(
		GetProcAddress(lua_shared_module, "CreateInterface"));
	if (create_interface_fn == nullptr)
		return nullptr;

	auto *const lua_shared = static_cast<lua_shared_interface *>(
		create_interface_fn("LUASHARED003", nullptr));
	return lua_shared != nullptr ? lua_shared->get_lua_interface(lua_client_realm) : nullptr;
}

int draw_reshade_effects(lua_State *state)
{
	ILuaBase *const lua = state->luabase;
	lua->SetState(state);

	effect_runtime *const runtime = s_runtime.load(std::memory_order_acquire);
	if (runtime == nullptr)
		return 0;

	command_list *const cmd_list = runtime->get_command_queue()->get_immediate_command_list();
	const render_state_tracking *const current_state = cmd_list->get_private_data<render_state_tracking>();
	if (current_state == nullptr || current_state->render_targets.empty())
		return 0;

	resource_view target = current_state->render_targets.front();
	if (target == 0)
		return 0;

	resource_view target_srgb = target;
	const runtime_data *const data = runtime->get_private_data<runtime_data>();
	const resource target_resource = runtime->get_device()->get_resource_from_view(target);
	if (target_resource == runtime->get_current_back_buffer() && data != nullptr)
	{
		const uint32_t index = runtime->get_current_back_buffer_index();
		if (index < data->back_buffer_targets.size() && data->back_buffer_targets[index][0] != 0)
		{
			target = data->back_buffer_targets[index][0];
			target_srgb = data->back_buffer_targets[index][1];
		}
	}

	d3d11_constant_buffer_state constant_buffer_state(cmd_list);
	runtime->render_effects(cmd_list, target, target_srgb);
	current_state->apply(cmd_list);
	constant_buffer_state.restore();
	return 0;
}

void inject_lua_function()
{
	ILuaBase *const lua = get_client_lua();
	if (lua == nullptr)
	{
		s_injected_lua.store(nullptr, std::memory_order_release);
		return;
	}

	if (s_injected_lua.load(std::memory_order_acquire) == lua)
		return;

	const int stack_top = lua->Top();
	lua->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
	lua->GetField(-1, "render");

	if (lua->IsType(-1, GarrysMod::Lua::Type::Table))
	{
		lua->PushCFunction(draw_reshade_effects);
		lua->SetField(-2, "DrawReShadeEffects");
		s_injected_lua.store(lua, std::memory_order_release);
		reshade::log::message(reshade::log::level::info,
			"Registered render.DrawReShadeEffects in the Garry's Mod client Lua state.");
	}

	lua->Pop(lua->Top() - stack_top);
}

void remove_lua_function()
{
	ILuaBase *const lua = get_client_lua();
	if (lua == nullptr)
		return;

	const int stack_top = lua->Top();
	lua->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
	lua->GetField(-1, "render");

	if (lua->IsType(-1, GarrysMod::Lua::Type::Table))
	{
		lua->GetField(-1, "DrawReShadeEffects");
		const bool is_our_function =
			lua->IsType(-1, GarrysMod::Lua::Type::Function) &&
			lua->GetCFunction(-1) == draw_reshade_effects;
		lua->Pop();

		if (is_our_function)
		{
			lua->PushNil();
			lua->SetField(-2, "DrawReShadeEffects");
		}
	}

	lua->Pop(lua->Top() - stack_top);
}

void suppress_automatic_effect_rendering(effect_runtime *runtime, command_list *cmd_list)
{
	if (s_block_automatic_rendering.load(std::memory_order_acquire))
		runtime->render_effects(cmd_list, {}, {});
}

void on_init_effect_runtime(effect_runtime *runtime)
{
	create_runtime_data(runtime);
	s_runtime.store(runtime, std::memory_order_release);
	inject_lua_function();
	suppress_automatic_effect_rendering(
		runtime, runtime->get_command_queue()->get_immediate_command_list());
}

void on_destroy_effect_runtime(effect_runtime *runtime)
{
	effect_runtime *expected = runtime;
	s_runtime.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel);
	destroy_runtime_data(runtime);
}

void on_reshade_present(effect_runtime *runtime)
{
	s_runtime.store(runtime, std::memory_order_release);
	inject_lua_function();
}

void on_present(
	reshade::api::command_queue *queue,
	reshade::api::swapchain *,
	const reshade::api::rect *,
	const reshade::api::rect *,
	uint32_t,
	const reshade::api::rect *)
{
	effect_runtime *const runtime = s_runtime.load(std::memory_order_acquire);
	if (runtime == nullptr || runtime->get_device() != queue->get_device())
		return;

	suppress_automatic_effect_rendering(runtime, queue->get_immediate_command_list());
}

void draw_settings(effect_runtime *)
{
	bool block_automatic_rendering = s_block_automatic_rendering.load(std::memory_order_acquire);
	if (ImGui::Checkbox("Only render effects from render.DrawReShadeEffects()", &block_automatic_rendering))
	{
		s_block_automatic_rendering.store(block_automatic_rendering, std::memory_order_release);
		reshade::set_config_value(
			nullptr, config_section, block_automatic_rendering_key, block_automatic_rendering);
	}

	ImGui::TextWrapped(
		"When enabled, the normal end-of-frame ReShade effect pass is suppressed. "
		"Effects are only rendered in frames where client Lua calls render.DrawReShadeEffects().");
}
}

extern "C" __declspec(dllexport) const char *NAME = "Garry's Mod ReShade Effects";
extern "C" __declspec(dllexport) const char *DESCRIPTION =
	"Adds render.DrawReShadeEffects() to client Lua and optionally suppresses automatic end-of-frame effect rendering.";

extern "C" __declspec(dllexport) bool AddonInit(HMODULE addon_module, HMODULE reshade_module)
{
	if (!reshade::register_addon(addon_module, reshade_module))
		return false;

	bool block_automatic_rendering = true;
	reshade::get_config_value(
		nullptr, config_section, block_automatic_rendering_key, block_automatic_rendering);
	s_block_automatic_rendering.store(block_automatic_rendering, std::memory_order_release);

	render_state_tracking::register_events();
	reshade::register_event<reshade::addon_event::init_effect_runtime>(on_init_effect_runtime);
	reshade::register_event<reshade::addon_event::destroy_effect_runtime>(on_destroy_effect_runtime);
	reshade::register_event<reshade::addon_event::present>(on_present);
	reshade::register_event<reshade::addon_event::reshade_present>(on_reshade_present);
	reshade::register_overlay(nullptr, draw_settings);
	inject_lua_function();
	return true;
}

extern "C" __declspec(dllexport) void AddonUninit(HMODULE addon_module, HMODULE reshade_module)
{
	reshade::unregister_overlay(nullptr, draw_settings);
	reshade::unregister_event<reshade::addon_event::reshade_present>(on_reshade_present);
	reshade::unregister_event<reshade::addon_event::present>(on_present);
	reshade::unregister_event<reshade::addon_event::destroy_effect_runtime>(on_destroy_effect_runtime);
	reshade::unregister_event<reshade::addon_event::init_effect_runtime>(on_init_effect_runtime);
	render_state_tracking::unregister_events();

	remove_lua_function();
	if (effect_runtime *const runtime = s_runtime.exchange(nullptr, std::memory_order_acq_rel))
		destroy_runtime_data(runtime);
	s_injected_lua.store(nullptr, std::memory_order_release);
	reshade::unregister_addon(addon_module, reshade_module);
}
