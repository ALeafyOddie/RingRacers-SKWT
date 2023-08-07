#include <algorithm>
#include <cstddef>
#include <vector>

#include "core/static_vec.hpp"

#include "k_battle.h"
#include "k_hud.h"
#include "k_kart.h"
#include "k_objects.h"
#include "m_fixed.h"
#include "p_local.h"
#include "p_mobj.h"
#include "r_draw.h"
#include "r_fps.h"
#include "r_main.h"
#include "st_stuff.h"
#include "v_video.h"

using namespace srb2;

namespace
{

struct TargetTracking
{
	mobj_t* mobj;
	vector3_t point;
	fixed_t camDist;

	skincolornum_t color() const
	{
		switch (mobj->type)
		{
		case MT_OVERTIME_CENTER:
			return SKINCOLOR_BLUE;

		case MT_MONITOR:
		case MT_EMERALD:
			return static_cast<skincolornum_t>(mobj->color);

		case MT_PLAYER:
			return player_emeralds_color();

		case MT_SUPER_FLICKY:
			return static_cast<skincolornum_t>(Obj_SuperFlickyOwner(mobj)->color);
		default:
			return SKINCOLOR_NONE;
		}
	}

	StaticVec<uint32_t, 7> player_emeralds_vec() const
	{
		StaticVec<uint32_t, 7> emeralds;

		const player_t* player = mobj->player;

		if (player == nullptr)
		{
			return emeralds;
		}

		for (int i = 0; i < 7; ++i)
		{
			const uint32_t emeraldFlag = (1U << i);

			if (player->emeralds & emeraldFlag)
			{
				emeralds.push_back(emeraldFlag);
			}
		}

		return emeralds;
	}

	skincolornum_t player_emeralds_color() const
	{
		const StaticVec emeralds = player_emeralds_vec();

		if (emeralds.empty())
		{
			return SKINCOLOR_NONE;
		}

		constexpr tic_t kPeriod = TICRATE / 2;
		const int idx = (leveltime / kPeriod) % emeralds.size();

		return static_cast<skincolornum_t>(K_GetChaosEmeraldColor(emeralds[idx]));
	}

