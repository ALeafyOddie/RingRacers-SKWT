// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2023 by Ronald "Eidolon" Kinard
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------

#include "pass_resource_managers.hpp"

#include <algorithm>
#include <cmath>

#include "../r_state.h"
#include "../v_video.h"
#include "../z_zone.h"

using namespace srb2;
using namespace srb2::hwr2;
using namespace srb2::rhi;

FramebufferManager::FramebufferManager() : Pass()
{
}

FramebufferManager::~FramebufferManager() = default;

void FramebufferManager::prepass(Rhi& rhi)
{
	uint32_t current_width = vid.width;
	uint32_t current_height = vid.height;

	// Destroy the framebuffer textures if they exist and the video size changed
	if (width_ != current_width || height_ != current_height)
	{
		if (main_color_ != kNullHandle)
		{
			rhi.destroy_texture(main_color_);
			main_color_ = kNullHandle;
		}
		if (main_depth_ != kNullHandle)
		{
			rhi.destroy_renderbuffer(main_depth_);
			main_depth_ = kNullHandle;
		}

		if (post_colors_[0] != kNullHandle)
		{
			rhi.destroy_texture(post_colors_[0]);
			post_colors_[0] = kNullHandle;
		}
		if (post_colors_[1] != kNullHandle)
		{
			rhi.destroy_texture(post_colors_[1]);
			post_colors_[1] = kNullHandle;
		}
		if (wipe_start_color_ != kNullHandle)
		{
			rhi.destroy_texture(wipe_start_color_);
			wipe_start_color_ = kNullHandle;
		}
		if (wipe_end_color_ != kNullHandle)
		{
			rhi.destroy_texture(wipe_end_color_);
			wipe_end_color_ = kNullHandle;
		}
	}
	width_ = current_width;
	height_ = current_height;

	// Recreate the framebuffer textures
	if (main_color_ == kNullHandle)
	{
		main_color_ = rhi.create_texture({
			TextureFormat::kRGBA,
			current_width,
			current_height,
			TextureWrapMode::kClamp,
			TextureWrapMode::kClamp
		});
	}
	if (main_depth_ == kNullHandle)
	{
		main_depth_ = rhi.create_renderbuffer({current_width, current_height});
	}

	if (post_colors_[0] == kNullHandle)
	{
		post_colors_[0] = rhi.create_texture({
			TextureFormat::kRGBA,
			current_width,
			current_height,
			TextureWrapMode::kClamp,
			TextureWrapMode::kClamp
		});
	}
	if (post_colors_[1] == kNullHandle)
	{
		post_colors_[1] = rhi.create_texture({
			TextureFormat::kRGBA,
			current_width,
			current_height,
			TextureWrapMode::kClamp,
			TextureWrapMode::kClamp
		});
	}

	if (wipe_start_color_ == kNullHandle)
	{
		wipe_start_color_ = rhi.create_texture({
			TextureFormat::kRGBA,
			current_width,
			current_height,
			TextureWrapMode::kClamp,
			TextureWrapMode::kClamp
		});
	}
	if (wipe_end_color_ == kNullHandle)
	{
		wipe_end_color_ = rhi.create_texture({
			TextureFormat::kRGBA,
			current_width,
			current_height,
			TextureWrapMode::kClamp,
			TextureWrapMode::kClamp
		});
	}
}

void FramebufferManager::transfer(Rhi& rhi, Handle<TransferContext> ctx)
{
}

void FramebufferManager::graphics(Rhi& rhi, Handle<GraphicsContext> ctx)
{
}

void FramebufferManager::postpass(Rhi& rhi)
{
}

MainPaletteManager::MainPaletteManager() = default;
MainPaletteManager::~MainPaletteManager() = default;

constexpr std::size_t kPaletteSize = 256;
constexpr std::size_t kLighttableRows = LIGHTLEVELS;

