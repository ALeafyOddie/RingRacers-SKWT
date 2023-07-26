// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2004-2020 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  y_inter.c
/// \brief Tally screens, or "Intermissions" as they were formally called in Doom

#include "doomdef.h"
#include "doomstat.h"
#include "d_main.h"
#include "f_finale.h"
#include "g_game.h"
#include "hu_stuff.h"
#include "i_net.h"
#include "i_video.h"
#include "p_tick.h"
#include "r_defs.h"
#include "r_skins.h"
#include "s_sound.h"
#include "st_stuff.h"
#include "v_video.h"
#include "w_wad.h"
#include "y_inter.h"
#include "z_zone.h"
#include "k_menu.h"
#include "m_misc.h"
#include "i_system.h"
#include "p_setup.h"

#include "r_local.h"
#include "r_fps.h"
#include "p_local.h"

#include "m_cond.h" // condition sets
#include "lua_hook.h" // IntermissionThinker hook

#include "lua_hud.h"
#include "lua_hudlib_drawlist.h"

#include "m_random.h" // M_RandomKey
#include "g_input.h" // G_PlayerInputDown
#include "k_hud.h" // K_DrawMapThumbnail
#include "k_battle.h"
#include "k_boss.h"
#include "k_pwrlv.h"
#include "k_grandprix.h"
#include "k_serverstats.h" // SV_BumpMatchStats
#include "m_easing.h"

#ifdef HWRENDER
#include "hardware/hw_main.h"
#endif

typedef struct
{
	char patch[9];
	 INT32 points;
	UINT8 display;
} y_bonus_t;

static y_data_t data;

// graphics
static patch_t *bgpatch = NULL;     // INTERSCR
static patch_t *widebgpatch = NULL;
static patch_t *bgtile = NULL;      // SPECTILE/SRB2BACK
static patch_t *interpic = NULL;    // custom picture defined in map header

static INT32 timer;
static INT32 powertype = PWRLV_DISABLED;

static INT32 intertic;
static INT32 endtic = -1;
static INT32 sorttic = -1;
static INT32 replayprompttic;

static fixed_t mqscroll = 0;
static fixed_t chkscroll = 0;

intertype_t intertype = int_none;

static huddrawlist_h luahuddrawlist_intermission;

static void Y_UnloadData(void);

//
// SRB2Kart - Y_CalculateMatchData and ancillary functions
//
static void Y_CompareTime(INT32 i)
{
	UINT32 val = ((players[i].pflags & PF_NOCONTEST || players[i].realtime == UINT32_MAX)
		? (UINT32_MAX-1) : players[i].realtime);

	if (!(val < data.val[data.numplayers]))
		return;

	data.val[data.numplayers] = val;
	data.num[data.numplayers] = i;
}

static void Y_CompareScore(INT32 i)
{
	UINT32 val = ((players[i].pflags & PF_NOCONTEST)
			? (UINT32_MAX-1) : players[i].roundscore);

	if (!(data.val[data.numplayers] == UINT32_MAX
	|| (!(players[i].pflags & PF_NOCONTEST) && val > data.val[data.numplayers])))
		return;

	data.val[data.numplayers] = val;
	data.num[data.numplayers] = i;
}

static void Y_CompareRank(INT32 i)
{
	INT16 increase = ((data.increase[i] == INT16_MIN) ? 0 : data.increase[i]);
	UINT32 score = players[i].score;

	if (powertype != PWRLV_DISABLED)
	{
		score = clientpowerlevels[i][powertype];
	}

	if (!(data.val[data.numplayers] == UINT32_MAX || (score - increase) > data.val[data.numplayers]))
		return;

	data.val[data.numplayers] = (score - increase);
	data.num[data.numplayers] = i;
}