	const uint8_t* colormap() const
	{
		const skincolornum_t clr = color();

		if (clr != SKINCOLOR_NONE)
		{
			return R_GetTranslationColormap(TC_RAINBOW, clr, GTC_CACHE);
		}

		return nullptr;
	}
};

void K_DrawTargetTracking(const TargetTracking& target)
{
	const uint8_t* colormap = target.colormap();

	trackingResult_t result = {};
	int32_t timer = 0;

	K_ObjectTracking(&result, &target.point, false);

	if (result.onScreen == false)
	{
		// Off-screen, draw alongside the borders of the screen.
		// Probably the most complicated thing.

		int32_t scrVal = 240;
		vector2_t screenSize = {};

		int32_t borderSize = 7;
		vector2_t borderWin = {};
		vector2_t borderDir = {};
		fixed_t borderLen = FRACUNIT;

		vector2_t arrowDir = {};

		vector2_t arrowPos = {};
		patch_t* arrowPatch = nullptr;
		int32_t arrowFlags = 0;

		vector2_t targetPos = {};
		patch_t* targetPatch = nullptr;

		timer = (leveltime / 3);

		screenSize.x = vid.width / vid.dupx;
		screenSize.y = vid.height / vid.dupy;

		if (r_splitscreen >= 2)
		{
			// Half-wide screens
			screenSize.x >>= 1;
			borderSize >>= 1;
		}

		if (r_splitscreen >= 1)
		{
			// Half-tall screens
			screenSize.y >>= 1;
		}

		scrVal = std::max(screenSize.x, screenSize.y) - 80;

		borderWin.x = screenSize.x - borderSize;
		borderWin.y = screenSize.y - borderSize;

		arrowDir.x = 0;
		arrowDir.y = P_MobjFlip(target.mobj) * FRACUNIT;

		// Simply pointing towards the result doesn't work, so inaccurate hack...
		borderDir.x = FixedMul(
			FixedMul(
				FINESINE((result.angle >> ANGLETOFINESHIFT) & FINEMASK),
				FINECOSINE((-result.pitch >> ANGLETOFINESHIFT) & FINEMASK)
			),
			result.fov
		);

		borderDir.y = FixedMul(FINESINE((-result.pitch >> ANGLETOFINESHIFT) & FINEMASK), result.fov);

		borderLen = R_PointToDist2(0, 0, borderDir.x, borderDir.y);

		if (borderLen > 0)
		{
			borderDir.x = FixedDiv(borderDir.x, borderLen);
			borderDir.y = FixedDiv(borderDir.y, borderLen);
		}
		else
		{
			// Eh just put it at the bottom.
			borderDir.x = 0;
			borderDir.y = FRACUNIT;
		}

		if (target.mobj->type == MT_BATTLECAPSULE
			|| target.mobj->type == MT_CDUFO)
		{
			targetPatch = kp_capsuletarget_icon[timer & 1];
		}

		if (abs(borderDir.x) > abs(borderDir.y))
		{
			// Horizontal arrow
			arrowPatch = kp_capsuletarget_arrow[1][timer & 1];
			arrowDir.y = 0;

			if (borderDir.x < 0)
			{
				// LEFT
				arrowDir.x = -FRACUNIT;
			}
			else
			{
				// RIGHT
				arrowDir.x = FRACUNIT;
			}
		}
		else
		{
			// Vertical arrow
			arrowPatch = kp_capsuletarget_arrow[0][timer & 1];
			arrowDir.x = 0;

			if (borderDir.y < 0)
			{
				// UP
				arrowDir.y = -FRACUNIT;
			}
			else
			{
				// DOWN
				arrowDir.y = FRACUNIT;
			}
		}

		arrowPos.x = (screenSize.x >> 1) + FixedMul(scrVal, borderDir.x);
		arrowPos.y = (screenSize.y >> 1) + FixedMul(scrVal, borderDir.y);

		arrowPos.x = std::clamp(arrowPos.x, borderSize, borderWin.x) * FRACUNIT;
		arrowPos.y = std::clamp(arrowPos.y, borderSize, borderWin.y) * FRACUNIT;

		if (targetPatch)
		{
			targetPos.x = arrowPos.x - (arrowDir.x * 12);
			targetPos.y = arrowPos.y - (arrowDir.y * 12);

			targetPos.x -= (targetPatch->width << FRACBITS) >> 1;
			targetPos.y -= (targetPatch->height << FRACBITS) >> 1;
		}

		arrowPos.x -= (arrowPatch->width << FRACBITS) >> 1;
		arrowPos.y -= (arrowPatch->height << FRACBITS) >> 1;

		if (arrowDir.x < 0)
		{
			arrowPos.x += arrowPatch->width << FRACBITS;
			arrowFlags |= V_FLIP;
		}

		if (arrowDir.y < 0)
		{
			arrowPos.y += arrowPatch->height << FRACBITS;
			arrowFlags |= V_VFLIP;
		}

		if (targetPatch)
		{
			V_DrawFixedPatch(targetPos.x, targetPos.y, FRACUNIT, V_SPLITSCREEN, targetPatch, colormap);
		}

		V_DrawFixedPatch(arrowPos.x, arrowPos.y, FRACUNIT, V_SPLITSCREEN | arrowFlags, arrowPatch, colormap);
	}
	else
	{
		// Draw simple overlay.
		const fixed_t farDistance = 1280 * mapobjectscale;
		bool useNear = (target.camDist < farDistance);

		vector2_t targetPos = {};

		bool visible = P_CheckSight(stplyr->mo, target.mobj); 

		if ((visible == false || target.mobj->type == MT_SUPER_FLICKY) && (leveltime & 1))
		{
			// Flicker when not visible.
			return;
		}

		targetPos.x = result.x;
		targetPos.y = result.y;

		auto draw = [&](patch_t* patch)
		{
			V_DrawFixedPatch(
				targetPos.x - ((patch->width << FRACBITS) >> 1),
				targetPos.y - ((patch->height << FRACBITS) >> 1),
				FRACUNIT,
				V_SPLITSCREEN,
				patch,
				colormap
			);
		};

		if (target.mobj->type == MT_SUPER_FLICKY)
		{
			timer = (leveltime / 2);
			draw(kp_superflickytarget[timer % 4]);
		}
		else if (useNear == true)
		{
			timer = (leveltime / 2);
			draw(kp_capsuletarget_near[timer % 8]);
		}
		else
		{
			timer = (leveltime / 3);
			draw(kp_capsuletarget_far[timer & 1]);

			if (r_splitscreen <= 1)
			{
				draw(kp_capsuletarget_far_text[timer & 1]);
			}
		}
	}
}

bool is_player_tracking_target(player_t *player = stplyr)
{
	if ((gametyperules & (GTR_BUMPERS|GTR_CLOSERPLAYERS)) != (GTR_BUMPERS|GTR_CLOSERPLAYERS))
	{
		return false;
	}

	if (K_Cooperative())
	{
		return false;
	}

	if (player == nullptr)
	{
		return false;
	}

	if (inDuel)
	{
		// Always draw targets in 1v1 but don't draw player's
		// own target on their own viewport.
		return player != stplyr;
	}

	// Except for DUEL mode, Overtime hides all TARGETs except
	// the kiosk.
	if (battleovertime.enabled)
	{
		return false;
	}

	if (player->emeralds != 0 && K_IsPlayerWanted(stplyr))
	{
		// The player who is about to win because of emeralds
		// gets a TARGET on them
		if (K_NumEmeralds(player) == 6) // 6 out of 7
		{
			return true;
		}

		// WANTED player sees TARGETs on players holding
		// emeralds
		if (K_IsPlayerWanted(stplyr))
		{
			return true;
		}
	}

	return K_IsPlayerWanted(player);
}

bool is_object_tracking_target(const mobj_t* mobj)
{
	switch (mobj->type)
	{
	case MT_BATTLECAPSULE:
	case MT_CDUFO:
		return battleprisons;

	case MT_SPECIAL_UFO:
		return true;

	case MT_PLAYER:
		return is_player_tracking_target(mobj->player);

	case MT_OVERTIME_CENTER:
		return inDuel == false && battleovertime.enabled;

	case MT_EMERALD:
		return is_player_tracking_target();

	case MT_MONITOR:
		return is_player_tracking_target() && Obj_MonitorGetEmerald(mobj) != 0;

	case MT_SUPER_FLICKY:
		return Obj_IsSuperFlickyTargettingYou(mobj, stplyr->mo);

	default:
		return false;
	}
}

}; // namespace

void K_drawTargetHUD(const vector3_t* origin, player_t* player)
{
	std::vector<TargetTracking> targetList;

	mobj_t* mobj = nullptr;
	mobj_t* next = nullptr;

	for (mobj = trackercap; mobj; mobj = next)
	{
		next = mobj->itnext;

		if (mobj->health <= 0)
		{
			continue;
		}

		if (is_object_tracking_target(mobj) == false)
		{
			continue;
		}

		vector3_t pos = {
			R_InterpolateFixed(mobj->old_x, mobj->x) + mobj->sprxoff,
			R_InterpolateFixed(mobj->old_y, mobj->y) + mobj->spryoff,
			R_InterpolateFixed(mobj->old_z, mobj->z) + mobj->sprzoff + (mobj->height >> 1),
		};

		targetList.push_back({mobj, pos, R_PointToDist2(origin->x, origin->y, pos.x, pos.y)});
	}

	// Sort by distance from camera. Further trackers get
	// drawn first so nearer ones draw over them.
	std::sort(targetList.begin(), targetList.end(), [](const auto& a, const auto& b) { return a.camDist > b.camDist; });

	std::for_each(targetList.cbegin(), targetList.cend(), K_DrawTargetTracking);
}
