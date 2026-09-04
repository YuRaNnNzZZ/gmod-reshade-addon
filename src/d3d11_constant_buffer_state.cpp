#include "d3d11_constant_buffer_state.hpp"

#include <Windows.h>
#include <d3d11_1.h>

#include <array>

#include <reshade.hpp>

namespace
{
constexpr UINT slot_count = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;

struct stage_state
{
	std::array<ID3D11Buffer *, slot_count> buffers = {};
	std::array<UINT, slot_count> first_constants = {};
	std::array<UINT, slot_count> constant_counts = {};

	void release()
	{
		for (ID3D11Buffer *&buffer : buffers)
		{
			if (buffer != nullptr)
				buffer->Release();
			buffer = nullptr;
		}
	}
};
}

struct d3d11_constant_buffer_state::implementation
{
	ID3D11DeviceContext1 *context = nullptr;
	stage_state vertex;
	stage_state hull;
	stage_state domain;
	stage_state geometry;
	stage_state pixel;
	stage_state compute;
	bool restored = false;

	~implementation()
	{
		vertex.release();
		hull.release();
		domain.release();
		geometry.release();
		pixel.release();
		compute.release();
		if (context != nullptr)
			context->Release();
	}
};

d3d11_constant_buffer_state::d3d11_constant_buffer_state(reshade::api::command_list *cmd_list)
{
	if (cmd_list->get_device()->get_api() != reshade::api::device_api::d3d11)
		return;

	auto *const context = reinterpret_cast<ID3D11DeviceContext *>(cmd_list->get_native());
	auto *state = new implementation();
	if (FAILED(context->QueryInterface(IID_PPV_ARGS(&state->context))))
	{
		delete state;
		return;
	}

	state->context->VSGetConstantBuffers1(
		0, slot_count, state->vertex.buffers.data(), state->vertex.first_constants.data(), state->vertex.constant_counts.data());
	state->context->HSGetConstantBuffers1(
		0, slot_count, state->hull.buffers.data(), state->hull.first_constants.data(), state->hull.constant_counts.data());
	state->context->DSGetConstantBuffers1(
		0, slot_count, state->domain.buffers.data(), state->domain.first_constants.data(), state->domain.constant_counts.data());
	state->context->GSGetConstantBuffers1(
		0, slot_count, state->geometry.buffers.data(), state->geometry.first_constants.data(), state->geometry.constant_counts.data());
	state->context->PSGetConstantBuffers1(
		0, slot_count, state->pixel.buffers.data(), state->pixel.first_constants.data(), state->pixel.constant_counts.data());
	state->context->CSGetConstantBuffers1(
		0, slot_count, state->compute.buffers.data(), state->compute.first_constants.data(), state->compute.constant_counts.data());

	_implementation = state;
}

d3d11_constant_buffer_state::~d3d11_constant_buffer_state()
{
	restore();
	delete _implementation;
}

void d3d11_constant_buffer_state::restore()
{
	if (_implementation == nullptr || _implementation->restored)
		return;

	auto *const state = _implementation;
	state->context->VSSetConstantBuffers1(
		0, slot_count, state->vertex.buffers.data(), state->vertex.first_constants.data(), state->vertex.constant_counts.data());
	state->context->HSSetConstantBuffers1(
		0, slot_count, state->hull.buffers.data(), state->hull.first_constants.data(), state->hull.constant_counts.data());
	state->context->DSSetConstantBuffers1(
		0, slot_count, state->domain.buffers.data(), state->domain.first_constants.data(), state->domain.constant_counts.data());
	state->context->GSSetConstantBuffers1(
		0, slot_count, state->geometry.buffers.data(), state->geometry.first_constants.data(), state->geometry.constant_counts.data());
	state->context->PSSetConstantBuffers1(
		0, slot_count, state->pixel.buffers.data(), state->pixel.first_constants.data(), state->pixel.constant_counts.data());
	state->context->CSSetConstantBuffers1(
		0, slot_count, state->compute.buffers.data(), state->compute.first_constants.data(), state->compute.constant_counts.data());
	state->restored = true;
}