static void Y_CalculateMatchData(UINT8 rankingsmode, void (*comparison)(INT32))
{
	INT32 i, j;
	boolean completed[MAXPLAYERS];
	INT32 numplayersingame = 0;
	boolean getmainplayer = false;

	// Initialize variables
	if (rankingsmode > 1)
		;
	else if ((data.rankingsmode = (boolean)rankingsmode))
	{
		sprintf(data.headerstring, "Total Rankings");
		data.gotthrough = false;
	}
	else
	{
		getmainplayer = true;

		data.encore = encoremode;

		memset(data.jitter, 0, sizeof (data.jitter));
	}

	for (i = 0; i < MAXPLAYERS; i++)
	{
		data.val[i] = UINT32_MAX;

		if (!playeringame[i] || players[i].spectator)
		{
			data.increase[i] = INT16_MIN;
			continue;
		}

		if (!rankingsmode)
			data.increase[i] = INT16_MIN;

		numplayersingame++;
	}

	memset(data.color, 0, sizeof (data.color));
	memset(data.character, 0, sizeof (data.character));
	memset(completed, 0, sizeof (completed));
	data.numplayers = 0;
	data.roundnum = 0;

	data.isduel = (numplayersingame <= 2);

	for (j = 0; j < numplayersingame; j++)
	{
		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (!playeringame[i] || players[i].spectator || completed[i])
				continue;

			comparison(i);
		}

		i = data.num[data.numplayers];

		completed[i] = true;

		data.color[data.numplayers] = players[i].skincolor;
		data.character[data.numplayers] = players[i].skin;

		if (data.numplayers && (data.val[data.numplayers] == data.val[data.numplayers-1]))
		{
			data.pos[data.numplayers] = data.pos[data.numplayers-1];
		}
		else
		{
			data.pos[data.numplayers] = data.numplayers+1;
		}

#define strtime data.strval[data.numplayers]

		strtime[0] = '\0';

		if (!rankingsmode)
		{
			if ((powertype == PWRLV_DISABLED)
				&& !(players[i].pflags & PF_NOCONTEST)
				&& (data.pos[data.numplayers] < (numplayersingame + spectateGriefed)))
			{
				// Online rank is handled further below in this file.
				data.increase[i] = K_CalculateGPRankPoints(data.pos[data.numplayers], numplayersingame + spectateGriefed);
				players[i].score += data.increase[i];
			}

			if (demo.recording)
			{
				G_WriteStanding(
					data.pos[data.numplayers],
					player_names[i],
					data.character[data.numplayers],
					data.color[data.numplayers],
					data.val[data.numplayers]
				);
			}

			if (data.val[data.numplayers] == (UINT32_MAX-1))
				STRBUFCPY(strtime, "RETIRED.");
			else
			{
				if (intertype == int_time)
				{
					snprintf(strtime, sizeof strtime, "%i'%02i\"%02i", G_TicsToMinutes(data.val[data.numplayers], true),
					G_TicsToSeconds(data.val[data.numplayers]), G_TicsToCentiseconds(data.val[data.numplayers]));
				}
				else
				{
					snprintf(strtime, sizeof strtime, "%d", data.val[data.numplayers]);
				}
			}
		}
		else
		{
			if (powertype != PWRLV_DISABLED && !clientpowerlevels[i][powertype])
			{
				// No power level (guests)
				STRBUFCPY(strtime, "----");
			}
			else
			{
				snprintf(strtime, sizeof strtime, "%d", data.val[data.numplayers]);
			}
		}

		strtime[sizeof strtime - 1] = '\0';

#undef strtime

		data.numplayers++;
	}

	if (getmainplayer == true)
	{
		// Okay, player scores have been set now - we can calculate GP-relevant material.
		{
			K_UpdateGPRank();

			// See also G_GetNextMap, M_DrawPause
			data.showrank = false;
			if (grandprixinfo.gp == true
				&& netgame == false // TODO netgame Special Mode support
				&& grandprixinfo.gamespeed >= KARTSPEED_NORMAL
				&& roundqueue.size > 1
				&& roundqueue.entries[roundqueue.size - 1].rankrestricted == true
			)
			{
				if (roundqueue.position == roundqueue.size-1)
				{
					// On A rank pace? Then you get a chance for S rank!
					gp_rank_e rankforline = K_CalculateGPGrade(&grandprixinfo.rank);

					data.showrank = (rankforline >= GRADE_A);

					data.linemeter =
						(min(rankforline, GRADE_A)
							* (2 * TICRATE)
						) / GRADE_A;

					// A little extra time to take it all in
					timer += TICRATE;
				}

				if (gamedata->everseenspecial == true
					|| roundqueue.position == roundqueue.size)
				{
					// Additional cases in which it should always be shown.
					data.showrank = true;
				}
			}
		}

		i = MAXPLAYERS;

		for (j = 0; j < data.numplayers; j++)
		{
			i = data.num[j];

			if (i >= MAXPLAYERS
				|| playeringame[i] == false
				|| players[i].spectator == true)
			{
				continue;
			}

			if (demo.playback)
			{
				if (!P_IsDisplayPlayer(&players[i]))
				{
					continue;
				}

				break;
			}

			if (!P_IsLocalPlayer(&players[i]))
			{
				continue;
			}

			break;
		}

		data.headerstring[0] = '\0';
		data.gotthrough = false;
		data.mainplayer = MAXPLAYERS;

		if (j < data.numplayers)
		{
			data.mainplayer = i;

			if (!(players[i].pflags & PF_NOCONTEST))
			{
				data.gotthrough = true;

				if (players[i].skin < numskins)
				{
					snprintf(data.headerstring,
						sizeof data.headerstring,
						"%s",
						skins[players[i].skin].realname);
				}

				if (roundqueue.size > 0
					&& roundqueue.roundnum > 0
					&& (grandprixinfo.gp == false
						|| grandprixinfo.eventmode == GPEVENT_NONE)
					)
				{
					data.roundnum = roundqueue.roundnum;
				}
			}
			else
			{
				snprintf(data.headerstring,
					sizeof data.headerstring,
					"NO CONTEST...");
			}
		}
		else
		{
			if (roundqueue.size > 0
				&& roundqueue.roundnum > 0
				&& (grandprixinfo.gp == false
					|| grandprixinfo.eventmode == GPEVENT_NONE)
				)
			{
				snprintf(data.headerstring,
					sizeof data.headerstring,
					"ROUND");

				data.roundnum = roundqueue.roundnum;
			}
			else if (bossinfo.valid == true && bossinfo.enemyname)
			{
				snprintf(data.headerstring,
					sizeof data.headerstring,
					"%s",
					bossinfo.enemyname);
			}
			else if (battleprisons == true)
			{
				snprintf(data.headerstring,
					sizeof data.headerstring,
					"PRISON BREAK");
			}
			else
			{
				snprintf(data.headerstring,
					sizeof data.headerstring,
					"%s STAGE",
					gametypes[gametype]->name);
			}
		}

		data.headerstring[sizeof data.headerstring - 1] = '\0';
	}
}

typedef enum
{
	BPP_AHEAD,
	BPP_DONE,
	BPP_MAIN,
	BPP_SHADOW = BPP_MAIN,
	BPP_MAX
} bottomprogressionpatch_t;