void MainPaletteManager::prepass(Rhi& rhi)
{
	if (!palette_)
	{
		palette_ = rhi.create_texture({TextureFormat::kRGBA, kPaletteSize, 1, TextureWrapMode::kClamp, TextureWrapMode::kClamp});
	}

	if (!lighttable_)
	{
		lighttable_ = rhi.create_texture({TextureFormat::kLuminance, kPaletteSize, kLighttableRows, TextureWrapMode::kClamp, TextureWrapMode::kClamp});
	}

	if (!encore_lighttable_)
	{
		encore_lighttable_ = rhi.create_texture({TextureFormat::kLuminance, kPaletteSize, kLighttableRows, TextureWrapMode::kClamp, TextureWrapMode::kClamp});
	}

	if (!default_colormap_)
	{
		default_colormap_ = rhi.create_texture({TextureFormat::kLuminance, kPaletteSize, 1, TextureWrapMode::kClamp, TextureWrapMode::kClamp});
	}
}

void MainPaletteManager::upload_palette(Rhi& rhi, Handle<TransferContext> ctx)
{
	std::array<byteColor_t, kPaletteSize> palette_32;
	for (std::size_t i = 0; i < kPaletteSize; i++)
	{
		palette_32[i] = V_GetColor(i).s;
	}
	rhi.update_texture(ctx, palette_, {0, 0, kPaletteSize, 1}, PixelFormat::kRGBA8, tcb::as_bytes(tcb::span(palette_32)));
}

void MainPaletteManager::upload_lighttables(Rhi& rhi, Handle<TransferContext> ctx)
{
	if (colormaps != nullptr)
	{
		tcb::span<const std::byte> colormap_bytes = tcb::as_bytes(tcb::span(colormaps, kPaletteSize * kLighttableRows));
		rhi.update_texture(ctx, lighttable_, {0, 0, kPaletteSize, kLighttableRows}, PixelFormat::kR8, colormap_bytes);
	}

	if (encoremap != nullptr)
	{
		tcb::span<const std::byte> encoremap_bytes = tcb::as_bytes(tcb::span(encoremap, kPaletteSize * kLighttableRows));
		rhi.update_texture(ctx, encore_lighttable_, {0, 0, kPaletteSize, kLighttableRows}, PixelFormat::kR8, encoremap_bytes);
	}

	if (!lighttables_to_upload_.empty())
	{
		for (auto lighttable : lighttables_to_upload_)
		{
			Handle<Texture> lighttable_tex = find_extra_lighttable(lighttable);
			SRB2_ASSERT(lighttable_tex != kNullHandle);
			tcb::span<const std::byte> lighttable_bytes = tcb::as_bytes(tcb::span(lighttable, kPaletteSize * kLighttableRows));
			rhi.update_texture(ctx, lighttable_tex, {0, 0, kPaletteSize, kLighttableRows}, PixelFormat::kR8, lighttable_bytes);
		}
		lighttables_to_upload_.clear();
	}
}

void MainPaletteManager::upload_default_colormap(Rhi& rhi, Handle<TransferContext> ctx)
{
	std::array<uint8_t, kPaletteSize> data;
	for (std::size_t i = 0; i < kPaletteSize; i++)
	{
		data[i] = i;
	}
	rhi.update_texture(ctx, default_colormap_, {0, 0, kPaletteSize, 1}, PixelFormat::kR8, tcb::as_bytes(tcb::span(data)));
}

void MainPaletteManager::upload_colormaps(Rhi& rhi, Handle<TransferContext> ctx)
{
	for (auto to_upload : colormaps_to_upload_)
	{
		SRB2_ASSERT(to_upload != nullptr);
		SRB2_ASSERT(colormaps_.find(to_upload) != colormaps_.end());

		rhi::Handle<rhi::Texture> map_texture = colormaps_.at(to_upload);

		tcb::span<const std::byte> map_bytes = tcb::as_bytes(tcb::span(to_upload, kPaletteSize));
		rhi.update_texture(ctx, map_texture, {0, 0, kPaletteSize, 1}, PixelFormat::kR8, map_bytes);
	}
	colormaps_to_upload_.clear();
}

