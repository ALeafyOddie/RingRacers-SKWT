// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2023 by Ronald "Eidolon" Kinard
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------

#ifndef __SRB2_HWR2_BLIT_RECT_HPP__
#define __SRB2_HWR2_BLIT_RECT_HPP__

#include <array>

#include "../rhi/rhi.hpp"
#include "resource_management.hpp"

namespace srb2::hwr2
{

/// @brief A render pass which blits a rect using a source texture or textures.
class BlitRectPass
{
	rhi::Handle<rhi::Pipeline> pipeline_;
	rhi::Handle<rhi::Texture> texture_;
	uint32_t texture_width_ = 0;
	uint32_t texture_height_ = 0;
	rhi::Handle<rhi::Texture> output_;
	uint32_t output_width_ = 0;
	uint32_t output_height_ = 0;
	bool output_correct_aspect_ = false;
	bool output_flip_ = false;
	rhi::Handle<rhi::Buffer> quad_vbo_;
	rhi::Handle<rhi::Buffer> quad_ibo_;
	std::array<rhi::Handle<rhi::UniformSet>, 2> uniform_sets_;
	rhi::Handle<rhi::BindingSet> binding_set_;

	bool quad_vbo_needs_upload_ = false;
	bool quad_ibo_needs_upload_ = false;

	void prepass(rhi::Rhi& rhi);
	void transfer(rhi::Rhi& rhi, rhi::Handle<rhi::GraphicsContext> ctx);
	void graphics(rhi::Rhi& rhi, rhi::Handle<rhi::GraphicsContext> ctx);

public:
	BlitRectPass();
	~BlitRectPass();

	void draw(rhi::Rhi& rhi, rhi::Handle<rhi::GraphicsContext> ctx);

	/// @brief Set the next blit texture. Don't call during graphics phase!
	/// @param texture the texture to use when blitting
	/// @param width   texture width
	/// @param height  texture height
	void set_texture(rhi::Handle<rhi::Texture> texture, uint32_t width, uint32_t height) noexcept
	{
		texture_ = texture;
		texture_width_ = width;
		texture_height_ = height;
	}

	/// @brief Set the next output texture. Don't call during graphics phase!
	/// @param width   texture width
	/// @param height  texture height
	void set_output(
		uint32_t width,
		uint32_t height,
		bool correct_aspect,
		bool flip
	) noexcept
	{
		output_width_ = width;
		output_height_ = height;
		output_correct_aspect_ = correct_aspect;
		output_flip_ = flip;
	}
};

} // namespace srb2::hwr2

#endif // __SRB2_HWR2_BLIT_RECT_HPP__