//
// Y_PlayerStandingsDrawer
//
// Handles drawing the center-of-screen player standings.
//
void Y_PlayerStandingsDrawer(y_data_t *standings, INT32 xoffset)
{
	if (standings->numplayers == 0)
	{
		return;
	}

	UINT8 i;

	SINT8 yspacing = 14;
	INT32 heightcount = (standings->numplayers - 1);

	INT32 x, y;
	INT32 x2, returny, inwardshim = 0;

	boolean verticalresults = (standings->numplayers < 4 && (standings->numplayers == 1 || standings->isduel == false));
	boolean datarightofcolumn = false;
	boolean drawping = (netgame && gamestate == GS_LEVEL);

	INT32 hilicol = highlightflags;

	patch_t *resbar = W_CachePatchName("R_RESBAR", PU_PATCH); // Results bars for players

	if (drawping || standings->rankingsmode != 0)
	{
		inwardshim = 8;
	}

	if (verticalresults)
	{
		x = (BASEVIDWIDTH/2) - 61;
	}
	else
	{
		x = 29;
		inwardshim /= 2;
		heightcount /= 2;
	}

	x += xoffset + inwardshim;
	x2 = x;

	if (drawping)
	{
		x2 -= 9;
	}

	if (standings->numplayers > 10)
	{
		yspacing--;
	}
	else if (standings->numplayers <= 6)
	{
		yspacing++;
		if (verticalresults)
		{
			yspacing++;
		}
	}

	y = 106 - (heightcount * yspacing)/2;

	if (standings->isduel)
	{
		y += 38;
	}
	else if (y < 70)
	{
		// One sanity check.
		y = 70;
	}

	returny = y;

	boolean (*_isHighlightedPlayer)(player_t *) =
		(demo.playback
			? P_IsDisplayPlayer
			: P_IsLocalPlayer
		);

	boolean doreverse = (
		standings->isduel && standings->numplayers == 2
		&& standings->num[0] > standings->num[1]
	);

	i = 0;
	UINT8 halfway = (standings->numplayers-1)/2;
	if (doreverse)
	{
		i = standings->numplayers-1;
		halfway++;
	}

	do // don't use "continue" in this loop just for sanity's sake
	{
		const UINT8 pnum = standings->num[i];

		if (pnum == MAXPLAYERS)
			;
		else if (!playeringame[pnum] || players[pnum].spectator == true)
			standings->num[i] = MAXPLAYERS; // this should be the only field setting in this function
		else
		{
			UINT8 *charcolormap = NULL;
			if (standings->color[i] != SKINCOLOR_NONE)
			{
				charcolormap = R_GetTranslationColormap(standings->character[i], standings->color[i], GTC_CACHE);
			}

			if (standings->isduel)
			{
				INT32 duelx = x + 25 - inwardshim/2, duely = y - 80;
				if (datarightofcolumn)
					duelx += inwardshim/2;
				else
					duelx -= inwardshim/2;

				V_DrawScaledPatch(duelx, duely, 0, W_CachePatchName("DUELGRPH", PU_CACHE));
				V_DrawScaledPatch(duelx + 8, duely + 9, V_TRANSLUCENT, W_CachePatchName("PREVBACK", PU_CACHE));

				UINT8 spr2 = SPR2_STIN;
				if (standings->pos[i] == 2)
				{
					spr2 = (datarightofcolumn ? SPR2_STGR : SPR2_STGL);
				}

				M_DrawCharacterSprite(
					duelx + 40, duely + 78,
					standings->character[i],
					spr2,
					(datarightofcolumn ? 1 : 7),
					0,
					0,
					charcolormap
				);

				if (!netgame)
				{
					UINT8 j, profilen = 0;
					for (j = 0; j <= splitscreen; j++)
					{
						if (pnum == g_localplayers[j])
							break;
					}

					if (j > splitscreen)
						continue;

					profilen = cv_lastprofile[j].value;

					duelx += 8;
					duely += 5;

					INT32 backx = duelx + (datarightofcolumn ? -1 : 11);

					V_DrawScaledPatch(backx, duely, 0, W_CachePatchName("FILEBACK", PU_CACHE));

					V_DrawScaledPatch(duelx + (datarightofcolumn ? 44 : 0), duely, 0, W_CachePatchName(va("CHARSEL%c", 'A' + j), PU_CACHE));

					profile_t *pr = PR_GetProfile(profilen);
					V_DrawCenteredFileString(backx+26, duely, 0, pr ? pr->profilename : "PLAYER");
				}
			}

			// Apply the jitter offset (later reversed)
			if (standings->jitter[pnum] > 0)
				y--;

			V_DrawMappedPatch(x, y, 0, resbar, NULL);

			V_DrawRightAlignedThinString(x+13, y-2, 0, va("%d", standings->pos[i]));

			if (standings->color[i] != SKINCOLOR_NONE)
			{
				if ((players[pnum].pflags & PF_NOCONTEST) && players[pnum].bot)
				{
					// RETIRED !!
					V_DrawMappedPatch(
						x+14, y-5,
						0,
						W_CachePatchName("MINIDEAD", PU_CACHE),
						R_GetTranslationColormap(TC_DEFAULT, standings->color[i], GTC_CACHE)
					);
				}
				else
				{
					charcolormap = R_GetTranslationColormap(standings->character[i], standings->color[i], GTC_CACHE);
					V_DrawMappedPatch(x+14, y-5, 0, faceprefix[standings->character[i]][FACE_MINIMAP], charcolormap);
				}
			}

/*			y2 = y;

			if ((netgame || (demo.playback && demo.netgame)) && playerconsole[pnum] == 0 && server_lagless && !players[pnum].bot)
			{
				static UINT8 alagles_timer = 0;
				patch_t *alagles;

				y2 = ( y - 4 );

				V_DrawScaledPatch(x + 36, y2, 0, W_CachePatchName(va("BLAGLES%d", (intertic / 3) % 6), PU_CACHE));
				// every 70 tics
				if (( leveltime % 70 ) == 0)
				{
					alagles_timer = 9;
				}
				if (alagles_timer > 0)
				{
					alagles = W_CachePatchName(va("ALAGLES%d", alagles_timer), PU_CACHE);
					V_DrawScaledPatch(x + 36, y2, 0, alagles);
					if (( leveltime % 2 ) == 0)
						alagles_timer--;
				}
				else
				{
					alagles = W_CachePatchName("ALAGLES0", PU_CACHE);
					V_DrawScaledPatch(x + 36, y2, 0, alagles);
				}

				y2 += SHORT (alagles->height) + 1;
			}*/

			V_DrawThinString(
				x+27, y-2,
				(
					_isHighlightedPlayer(&players[pnum])
						? hilicol
						: 0
				),
				player_names[pnum]
			);

			V_DrawRightAlignedThinString(
				x+118, y-2,
				0,
				standings->strval[i]
			);

			if (drawping)
			{
				if (players[pnum].bot)
				{
					/*V_DrawScaledPatch(
						x2, y-1,
						0,
						kp_cpu
					);*/
				}
				else if (pnum != serverplayer || !server_lagless)
				{
					HU_drawPing(
						(x2 - 2) * FRACUNIT, (y-2) * FRACUNIT,
						playerpingtable[pnum],
						0,
						false,
						(datarightofcolumn ? 1 : -1)
					);
				}
			}
			else if (standings->rankingsmode != 0)
			{
				char *increasenum = NULL;

				if (standings->increase[pnum] != INT16_MIN)
				{
					increasenum = va(
						"(%d)",
						standings->increase[pnum]
					);
				}

				if (increasenum)
				{
					if (datarightofcolumn)
					{
						V_DrawThinString(
							x2, y-2,
							0,
							increasenum
						);
					}
					else
					{
						V_DrawRightAlignedThinString(
							x2, y-2,
							0,
							increasenum
						);
					}
				}

			}

			// Reverse the jitter offset
			if (standings->jitter[pnum] > 0)
				y++;
		}

		y += yspacing;

		if (verticalresults == false && i == halfway)
		{
			x = 169 + xoffset - inwardshim;
			y = returny;

			datarightofcolumn = true;
			x2 = x + 118 + 5;
		}

		if (!doreverse)
		{
			if (++i < standings->numplayers)
				continue;
			break;
		}

		if (i == 0)
			break;
		i--;
	}
	while (true);
}