rhi::Handle<rhi::Texture> MainPaletteManager::find_or_create_colormap(rhi::Rhi& rhi, srb2::NotNull<const uint8_t*> colormap)
{
	if (colormaps_.find(colormap) != colormaps_.end())
	{
		return colormaps_[colormap];
	}

	Handle<Texture> texture = rhi.create_texture({TextureFormat::kLuminance, kPaletteSize, 1, TextureWrapMode::kClamp, TextureWrapMode::kClamp});

	colormaps_.insert_or_assign(colormap, texture);
	colormaps_to_upload_.push_back(colormap);
	return texture;
}

rhi::Handle<rhi::Texture> MainPaletteManager::find_colormap(srb2::NotNull<const uint8_t*> colormap) const
{
	if (colormaps_.find(colormap) == colormaps_.end())
	{
		return kNullHandle;
	}

	return colormaps_.at(colormap);
}

rhi::Handle<rhi::Texture> MainPaletteManager::find_or_create_extra_lighttable(Rhi& rhi, srb2::NotNull<const uint8_t*> lighttable)
{
	if (lighttables_.find(lighttable) != lighttables_.end())
	{
		return lighttables_[lighttable];
	}

	Handle<Texture> texture = rhi.create_texture({TextureFormat::kLuminance, kPaletteSize, kLighttableRows, TextureWrapMode::kClamp, TextureWrapMode::kClamp});

	lighttables_.insert_or_assign(lighttable, texture);
	lighttables_to_upload_.push_back(lighttable);
	return texture;
}

rhi::Handle<rhi::Texture> MainPaletteManager::find_extra_lighttable(srb2::NotNull<const uint8_t*> lighttable) const
{
	if (lighttables_.find(lighttable) == lighttables_.end())
	{
		return kNullHandle;
	}

	return lighttables_.at(lighttable);
}

void MainPaletteManager::transfer(Rhi& rhi, Handle<TransferContext> ctx)
{
	upload_palette(rhi, ctx);
	upload_lighttables(rhi, ctx);
	upload_default_colormap(rhi, ctx);
	upload_colormaps(rhi, ctx);
}

void MainPaletteManager::graphics(Rhi& rhi, Handle<GraphicsContext> ctx)
{
}

void MainPaletteManager::postpass(Rhi& rhi)
{
	// Delete all colormap textures so they'll be recreated next frame
	// Unfortunately the 256x1 translation colormaps are sometimes freed at runtime
	for (auto& cm : colormaps_)
	{
		rhi.destroy_texture(cm.second);
	}
	colormaps_.clear();

	for (auto& lt : lighttables_)
	{
		rhi.destroy_texture(lt.second);
	}
	lighttables_.clear();
}

CommonResourcesManager::CommonResourcesManager() = default;
CommonResourcesManager::~CommonResourcesManager() = default;

void CommonResourcesManager::prepass(Rhi& rhi)
{
	if (!init_)
	{
		black_ = rhi.create_texture({TextureFormat::kRGBA, 1, 1, TextureWrapMode::kClamp, TextureWrapMode::kClamp});
		white_ = rhi.create_texture({TextureFormat::kRGBA, 1, 1, TextureWrapMode::kClamp, TextureWrapMode::kClamp});
		transparent_ = rhi.create_texture({
			TextureFormat::kRGBA,
			1,
			1,
			TextureWrapMode::kClamp,
			TextureWrapMode::kClamp
		});
	}
}

void CommonResourcesManager::transfer(Rhi& rhi, Handle<TransferContext> ctx)
{
	if (!init_)
	{
		uint8_t black[4] = {0, 0, 0, 255};
		tcb::span<const std::byte> black_bytes = tcb::as_bytes(tcb::span(black, 4));
		uint8_t white[4] = {255, 255, 255, 255};
		tcb::span<const std::byte> white_bytes = tcb::as_bytes(tcb::span(white, 4));
		uint8_t transparent[4] = {0, 0, 0, 0};
		tcb::span<const std::byte> transparent_bytes = tcb::as_bytes(tcb::span(transparent, 4));

		rhi.update_texture(ctx, black_, {0, 0, 1, 1}, PixelFormat::kRGBA8, black_bytes);
		rhi.update_texture(ctx, white_, {0, 0, 1, 1}, PixelFormat::kRGBA8, white_bytes);
		rhi.update_texture(ctx, transparent_, {0, 0, 1, 1}, PixelFormat::kRGBA8, transparent_bytes);
	}
}

