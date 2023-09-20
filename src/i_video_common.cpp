// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2023 by Ronald "Eidolon" Kinard
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------

#include "i_video.h"

#include <algorithm>
#include <array>
#include <vector>

#include <imgui.h>
#include <tracy/tracy/Tracy.hpp>

#include "cxxutil.hpp"
#include "f_finale.h"
#include "m_misc.h"
#include "hwr2/hardware_state.hpp"
#include "hwr2/patch_atlas.hpp"
#include "hwr2/twodee.hpp"
#include "v_video.h"

// KILL THIS WHEN WE KILL OLD OGL SUPPORT PLEASE
#include "d_netcmd.h" // kill
#include "discord.h"  // kill
#include "doomstat.h" // kill
#include "s_sound.h"  // kill
#include "sdl/ogl_sdl.h"
#include "st_stuff.h" // kill

// Legacy FinishUpdate Draws
#include "d_netcmd.h"
#ifdef HAVE_DISCORDRPC
#include "discord.h"
#endif
#include "doomstat.h"
#ifdef SRB2_CONFIG_ENABLE_WEBM_MOVIES
#include "m_avrecorder.h"
#endif
#include "st_stuff.h"
#include "s_sound.h"
#include "st_stuff.h"
#include "v_video.h"

using namespace srb2;
using namespace srb2::hwr2;
using namespace srb2::rhi;

static Rhi* g_last_known_rhi = nullptr;
static bool g_imgui_frame_active = false;
static Handle<GraphicsContext> g_main_graphics_context;
static HardwareState g_hw_state;

Handle<Rhi> srb2::sys::g_current_rhi = kNullHandle;

static bool rhi_changed(Rhi* rhi)
{
	return g_last_known_rhi != rhi;
}

static void reset_hardware_state(Rhi* rhi)
{
	// The lifetime of objects pointed to by RHI Handles is determined by the RHI itself, so it is enough to simply
	// "forget" about the resources previously known.
	g_hw_state = HardwareState {};
	g_hw_state.palette_manager = std::make_unique<PaletteManager>();
	g_hw_state.flat_manager = std::make_unique<FlatTextureManager>();
	g_hw_state.patch_atlas_cache = std::make_unique<PatchAtlasCache>(2048, 3);
	g_hw_state.twodee_renderer = std::make_unique<TwodeeRenderer>(
		g_hw_state.palette_manager.get(),
		g_hw_state.flat_manager.get(),
		g_hw_state.patch_atlas_cache.get()
	);
	g_hw_state.software_screen_renderer = std::make_unique<SoftwareScreenRenderer>();
	g_hw_state.blit_postimg_screens = std::make_unique<BlitPostimgScreens>(g_hw_state.palette_manager.get());
	g_hw_state.wipe = std::make_unique<PostprocessWipePass>();
	g_hw_state.blit_rect = std::make_unique<BlitRectPass>();
	g_hw_state.screen_capture = std::make_unique<ScreenshotPass>();
	g_hw_state.backbuffer = std::make_unique<UpscaleBackbuffer>();
	g_hw_state.wipe_frames = {};

	g_last_known_rhi = rhi;
}

static void new_twodee_frame();
static void new_imgui_frame();

static void preframe_update(Rhi& rhi)
{
	SRB2_ASSERT(g_main_graphics_context != kNullHandle);

	g_hw_state.palette_manager->update(rhi, g_main_graphics_context);
	new_twodee_frame();
	new_imgui_frame();
}

static void postframe_update(Rhi& rhi)
{
	g_hw_state.palette_manager->destroy_per_frame_resources(rhi);
}

#ifdef HWRENDER
static void finish_legacy_ogl_update()
{
	int player;

	SCR_CalculateFPS();

	if (st_overlay)
	{
		if (cv_songcredits.value)
			HU_DrawSongCredits();

		if (cv_ticrate.value)
			SCR_DisplayTicRate();

		if (cv_showping.value && netgame && (consoleplayer != serverplayer || !server_lagless))
		{
			if (server_lagless)
			{
				if (consoleplayer != serverplayer)
					SCR_DisplayLocalPing();
			}
			else
			{
				for (player = 1; player < MAXPLAYERS; player++)
				{
					if (D_IsPlayerHumanAndGaming(player))
					{
						SCR_DisplayLocalPing();
						break;
					}
				}
			}
		}
		if (cv_mindelay.value && consoleplayer == serverplayer && Playing())
			SCR_DisplayLocalPing();
	}

	if (marathonmode)
		SCR_DisplayMarathonInfo();

	// draw captions if enabled
	if (cv_closedcaptioning.value)
		SCR_ClosedCaptions();

#ifdef HAVE_DISCORDRPC
	if (discordRequestList != NULL)
		ST_AskToJoinEnvelope();
#endif

	ST_drawDebugInfo();

	OglSdlFinishUpdate(cv_vidwait.value);
}
#endif