//
// Y_RoundQueueDrawer
//
// Handles drawing the bottom-of-screen progression.
// Currently requires intermission y_data for animation only.
//
void Y_RoundQueueDrawer(y_data_t *standings, INT32 offset, boolean doanimations, boolean widescreen)
{
	if (roundqueue.size == 0)
	{
		return;
	}

	// The following is functionally a hack.
	// Due to how interpolation works, it's functionally one frame behind.
	// So we offset certain interpolated timers by this to make our lives easier!
	// This permits cues handled in the ticker and visuals to match up,
	// like the player pin reaching the Sealed Star the frame of the fade.
	// We also do this rather than doing extrapoleration because that would
	// still put 35fps in the future. ~toast 100523
	SINT8 interpoffs = (R_UsingFrameInterpolation() ? 1 : 0);

	UINT8 i;

	UINT8 *greymap = R_GetTranslationColormap(TC_DEFAULT, SKINCOLOR_GREY, GTC_CACHE);

	INT32 baseflags = (widescreen ? V_SNAPTOBOTTOM : 0);
	INT32 bufferspace = ((vid.width/vid.dupx) - BASEVIDWIDTH) / 2;

	// Background pieces
	patch_t *queuebg_flat = W_CachePatchName("R_RMBG1", PU_PATCH);
	patch_t *queuebg_upwa = W_CachePatchName("R_RMBG2", PU_PATCH);
	patch_t *queuebg_down = W_CachePatchName("R_RMBG3", PU_PATCH);
	patch_t *queuebg_prize = W_CachePatchName("R_RMBG4", PU_PATCH);

	// Progression lines
	patch_t *line_upwa[BPP_MAX];
	patch_t *line_down[BPP_MAX];
	patch_t *line_flat[BPP_MAX];

	line_upwa[BPP_AHEAD] = W_CachePatchName("R_RRMLN1", PU_PATCH);
	line_upwa[BPP_DONE] = W_CachePatchName("R_RRMLN3", PU_PATCH);
	line_upwa[BPP_SHADOW] = W_CachePatchName("R_RRMLS1", PU_PATCH);

	line_down[BPP_AHEAD] = W_CachePatchName("R_RRMLN2", PU_PATCH);
	line_down[BPP_DONE] = W_CachePatchName("R_RRMLN4", PU_PATCH);
	line_down[BPP_SHADOW] = W_CachePatchName("R_RRMLS2", PU_PATCH);

	line_flat[BPP_AHEAD] = W_CachePatchName("R_RRMLN5", PU_PATCH);
	line_flat[BPP_DONE] = W_CachePatchName("R_RRMLN6", PU_PATCH);
	line_flat[BPP_SHADOW] = W_CachePatchName("R_RRMLS3", PU_PATCH);

	// Progress markers
	patch_t *level_dot[BPP_MAIN];
	patch_t *capsu_dot[BPP_MAIN];
	patch_t *prize_dot[BPP_MAIN];

	level_dot[BPP_AHEAD] = W_CachePatchName("R_RRMRK2", PU_PATCH);
	level_dot[BPP_DONE] = W_CachePatchName("R_RRMRK1", PU_PATCH);

	capsu_dot[BPP_AHEAD] = W_CachePatchName("R_RRMRK3", PU_PATCH);
	capsu_dot[BPP_DONE] = W_CachePatchName("R_RRMRK5", PU_PATCH);

	prize_dot[BPP_AHEAD] = W_CachePatchName("R_RRMRK4", PU_PATCH);
	prize_dot[BPP_DONE] = W_CachePatchName("R_RRMRK6", PU_PATCH);

	UINT8 *colormap = NULL, *oppositemap = NULL;
	fixed_t playerx = 0, playery = 0;
	UINT8 pskin = MAXSKINS;
	UINT16 pcolor = SKINCOLOR_WHITE;

	if (standings->mainplayer == MAXPLAYERS)
	{
		;
	}
	else if (playeringame[standings->mainplayer] == false)
	{
		standings->mainplayer = MAXPLAYERS;
	}
	else if (players[standings->mainplayer].spectator == false
		&& players[standings->mainplayer].skin < numskins
		&& players[standings->mainplayer].skincolor != SKINCOLOR_NONE
		&& players[standings->mainplayer].skincolor < numskincolors
	)
	{
		pskin = players[standings->mainplayer].skin;
		pcolor = players[standings->mainplayer].skincolor;
	}

	colormap = R_GetTranslationColormap(TC_DEFAULT, pcolor, GTC_CACHE);
	oppositemap = R_GetTranslationColormap(TC_DEFAULT, skincolors[pcolor].invcolor, GTC_CACHE);

	UINT8 workingqueuesize = roundqueue.size;
	boolean upwa = false;

	if (roundqueue.size > 1
		&& roundqueue.entries[roundqueue.size - 1].rankrestricted == true
	)
	{
		if (roundqueue.size & 1)
		{
			upwa = true;
		}

		workingqueuesize--;
	}

	INT32 widthofroundqueue = 24*(workingqueuesize - 1);

	INT32 x = (BASEVIDWIDTH - widthofroundqueue) / 2;
	INT32 y, basey = 167 + offset;

	INT32 spacetospecial = 0;

	// The following block handles horizontal easing of the
	// progression bar on the last non-rankrestricted round.
	if (standings->showrank == true)
	{
		fixed_t percentslide = 0;
		SINT8 deferxoffs = 0;

		const INT32 desiredx2 = (290 + bufferspace);
		spacetospecial = max(desiredx2 - widthofroundqueue - (24 - bufferspace), 16);

		if (roundqueue.position == roundqueue.size)
		{
			percentslide = FRACUNIT;
		}
		else if (doanimations
			&& roundqueue.position == roundqueue.size-1
			&& timer - interpoffs <= 3*TICRATE)
		{
			const INT32 through = (3*TICRATE) - (timer - interpoffs - 1);
			const INT32 slidetime = (TICRATE/2);

			if (through >= slidetime)
			{
				percentslide = FRACUNIT;
			}
			else
			{
					percentslide = R_InterpolateFixed(
						(through - 1) * FRACUNIT,
						(through * FRACUNIT)
					) / slidetime;
			}
		}

		if (percentslide != 0)
		{
			const INT32 differencetocover = (x + widthofroundqueue + spacetospecial - desiredx2);

			if (percentslide == FRACUNIT)
			{
				x -= (differencetocover + deferxoffs);
			}
			else
			{
				x -= Easing_OutCubic(
					percentslide,
					0,
					differencetocover * FRACUNIT
				) / FRACUNIT;
			}
		}
	}

	// Fill in background to left edge of screen
	fixed_t xiter = x;

	if (upwa == true)
	{
		xiter -= 24;
		V_DrawMappedPatch(xiter, basey, baseflags, queuebg_upwa, greymap);
	}

	while (xiter > -bufferspace)
	{
		xiter -= 24;
		V_DrawMappedPatch(xiter, basey, baseflags, queuebg_flat, greymap);
	}

	for (i = 0; i < workingqueuesize; i++)
	{
		// Draw the background, and grab the appropriate line, to the right of the dot
		patch_t **choose_line = NULL;

		upwa ^= true;
		if (upwa == false)
		{
			y = basey + 4;

			V_DrawMappedPatch(x, basey, baseflags, queuebg_down, greymap);

			if (i+1 != workingqueuesize) // no more line?
			{
				choose_line = line_down;
			}
		}
		else
		{
			y = basey + 12;

			if (i+1 != workingqueuesize) // no more line?
			{
				V_DrawMappedPatch(x, basey, baseflags, queuebg_upwa, greymap);

				choose_line = line_upwa;
			}
			else
			{
				V_DrawMappedPatch(x, basey, baseflags, queuebg_flat, greymap);
			}
		}

		if (roundqueue.position == i+1)
		{
			playerx = (x * FRACUNIT);
			playery = (y * FRACUNIT);

			// If there's standard progression ahead of us, visibly move along it.
			if (
				doanimations
				&& choose_line != NULL
				&& timer - interpoffs <= 2*TICRATE
			)
			{
				// 8 tics is chosen because it plays nice
				// with both the x and y distance to cover.
				fixed_t through = (2*TICRATE) - (timer - interpoffs - 1);;

				if (through > 8)
				{
					if (through == 9 + interpoffs)
					{
						// Impactful landing
						playery += FRACUNIT;
					}

					through = 8 * FRACUNIT;
				}
				else
				{
					through = R_InterpolateFixed(
						(through - 1) * FRACUNIT,
						(through * FRACUNIT)
					);
				}

				// 24 pixels when all is said and done
				playerx += through * 3;

				if (upwa == false)
				{
					playery += through;
				}
				else
				{
					playery -= through;
				}

				if (through > 0 && through < 8 * FRACUNIT)
				{
					// Hoparabola and a skip.
					const fixed_t jumpfactor = through - (4 * FRACUNIT);
					// jumpfactor squared goes through 36 -> 0 -> 36.
					// 12 pixels is an arbitrary jump height, but we match it to invert the parabola.
					playery -= ((12 * FRACUNIT)
							- (FixedMul(jumpfactor, jumpfactor) / 3)
					);
				}
			}
			// End of the moving along
		}

		if (choose_line != NULL)
		{
			// Draw the line to the right of the dot

			V_DrawMappedPatch(
				x - 1, basey + 11,
				baseflags,
				choose_line[BPP_SHADOW],
				NULL
			);

			boolean lineisfull = false, recttoclear = false;

			if (roundqueue.position > i+1)
			{
				lineisfull = true;
			}
			else if (
				doanimations == true
				&& roundqueue.position == i+1
				&& timer - interpoffs <= 2*TICRATE
			)
			{
				// 8 tics is chosen because it plays nice
				// with both the x and y distance to cover.
				const INT32 through = (2*TICRATE) - (timer - interpoffs - 1);

				if (through == 0)
				{
					; // no change...
				}
				else if (through > 8)
				{
					lineisfull = true;
				}
				else
				{
					V_DrawMappedPatch(
						x - 1, basey + 12,
						baseflags,
						choose_line[BPP_DONE],
						colormap
					);

					V_SetClipRect(
						playerx + FRACUNIT,
						0,
						(BASEVIDWIDTH + bufferspace) << FRACBITS,
						BASEVIDHEIGHT << FRACBITS,
						baseflags
					);

					recttoclear = true;
				}
			}

			V_DrawMappedPatch(
				x - 1, basey + 12,
				baseflags,
				choose_line[lineisfull ? BPP_DONE : BPP_AHEAD],
				lineisfull ? colormap : NULL
			);

			if (recttoclear == true)
			{
				V_ClearClipRect();
			}
		}
		else
		{
			// No more line! Fill in background to right edge of screen
			xiter = x;
			while (xiter < BASEVIDWIDTH + bufferspace)
			{
				xiter += 24;
				V_DrawMappedPatch(xiter, basey, baseflags, queuebg_flat, greymap);
			}

			// Handle special entry on the end
			// (has to be drawn before the semifinal dot due to overlap)
			if (standings->showrank == true)
			{
				const fixed_t x2 = x + spacetospecial;

				if (roundqueue.position == roundqueue.size)
				{
					playerx = (x2 * FRACUNIT);
					playery = (y * FRACUNIT);
				}
				else if (
					doanimations == true
					&& roundqueue.position == roundqueue.size-1
					&& timer - interpoffs <= 2*TICRATE
				)
				{
					const INT32 through = ((2*TICRATE) - (timer - interpoffs - 1));
					fixed_t linefill;

					if (through > standings->linemeter)
					{
						linefill = standings->linemeter * FRACUNIT;

						// Small judder if there's enough time for it
						if (timer <= 2)
						{
							;
						}
						else if (through == (standings->linemeter + 1 + interpoffs))
						{
							playerx += FRACUNIT;
						}
						else if (through == (standings->linemeter + 2 + interpoffs))
						{
							playerx -= FRACUNIT;
						}
					}
					else
					{
						linefill = R_InterpolateFixed(
							(through - 1) * FRACUNIT,
							(through * FRACUNIT)
						);
					}

					const fixed_t percent = FixedDiv(
							linefill,
							(2*TICRATE) * FRACUNIT
						);

					playerx +=
						FixedMul(
							(x2 - x) * FRACUNIT,
							percent
						);
				}

				// Special background bump
				V_DrawMappedPatch(x2 - 13, basey, baseflags, queuebg_prize, greymap);

				// Draw the final line
				const fixed_t barstart = x + 6;
				const fixed_t barend = x2 - 6;

				if (barend - 2 >= barstart)
				{
					boolean lineisfull = false, recttoclear = false;

					xiter = barstart;

					if (playerx >= (barend + 1) * FRACUNIT)
					{
						lineisfull = true;
					}
					else if (playerx <= (barstart - 1) * FRACUNIT)
					{
						;
					}
					else
					{
						const fixed_t fillend = min((playerx / FRACUNIT) + 2, barend);

						while (xiter < fillend)
						{
							V_DrawMappedPatch(
								xiter - 1, basey + 10,
								baseflags,
								line_flat[BPP_SHADOW],
								NULL
							);

							V_DrawMappedPatch(
								xiter - 1, basey + 12,
								baseflags,
								line_flat[BPP_DONE],
								colormap
							);

							xiter += 2;
						}

						// Undo the last step so we can draw the unfilled area of the patch.
						xiter -= 2;

						V_SetClipRect(
							playerx,
							0,
							(BASEVIDWIDTH + bufferspace) << FRACBITS,
							BASEVIDHEIGHT << FRACBITS,
							baseflags
						);

						recttoclear = true;
					}

					while (xiter < barend)
					{
						V_DrawMappedPatch(
							xiter - 1, basey + 10,
							baseflags,
							line_flat[BPP_SHADOW],
							NULL
						);

						V_DrawMappedPatch(
							xiter - 1, basey + 12,
							baseflags,
							line_flat[lineisfull ? BPP_DONE : BPP_AHEAD],
							lineisfull ? colormap : NULL
						);

						xiter += 2;
					}

					if (recttoclear == true)
					{
						V_ClearClipRect();
					}
				}

				// Draw the final dot
				V_DrawMappedPatch(
					x2 - 8, y,
					baseflags,
					prize_dot[roundqueue.position == roundqueue.size ? BPP_DONE : BPP_AHEAD],
					roundqueue.position == roundqueue.size ? oppositemap : colormap
				);
			}
			// End of the special entry handling
		}

		// Now draw the dot
		patch_t **chose_dot = NULL;

		if (roundqueue.entries[i].rankrestricted == true)
		{
			// This shouldn't show up in regular play, but don't hide it entirely.
			chose_dot = prize_dot;
		}
		else if (grandprixinfo.gp == true
			&& roundqueue.entries[i].gametype != roundqueue.entries[0].gametype
		)
		{
			chose_dot = capsu_dot;
		}
		else
		{
			chose_dot = level_dot;
		}

		if (chose_dot)
		{
			V_DrawMappedPatch(
				x - 8, y,
				baseflags,
				chose_dot[roundqueue.position >= i+1 ? BPP_DONE : BPP_AHEAD],
				roundqueue.position == i+1 ? oppositemap : colormap
			);
		}

		x += 24;
	}

	// Draw the player position through the round queue!
	if (playery != 0)
	{
		patch_t *rpmark[2];
		rpmark[0] = W_CachePatchName("R_RPMARK", PU_PATCH);
		rpmark[1] = W_CachePatchName("R_R2MARK", PU_PATCH);

		// Change alignment
		playerx -= (10 * FRACUNIT);
		playery -= (14 * FRACUNIT);

		if (pskin < numskins)
		{
			// Draw outline for rank icon
			V_DrawFixedPatch(
				playerx, playery,
				FRACUNIT,
				baseflags,
				rpmark[0],
				NULL
			);

			// Draw the player's rank icon
			V_DrawFixedPatch(
				playerx + FRACUNIT, playery + FRACUNIT,
				FRACUNIT,
				baseflags,
				faceprefix[pskin][FACE_RANK],
				R_GetTranslationColormap(pskin, pcolor, GTC_CACHE)
			);
		}
		else
		{
			// Draw mini arrow
			V_DrawFixedPatch(
				playerx, playery,
				FRACUNIT,
				baseflags,
				rpmark[1],
				NULL
			);
		}
	}
}