void CommonResourcesManager::graphics(Rhi& rhi, Handle<GraphicsContext> ctx)
{
}

void CommonResourcesManager::postpass(Rhi& rhi)
{
	init_ = true;
}

static uint32_t get_flat_size(lumpnum_t lump)
{
	SRB2_ASSERT(lump != LUMPERROR);

	std::size_t lumplength = W_LumpLength(lump);
	if (lumplength == 0)
	{
		return 0;
	}

	if ((lumplength & (lumplength - 1)) != 0)
	{
		// Lump length is not a power of two and therefore not a flat.
		return 0;
	}
	uint32_t lumpsize = std::pow(2, std::log2(lumplength) / 2);
	return lumpsize;
}

FlatTextureManager::FlatTextureManager() : Pass()
{
}

FlatTextureManager::~FlatTextureManager() = default;

void FlatTextureManager::prepass(Rhi& rhi)
{
}

void FlatTextureManager::transfer(Rhi& rhi, Handle<TransferContext> ctx)
{
	std::vector<std::array<uint8_t, 2>> flat_data;
	for (auto flat_lump : to_upload_)
	{
		flat_data.clear();
		Handle<Texture> flat_texture = flats_[flat_lump];
		SRB2_ASSERT(flat_texture != kNullHandle);
		std::size_t lump_length = W_LumpLength(flat_lump);
		uint32_t flat_size = get_flat_size(flat_lump);
		flat_data.reserve(flat_size * flat_size);

		const uint8_t* flat_memory = static_cast<const uint8_t*>(W_CacheLumpNum(flat_lump, PU_PATCH));
		SRB2_ASSERT(flat_memory != nullptr);

		tcb::span<const uint8_t> flat_bytes = tcb::span(flat_memory, lump_length);
		for (const uint8_t index : flat_bytes)
		{
			// The alpha/green channel is set to 0 if it's index 247; this is not usually used but fake floors can be
			// masked sometimes, so we need to treat it as transparent when rendering them.
			// See https://zdoom.org/wiki/Palette for remarks on fake 247 transparency
			flat_data.push_back({index, index == 247 ? static_cast<uint8_t>(0) : static_cast<uint8_t>(255)});
		}

		// A flat size of 1 would end up being 2 bytes, so we need 2 more bytes to be unpack-aligned on texture upload
		// Any other size would implicitly be aligned.
		// Sure hope nobody tries to load any flats that are too big for the gpu!
		if (flat_size == 1)
		{
			flat_data.push_back({0, 0});
		}

		tcb::span<const std::byte> data_bytes = tcb::as_bytes(tcb::span(flat_data));
		rhi.update_texture(ctx, flat_texture, {0, 0, flat_size, flat_size}, rhi::PixelFormat::kRG8, data_bytes);
	}
	to_upload_.clear();
}

void FlatTextureManager::graphics(Rhi& rhi, Handle<GraphicsContext> ctx)
{
}

void FlatTextureManager::postpass(Rhi& rhi)
{
}

Handle<Texture> FlatTextureManager::find_or_create_indexed(Rhi& rhi, lumpnum_t lump)
{
	SRB2_ASSERT(lump != LUMPERROR);

	auto flat_itr = flats_.find(lump);
	if (flat_itr != flats_.end())
	{
		return flat_itr->second;
	}

	uint32_t flat_size = get_flat_size(lump);
	Handle<Texture> new_tex = rhi.create_texture({
		TextureFormat::kLuminanceAlpha,
		flat_size,
		flat_size,
		TextureWrapMode::kRepeat,
		TextureWrapMode::kRepeat
	});
	flats_.insert({lump, new_tex});
	to_upload_.push_back(lump);
	return new_tex;
}

Handle<Texture> FlatTextureManager::find_indexed(lumpnum_t lump) const
{
	SRB2_ASSERT(lump != LUMPERROR);

	auto flat_itr = flats_.find(lump);
	if (flat_itr != flats_.end())
	{
		return flat_itr->second;
	}
	return kNullHandle;
}