static void temp_legacy_finishupdate_draws()
{
	SCR_CalculateFPS();
	if (st_overlay)
	{
		if (cv_songcredits.value)
			HU_DrawSongCredits();

		if (cv_ticrate.value)
			SCR_DisplayTicRate();

		if (cv_showping.value && netgame && (consoleplayer != serverplayer || !server_lagless))
		{
			if (server_lagless)
			{
				if (consoleplayer != serverplayer)
					SCR_DisplayLocalPing();
			}
			else
			{
				for (int player = 1; player < MAXPLAYERS; player++)
				{
					if (D_IsPlayerHumanAndGaming(player))
					{
						SCR_DisplayLocalPing();
						break;
					}
				}
			}
		}
		if (cv_mindelay.value && consoleplayer == serverplayer && Playing())
			SCR_DisplayLocalPing();
#ifdef SRB2_CONFIG_ENABLE_WEBM_MOVIES
		M_AVRecorder_DrawFrameRate();
#endif
	}

	if (marathonmode)
		SCR_DisplayMarathonInfo();

	// draw captions if enabled
	if (cv_closedcaptioning.value)
		SCR_ClosedCaptions();

#ifdef HAVE_DISCORDRPC
	if (discordRequestList != NULL)
		ST_AskToJoinEnvelope();
#endif

	ST_drawDebugInfo();
}

static void new_twodee_frame()
{
	g_2d = Twodee();
	Patch_ResetFreedThisFrame();
}

static void new_imgui_frame()
{
	if (g_imgui_frame_active)
	{
		ImGui::EndFrame();
		g_imgui_frame_active = false;
	}
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize.x = vid.realwidth;
	io.DisplaySize.y = vid.realheight;
	ImGui::NewFrame();
	g_imgui_frame_active = true;
}

rhi::Handle<rhi::GraphicsContext> sys::main_graphics_context()
{
	return g_main_graphics_context;
}

HardwareState* sys::main_hardware_state()
{
	return &g_hw_state;
}

void I_CaptureVideoFrame()
{
	rhi::Rhi* rhi = srb2::sys::get_rhi(srb2::sys::g_current_rhi);
	rhi::Handle<rhi::GraphicsContext> ctx = srb2::sys::main_graphics_context();
	hwr2::HardwareState* hw_state = srb2::sys::main_hardware_state();

	hw_state->screen_capture->set_source(static_cast<uint32_t>(vid.width), static_cast<uint32_t>(vid.height));
	hw_state->screen_capture->capture(*rhi, ctx);
}

void I_StartDisplayUpdate(void)
{
	if (rendermode == render_none)
	{
		return;
	}

#ifdef HWRENDER
	if (rendermode == render_opengl)
	{
		return;
	}
#endif

	rhi::Rhi* rhi = sys::get_rhi(sys::g_current_rhi);

	if (rhi == nullptr)
	{
		// ???
		return;
	}

	if (rhi_changed(rhi))
	{
		// Reset all hardware 2 state
		reset_hardware_state(rhi);
	}

	rhi::Handle<rhi::GraphicsContext> ctx = rhi->begin_graphics();
	HardwareState* hw_state = &g_hw_state;

	hw_state->backbuffer->begin_pass(*rhi, ctx);

	g_main_graphics_context = ctx;

	preframe_update(*rhi);
}

void I_FinishUpdate(void)
{
	ZoneScoped;
	if (rendermode == render_none)
	{
		FrameMark;
		return;
	}

#ifdef HWRENDER
	if (rendermode == render_opengl)
	{
		finish_legacy_ogl_update();
		FrameMark;
		return;
	}
#endif

	temp_legacy_finishupdate_draws();

	rhi::Rhi* rhi = sys::get_rhi(sys::g_current_rhi);

	if (rhi == nullptr)
	{
		// ???
		FrameMark;
		return;
	}

	rhi::Handle<rhi::GraphicsContext> ctx = g_main_graphics_context;

	if (ctx != kNullHandle)
	{
		// better hope the drawing code left the context in a render pass, I guess
		g_hw_state.twodee_renderer->flush(*rhi, ctx, g_2d);
		rhi->end_render_pass(ctx);

		rhi->begin_default_render_pass(ctx, true);

		// Upscale draw the backbuffer (with postprocessing maybe?)
		g_hw_state.blit_rect->set_output(vid.realwidth, vid.realheight, true, true);
		g_hw_state.blit_rect->set_texture(g_hw_state.backbuffer->color(), static_cast<uint32_t>(vid.width), static_cast<uint32_t>(vid.height));
		g_hw_state.blit_rect->draw(*rhi, ctx);
		rhi->end_render_pass(ctx);

		rhi->end_graphics(ctx);
		g_main_graphics_context = kNullHandle;

		postframe_update(*rhi);
	}

	rhi->present();
	rhi->finish();

	FrameMark;

	// Immediately prepare to begin drawing the next frame
	I_StartDisplayUpdate();
}