//
// Y_IntermissionDrawer
//
// Called by D_Display. Nothing is modified here; all it does is draw. (SRB2Kart: er, about that...)
// Neat concept, huh?
//
void Y_IntermissionDrawer(void)
{
	// INFO SEGMENT
	// Numbers are V_DrawRightAlignedThinString as flags
	// resbar 1 (48,82)  5 (176, 82)
	// 2 (48, 96)

	//player icon 1 (55,79) 2 (55,93) 5 (183,79)

	// If we early return, skip drawing the 3D scene (software buffer) so it doesn't clobber the frame for the wipe
	g_wipeskiprender = true;

	if (intertype == int_none || rendermode == render_none)
		return;

	g_wipeskiprender = false;

	fixed_t x;

	// Checker scroll
	patch_t *rbgchk = W_CachePatchName("R_RBGCHK", PU_PATCH);

	// Scrolling marquee
	patch_t *rrmq = W_CachePatchName("R_RRMQ", PU_PATCH);

	// Blending mask for the background
	patch_t *mask = W_CachePatchName("R_MASK", PU_PATCH);

	fixed_t mqloop = SHORT(rrmq->width)*FRACUNIT;
	fixed_t chkloop = SHORT(rbgchk->width)*FRACUNIT;

	UINT8 *bgcolor = R_GetTranslationColormap(TC_RAINBOW, SKINCOLOR_INTERMISSION, GTC_CACHE);

	// Draw the background
	K_DrawMapThumbnail(0, 0, BASEVIDWIDTH<<FRACBITS, (data.encore ? V_FLIP : 0), prevmap, bgcolor);
	
	// Draw a mask over the BG to get the correct colorization
	V_DrawMappedPatch(0, 0, V_ADD|V_TRANSLUCENT, mask, NULL);
	
	// Draw the marquee (scroll pending)
	//V_DrawMappedPatch(0, 154, V_SUBTRACT, rrmq, NULL);
	
	// Draw the checker pattern (scroll pending)
	//V_DrawMappedPatch(0, 0, V_SUBTRACT, rbgchk, NULL);
	
	for (x = -mqscroll; x < (BASEVIDWIDTH * FRACUNIT); x += mqloop)
	{
		V_DrawFixedPatch(x, 154<<FRACBITS, FRACUNIT, V_SUBTRACT, rrmq, NULL);
	}

	V_DrawFixedPatch(chkscroll, 0, FRACUNIT, V_SUBTRACT, rbgchk, NULL);
	V_DrawFixedPatch(chkscroll - chkloop, 0, FRACUNIT, V_SUBTRACT, rbgchk, NULL);

	// Animate scrolling elements if relevant
	if (!paused && !P_AutoPause())
	{
		mqscroll += renderdeltatics;
		if (mqscroll > mqloop)
			mqscroll %= mqloop;

		chkscroll += renderdeltatics;
		if (chkscroll > chkloop)
			chkscroll %= chkloop;
	}

	if (renderisnewtic)
	{
		LUA_HUD_ClearDrawList(luahuddrawlist_intermission);
		LUA_HookHUD(luahuddrawlist_intermission, HUD_HOOK(intermission));
	}
	LUA_HUD_DrawList(luahuddrawlist_intermission);

	if (!LUA_HudEnabled(hud_intermissiontally))
		goto skiptallydrawer;

	x = 0;
	if (sorttic != -1 && intertic > sorttic)
	{
		const INT32 count = (intertic - sorttic);

		if (count < 8)
			x = -((count * BASEVIDWIDTH) / 8);
		else if (count == 8)
			goto skiptallydrawer;
		else if (count < 16)
			x = (((16 - count) * BASEVIDWIDTH) / 8);
	}

	// Draw the header bar
	{
		// Header bar
		patch_t *rtpbr = W_CachePatchName("R_RTPBR", PU_PATCH);
		V_DrawMappedPatch(20 + x, 24, 0, rtpbr, NULL);

		INT32 headerx, headery, headerwidth = 0;

		if (data.gotthrough)
		{
			// GOT THROUGH ROUND
			patch_t *gthro = W_CachePatchName("R_GTHRO", PU_PATCH);
			V_DrawMappedPatch(50 + x, 42, 0, gthro, NULL);

			headerx = 51;
			headery = 7;
		}
		else
		{
			headerwidth = V_TitleCardStringWidth(data.headerstring);

			headerx = (BASEVIDWIDTH - headerwidth)/2;
			headery = 17;
		}

		// Draw round numbers
		if (data.roundnum > 0 && data.roundnum <= 10)
		{
			patch_t *roundpatch =
				W_CachePatchName(
					va("TT_RND%d", data.roundnum),
					PU_PATCH
				);

			INT32 roundx = 240;

			if (headerwidth != 0)
			{
				const INT32 roundoffset = 8 + SHORT(roundpatch->width);

				roundx = headerx + roundoffset;
				headerx -= roundoffset/2;
			}

			V_DrawMappedPatch(x + roundx, 39, 0, roundpatch, NULL);
		}

		V_DrawTitleCardString(x + headerx, headery, data.headerstring, 0, false, 0, 0);
	}

	// Returns early if there's no players to draw
	Y_PlayerStandingsDrawer(&data, x);

	// Draw bottom (and top) pieces
skiptallydrawer:
	if (!LUA_HudEnabled(hud_intermissionmessages))
		goto finalcounter;

	// Returns early if there's no roundqueue entries to draw
	Y_RoundQueueDrawer(&data, 0, true, false);

	if (netgame)
	{
		if (speedscramble != -1 && speedscramble != gamespeed)
		{
			V_DrawCenteredThinString(BASEVIDWIDTH/2, 154, highlightflags|V_SNAPTOBOTTOM,
				va(M_GetText("Next race will be %s Speed!"), kartspeed_cons_t[1+speedscramble].strvalue));
		}
	}

finalcounter:
	{
		if ((modeattacking == ATTACKING_NONE) && (demo.recording || demo.savemode == DSM_SAVED) && !demo.playback)
		{
			switch (demo.savemode)
			{
				case DSM_NOTSAVING: 
				{
					INT32 buttonx = BASEVIDWIDTH;
					INT32 buttony = 2;
					
					K_drawButtonAnim(buttonx - 76, buttony, 0, kp_button_b[1], replayprompttic);
					V_DrawRightAlignedThinString(buttonx - 55, buttony, highlightflags, "or");
					K_drawButtonAnim(buttonx - 55, buttony, 0, kp_button_x[1], replayprompttic);
					V_DrawRightAlignedThinString(buttonx - 2, buttony, highlightflags, "Save replay");
					break;	
				}
				case DSM_SAVED:
					V_DrawRightAlignedThinString(BASEVIDWIDTH - 2, 2, highlightflags, "Replay saved!");
					break;

				case DSM_TITLEENTRY:
					ST_DrawDemoTitleEntry();
					break;

				default: // Don't render any text here
					break;
			}
		}
	}

	{
		const INT32 tickDown = (timer + 1)/TICRATE;

		// See also k_vote.c
		V__DrawOneScaleString(
			2*FRACUNIT,
			(BASEVIDHEIGHT - (2+8))*FRACUNIT,
			FRACUNIT,
			0, NULL,
			OPPRF_FONT,
			va("%d", tickDown)
		);
	}

	M_DrawMenuForeground();
}

//
// Y_Ticker
//
// Manages fake score tally for single player end of act, and decides when intermission is over.
//
void Y_Ticker(void)
{
	if (intertype == int_none)
		return;

	if (demo.recording)
	{
		if (demo.savemode == DSM_NOTSAVING)
		{
			replayprompttic++;
			G_CheckDemoTitleEntry();
		} 
			
		if (demo.savemode == DSM_WILLSAVE || demo.savemode == DSM_WILLAUTOSAVE)
			G_SaveDemo();
	}

	// Check for pause or menu up in single player
	if (paused || P_AutoPause())
		return;

	LUA_HOOK(IntermissionThinker);

	intertic++;

	// Team scramble code for team match and CTF.
	// Don't do this if we're going to automatically scramble teams next round.
	/*if (G_GametypeHasTeams() && cv_teamscramble.value && !cv_scrambleonchange.value && server)
	{
		// If we run out of time in intermission, the beauty is that
		// the P_Ticker() team scramble code will pick it up.
		if ((intertic % (TICRATE/7)) == 0)
			P_DoTeamscrambling();
	}*/

	if ((timer && !--timer)
		|| (intertic == endtic))
	{
		Y_EndIntermission();
		G_AfterIntermission();
		return;
	}

	// Animation sounds for roundqueue, see Y_RoundQueueDrawer
	if (roundqueue.size != 0
		&& roundqueue.position != 0
		&& (timer - 1) <= 2*TICRATE)
	{
		const INT32 through = ((2*TICRATE) - (timer - 1));

		if (data.showrank == true
			&& roundqueue.position == roundqueue.size-1)
		{
			// Handle special entry on the end
			if (through == data.linemeter && timer > 2)
			{
				S_StopSoundByID(NULL, sfx_gpmetr);
				S_StartSound(NULL, sfx_kc50);
			}
			else if (through == 0)
			{
				S_StartSound(NULL, sfx_gpmetr);
			}
		}
		else if (through == 9
			&& roundqueue.position < roundqueue.size)
		{
			// Impactful landing
			S_StartSound(NULL, sfx_kc50);
		}
	}

	if (intertic < TICRATE || endtic != -1)
	{
		return;
	}

	if (data.rankingsmode && intertic & 1)
	{
		memset(data.jitter, 0, sizeof (data.jitter));
		return;
	}

	if (intertype == int_time || intertype == int_score)
	{
		{
			if (!data.rankingsmode && sorttic != -1 && (intertic >= sorttic + 8))
			{
				// Anything with post-intermission consequences here should also occur in Y_EndIntermission.
				K_RetireBots();
				Y_CalculateMatchData(1, Y_CompareRank);
			}

			if (data.rankingsmode && intertic > sorttic+16+(2*TICRATE))
			{
				INT32 q=0,r=0;
				boolean kaching = true;

				for (q = 0; q < data.numplayers; q++)
				{
					if (data.num[q] == MAXPLAYERS
						|| !data.increase[data.num[q]]
						|| data.increase[data.num[q]] == INT16_MIN)
					{
						continue;
					}

					r++;
					data.jitter[data.num[q]] = 1;

					if (powertype != PWRLV_DISABLED)
					{
						// Power Levels
						if (abs(data.increase[data.num[q]]) < 10)
						{
							// Not a lot of point increase left, just set to 0 instantly
							data.increase[data.num[q]] = 0;
						}
						else
						{
							SINT8 remove = 0; // default (should not happen)

							if (data.increase[data.num[q]] < 0)
								remove = -10;
							else if (data.increase[data.num[q]] > 0)
								remove = 10;

							// Remove 10 points at a time
							data.increase[data.num[q]] -= remove;

							// Still not zero, no kaching yet
							if (data.increase[data.num[q]] != 0)
								kaching = false;
						}
					}
					else
					{
						// Basic bitch points
						if (data.increase[data.num[q]])
						{
							if (--data.increase[data.num[q]])
								kaching = false;
						}
					}
				}

				if (r)
				{
					S_StartSound(NULL, (kaching ? sfx_chchng : sfx_ptally));
					Y_CalculateMatchData(2, Y_CompareRank);
				}
				/*else -- This is how to define an endtic, but we currently use timer for both SP and MP.
					endtic = intertic + 3*TICRATE;*/
			}
		}
	}
}

//
// Y_DetermineIntermissionType
//
// Determines the intermission type from the current gametype.
//
void Y_DetermineIntermissionType(void)
{
	// no intermission for GP events
	if ((grandprixinfo.gp == true && grandprixinfo.eventmode != GPEVENT_NONE)
	// or for failing in time attack mode
	|| (modeattacking && (players[consoleplayer].pflags & PF_NOCONTEST))
	// or for explicit requested skip (outside of modeattacking)
	|| (modeattacking == ATTACKING_NONE && skipstats != 0))
	{
		intertype = int_none;
		return;
	}

	// set initially
	intertype = gametypes[gametype]->intermission;

	// special cases
	if (intertype == int_scoreortimeattack)
	{
		UINT8 i = 0, nump = 0;
		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (!playeringame[i] || players[i].spectator)
				continue;
			nump++;
		}
		intertype = (nump < 2 ? int_time : int_score);
	}
}

//
// Y_StartIntermission
//
// Called by G_DoCompleted. Sets up data for intermission drawer/ticker.
//
void Y_StartIntermission(void)
{
	UINT8 i = 0, nump = 0;
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator)
			continue;
		nump++;
	}

	intertic = -1;

#ifdef PARANOIA
	if (endtic != -1)
		I_Error("endtic is dirty");
#endif

	// set player Power Level type
	powertype = K_UsingPowerLevels();

	// determine the tic the intermission ends
	// Technically cv_inttime is saved to demos... but this permits having extremely long timers for post-netgame chatting without stranding you on the intermission in netreplays.
	if (!K_CanChangeRules(false))
	{
		timer = 10*TICRATE;
	}
	else
	{
		timer = cv_inttime.value*TICRATE;
	}

	// determine the tic everybody's scores/PWR starts getting sorted
	sorttic = -1;
	if (!timer)
	{
		// Prevent a weird bug
		timer = 1;
	}
	else if (
		( // Match Race or Time Attack
			netgame == false
			&& grandprixinfo.gp == false
		)
		&& (
			modeattacking != ATTACKING_NONE // Definitely never another map
			|| ( // Any level sequence?
				roundqueue.size == 0 // No maps queued, points aren't relevant
				|| roundqueue.position == 0 // OR points from this round will be discarded
			)
		)
	)
	{
		// No PWR/global score, skip it
		// (the above is influenced by G_GetNextMap)
		timer /= 2;
	}
	else
	{
		// Minimum two seconds for match results, then two second slideover approx halfway through
		sorttic = max((timer/2) - 2*TICRATE, 2*TICRATE);
	}

	// We couldn't display the intermission even if we wanted to.
	// But we still need to give the players their score bonuses, dummy.
	//if (dedicated) return;

	// This should always exist, but just in case...
	if (prevmap >= nummapheaders || !mapheaderinfo[prevmap])
		I_Error("Y_StartIntermission: Internal map ID %d not found (nummapheaders = %d)", prevmap, nummapheaders);

	if (timer > 1 && musiccountdown == 0)
		S_ChangeMusicInternal("racent", true); // loop it

	S_ShowMusicCredit(); // Always call

	switch (intertype)
	{
		case int_score:
		{
			// Calculate who won
			Y_CalculateMatchData(0, Y_CompareScore);
			break;
		}
		case int_time:
		{
			// Calculate who won
			Y_CalculateMatchData(0, Y_CompareTime);
			break;
		}

		case int_none:
		default:
			break;
	}

	LUA_HUD_DestroyDrawList(luahuddrawlist_intermission);
	luahuddrawlist_intermission = LUA_HUD_CreateDrawList();

	if (powertype != PWRLV_DISABLED)
	{
		for (i = 0; i < MAXPLAYERS; i++)
		{
			// Kind of a hack to do this here,
			// but couldn't think of a better way.
			data.increase[i] = K_FinalPowerIncrement(
				&players[i],
				clientpowerlevels[i][powertype],
				clientPowerAdd[i]
			);
		}

		K_CashInPowerLevels();
		SV_BumpMatchStats();
	}

	if (roundqueue.size > 0 && roundqueue.position == roundqueue.size)
	{
		Automate_Run(AEV_QUEUEEND);
	}

	Automate_Run(AEV_INTERMISSIONSTART);
	bgpatch = W_CachePatchName("MENUBG", PU_STATIC);
	widebgpatch = W_CachePatchName("WEIRDRES", PU_STATIC);

	M_UpdateMenuBGImage(true);
}

// ======

//
// Y_EndIntermission
//
void Y_EndIntermission(void)
{
	if (!data.rankingsmode)
	{
		K_RetireBots();
	}

	Y_UnloadData();

	endtic = -1;
	sorttic = -1;
	intertype = int_none;
}

#define UNLOAD(x) if (x) {Patch_Free(x);} x = NULL;
#define CLEANUP(x) x = NULL;

//
// Y_UnloadData
//
static void Y_UnloadData(void)
{
	// In hardware mode, don't Z_ChangeTag a pointer returned by W_CachePatchName().
	// It doesn't work and is unnecessary.
	if (rendermode != render_soft)
		return;

	// unload the background patches
	UNLOAD(bgpatch);
	UNLOAD(widebgpatch);
	UNLOAD(bgtile);
	UNLOAD(interpic);
}
