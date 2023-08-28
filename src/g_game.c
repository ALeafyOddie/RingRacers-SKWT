// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2020 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  g_game.c
/// \brief game loop functions, events handling

#include "doomdef.h"
#include "console.h"
#include "d_main.h"
#include "d_clisrv.h"
#include "d_player.h"
#include "d_clisrv.h"
#include "f_finale.h"
#include "filesrch.h" // for refreshdirmenu
#include "p_setup.h"
#include "p_saveg.h"
#include "i_time.h"
#include "i_system.h"
#include "am_map.h"
#include "m_random.h"
#include "p_local.h"
#include "r_draw.h"
#include "r_main.h"
#include "s_sound.h"
#include "g_game.h"
#include "g_demo.h"
#include "m_cheat.h"
#include "m_misc.h"
#include "k_menu.h"
#include "m_argv.h"
#include "hu_stuff.h"
#include "st_stuff.h"
#include "z_zone.h"
#include "i_video.h"
#include "byteptr.h"
#include "i_joy.h"
#include "r_local.h"
#include "r_skins.h"
#include "y_inter.h"
#include "v_video.h"
#include "lua_hook.h"
#include "m_cond.h" // condition sets
#include "r_fps.h" // frame interpolation/uncapped
#include "lua_hud.h"

// SRB2kart
#include "k_kart.h"
#include "k_battle.h"
#include "k_pwrlv.h"
#include "k_color.h"
#include "k_respawn.h"
#include "k_grandprix.h"
#include "k_boss.h"
#include "k_specialstage.h"
#include "k_bot.h"
#include "doomstat.h"
#include "k_director.h"
#include "k_podium.h"
#include "k_rank.h"
#include "acs/interface.h"
#include "g_party.h"
#include "k_vote.h"
#include "k_serverstats.h"
#include "k_zvote.h"
#include "music.h"
#include "k_roulette.h"

#ifdef HAVE_DISCORDRPC
#include "discord.h"
#endif

#ifdef HWRENDER
#include "hardware/hw_main.h" // for cv_glshearing
#endif

gameaction_t gameaction;
gamestate_t gamestate = GS_NULL;
UINT8 ultimatemode = false;

JoyType_t Joystick[MAXSPLITSCREENPLAYERS];

// SRB2kart
char gamedatafilename[64] =
#if defined (TESTERS) || defined (HOSTTESTERS)
	"test"
#elif defined(DEVELOP)
	"develop"
#endif
	"ringdata.dat";
char timeattackfolder[64] = "ringracers";
char customversionstring[32] = "\0";

static void G_DoCompleted(void);
static void G_DoStartContinue(void);
static void G_DoContinued(void);
static void G_DoWorldDone(void);
static void G_DoStartVote(void);

char   mapmusname[7]; // Music name
UINT16 mapmusflags; // Track and reset bit
UINT32 mapmusposition; // Position to jump to
UINT32 mapmusresume;
UINT8 mapmusrng; // Random selection result

INT16 gamemap = 1;
UINT32 maptol;

preciptype_t globalweather = PRECIP_NONE;
preciptype_t curWeather = PRECIP_NONE;

precipprops_t precipprops[MAXPRECIP] =
{
	{"NONE",				MT_NULL, 			0}, // PRECIP_NONE
	{"RAIN",				MT_RAIN, 			0}, // PRECIP_RAIN
	{"SNOW",				MT_SNOWFLAKE,		0}, // PRECIP_SNOW
	{"BLIZZARD",			MT_BLIZZARDSNOW,	0}, // PRECIP_BLIZZARD
	{"STORM",				MT_RAIN,			PRECIPFX_THUNDER|PRECIPFX_LIGHTNING}, // PRECIP_STORM
	{"STORM_NORAIN",		MT_NULL,			PRECIPFX_THUNDER|PRECIPFX_LIGHTNING}, // PRECIP_STORM_NORAIN
	{"STORM_NOSTRIKES",		MT_RAIN,			PRECIPFX_THUNDER} // PRECIP_STORM_NOSTRIKES
};

preciptype_t precip_freeslot = PRECIP_FIRSTFREESLOT;

INT32 cursaveslot = 0; // Auto-save 1p savegame slot
UINT8 gamecomplete = 0;

marathonmode_t marathonmode = 0;
tic_t marathontime = 0;

UINT8 numgameovers = 0; // for startinglives balance
SINT8 startinglivesbalance[maxgameovers+1] = {3, 5, 7, 9, 12, 15, 20, 25, 30, 40, 50, 75, 99, 0x7F};

UINT16 mainwads = 0;
UINT16 musicwads = 0;
boolean modifiedgame = false; // Set if homebrew PWAD stuff has been added.
boolean majormods = false; // Set if Lua/Gameplay SOC/replacement map has been added.
boolean savemoddata = false;
boolean usedCheats = false; // Set when a "cheats on" is ever used.
UINT8 paused;
UINT8 modeattacking = ATTACKING_NONE;
boolean imcontinuing = false;

// menu demo things
UINT8  numDemos      = 0;
UINT32 demoDelayTime = 15*TICRATE;
UINT32 demoIdleTime  = 3*TICRATE;

boolean netgame; // only true if packets are broadcast
boolean multiplayer;
boolean playeringame[MAXPLAYERS];
boolean addedtogame;
player_t players[MAXPLAYERS];

INT32 consoleplayer; // player taking events and displaying
INT32 displayplayers[MAXSPLITSCREENPLAYERS]; // view being displayed
INT32 g_localplayers[MAXSPLITSCREENPLAYERS];

tic_t gametic;
tic_t levelstarttic; // gametic at level start
INT16 lastmap; // last level you were at (returning from special stages)
tic_t timeinmap; // Ticker for time spent in level (used for levelcard display)

char * titlemap = NULL;
boolean hidetitlepics = false;
char * bootmap = NULL; //bootmap for loading a map on startup

char * podiummap = NULL; // map to load for podium

boolean looptitle = true;

UINT16 skincolor_redteam = SKINCOLOR_RED;
UINT16 skincolor_blueteam = SKINCOLOR_BLUE;
UINT16 skincolor_redring = SKINCOLOR_RASPBERRY;
UINT16 skincolor_bluering = SKINCOLOR_PERIWINKLE;

boolean exitfadestarted = false;

cutscene_t *cutscenes[128];
textprompt_t *textprompts[MAX_PROMPTS];

UINT16 nextmapoverride;
UINT8 skipstats;

quake_t *g_quakes = NULL;

// Map Header Information
mapheader_t** mapheaderinfo = {NULL};
INT32 nummapheaders = 0;
INT32 basenummapheaders = 0;
INT32 mapallocsize = 0;

unloaded_mapheader_t *unloadedmapheaders = NULL;

// Kart cup definitions
cupheader_t *kartcupheaders = NULL;
UINT16 numkartcupheaders = 0;

unloaded_cupheader_t *unloadedcupheaders = NULL;

static boolean exitgame = false;
static boolean retrying = false;
static boolean retryingmodeattack = false;

UINT8 stagefailed; // Used for GEMS BONUS? Also to see if you beat the stage.

INT32 luabanks[NUM_LUABANKS];

// Temporary holding place for nights data for the current map
//nightsdata_t ntemprecords;

UINT32 bluescore, redscore; // CTF and Team Match team scores

// ring count... for PERFECT!
INT32 nummaprings = 0;

UINT8 nummapspraycans = 0;

// Elminates unnecessary searching.
boolean CheckForBustableBlocks;
boolean CheckForBouncySector;
boolean CheckForQuicksand;
boolean CheckForMarioBlocks;
boolean CheckForFloatBob;
boolean CheckForReverseGravity;

// Powerup durations
UINT16 invulntics = 20*TICRATE;
UINT16 sneakertics = 20*TICRATE;
UINT16 flashingtics = 3*TICRATE/2; // SRB2kart
UINT16 tailsflytics = 8*TICRATE;
UINT16 underwatertics = 30*TICRATE;
UINT16 spacetimetics = 11*TICRATE + (TICRATE/2);
UINT16 extralifetics = 4*TICRATE;
UINT16 nightslinktics = 2*TICRATE;

INT32 gameovertics = 15*TICRATE;
UINT8 ammoremovaltics = 2*TICRATE;

// SRB2kart
tic_t introtime = 3;
tic_t starttime = 3;

const tic_t bulbtime = TICRATE/2;
UINT8 numbulbs = 1;

tic_t raceexittime = 7*TICRATE + (TICRATE/2);

INT32 hyudorotime = 7*TICRATE;
INT32 stealtime = TICRATE/2;
INT32 sneakertime = TICRATE + (TICRATE/3);
INT32 itemtime = 8*TICRATE;
INT32 bubbletime = TICRATE/2;
INT32 comebacktime = 3*TICRATE;
INT32 bumptime = 6;
INT32 greasetics = 3*TICRATE;
INT32 wipeoutslowtime = 20;
INT32 wantedreduce = 5*TICRATE;
INT32 wantedfrequency = 10*TICRATE;

UINT8 use1upSound = 0;
UINT8 maxXtraLife = 2; // Max extra lives from rings

UINT8 introtoplay;
UINT8 creditscutscene;
UINT8 useSeal = 1;

// Emerald locations
mobj_t *hunt1;
mobj_t *hunt2;
mobj_t *hunt3;

tic_t racecountdown, exitcountdown, musiccountdown; // for racing
exitcondition_t g_exit;

fixed_t gravity;
fixed_t mapobjectscale;

struct maplighting maplighting;

INT16 autobalance; //for CTF team balance
INT16 teamscramble; //for CTF team scramble
INT16 scrambleplayers[MAXPLAYERS]; //for CTF team scramble
INT16 scrambleteams[MAXPLAYERS]; //for CTF team scramble
INT16 scrambletotal; //for CTF team scramble
INT16 scramblecount; //for CTF team scramble

// SRB2Kart
// Cvars that we don't want changed mid-game
UINT8 numlaps; // Removed from Cvar hell
UINT8 gamespeed; // Game's current speed (or difficulty, or cc, or etc); 0 for easy, 1 for normal, 2 for hard
boolean encoremode = false; // Encore Mode currently enabled?
boolean prevencoremode;
boolean franticitems; // Frantic items currently enabled?

// Voting system
UINT16 g_voteLevels[4][2]; // Levels that were rolled by the host
SINT8 g_votes[VOTE_TOTAL]; // Each player's vote
SINT8 g_pickedVote; // What vote the host rolls

// Server-sided, synched variables
tic_t wantedcalcdelay; // Time before it recalculates WANTED
tic_t itemCooldowns[NUMKARTITEMS - 1]; // Cooldowns to prevent item spawning
tic_t mapreset; // Map reset delay when enough players have joined an empty game
boolean thwompsactive; // Thwomps activate on lap 2
UINT8 lastLowestLap; // Last lowest lap, for activating race lap executors
SINT8 spbplace; // SPB exists, give the person behind better items
boolean rainbowstartavailable; // Boolean, keeps track of if the rainbow start was gotten
boolean inDuel; // Boolean, keeps track of if it is a 1v1

// Client-sided, unsynched variables (NEVER use in anything that needs to be synced with other players)
tic_t bombflashtimer = 0;	// Cooldown before another FlashPal can be intialized by a bomb exploding near a displayplayer. Avoids seizures.
boolean legitimateexit; // Did this client actually finish the match?
boolean comebackshowninfo; // Have you already seen the "ATTACK OR PROTECT" message?
tic_t curlap; // Current lap time
tic_t bestlap; // Best lap time

boolean precache = true; // if true, load all graphics at start

UINT16 prevmap, nextmap;

// now automatically allocated in D_RegisterClientCommands
// so that it doesn't have to be updated depending on the value of MAXPLAYERS
char player_names[MAXPLAYERS][MAXPLAYERNAME+1];
INT32 player_name_changes[MAXPLAYERS];

// MAKE SURE YOU SAVE DATA BEFORE CALLING THIS
void G_ClearRecords(void)
{
	UINT16 i;

	for (i = 0; i < numskins; i++)
	{
		memset(&skins[i].records, 0, sizeof(skins[i].records));
	}

	for (i = 0; i < nummapheaders; i++)
	{
		memset(&mapheaderinfo[i]->records, 0, sizeof(recorddata_t));
	}

	cupheader_t *cup;
	for (cup = kartcupheaders; cup; cup = cup->next)
	{
		memset(&cup->windata, 0, sizeof(cup->windata));
	}

	unloaded_skin_t *unloadedskin, *nextunloadedskin = NULL;
	for (unloadedskin = unloadedskins; unloadedskin; unloadedskin = nextunloadedskin)
	{
		nextunloadedskin = unloadedskin->next;
		Z_Free(unloadedskin);
	}
	unloadedskins = NULL;

	unloaded_mapheader_t *unloadedmap, *nextunloadedmap = NULL;
	for (unloadedmap = unloadedmapheaders; unloadedmap; unloadedmap = nextunloadedmap)
	{
		nextunloadedmap = unloadedmap->next;
		Z_Free(unloadedmap->lumpname);
		Z_Free(unloadedmap);
	}
	unloadedmapheaders = NULL;

	unloaded_cupheader_t *unloadedcup, *nextunloadedcup = NULL;
	for (unloadedcup = unloadedcupheaders; unloadedcup; unloadedcup = nextunloadedcup)
	{
		nextunloadedcup = unloadedcup->next;
		Z_Free(unloadedcup);
	}
	unloadedcupheaders = NULL;
}

// For easy retrieval of records
tic_t G_GetBestTime(INT16 map)
{
	if (!mapheaderinfo[map] || mapheaderinfo[map]->records.time <= 0)
		return (tic_t)UINT32_MAX;

	return mapheaderinfo[map]->records.time;
}

// BE RIGHT BACK

// Not needed
/*
tic_t G_GetBestLap(INT16 map)
{
	if (!mapheaderinfo[map] || mapheaderinfo[map]->records.lap <= 0)
		return (tic_t)UINT32_MAX;

	return mapheaderinfo[map]->records.lap;
}
*/

struct stickermedalinfo stickermedalinfo;

void G_UpdateTimeStickerMedals(UINT16 map, boolean showownrecord)
{
	emblem_t *emblem = M_GetLevelEmblems(map+1);
	boolean gonnadrawtime = false;

	memset(&stickermedalinfo, 0, sizeof(stickermedalinfo));
	stickermedalinfo.timetoreach = UINT32_MAX;

	while (emblem != NULL)
	{
		UINT8 i = 0;

		switch (emblem->type)
		{
			case ET_TIME:
			{
				break;
			}
			case ET_MAP:
			{
				if (emblem->flags & ME_SPBATTACK && cv_dummyspbattack.value)
					break;
				goto bademblem;
			}
			default:
				goto bademblem;
		}

		if (cv_dummyspbattack.value && !(emblem->flags & ME_SPBATTACK))
			return;

		if (!gamedata->collected[(emblem-emblemlocations)] && gonnadrawtime)
			break;

		// Simpler than having two checks
		if (stickermedalinfo.visiblecount == MAXMEDALVISIBLECOUNT)
			stickermedalinfo.visiblecount--;

		// Shuffle along, so [0] is the "main focus"
		for (i = stickermedalinfo.visiblecount; i > 0; i--)
		{
			stickermedalinfo.emblems[i] = stickermedalinfo.emblems[i-1];
		}
		stickermedalinfo.emblems[0] = emblem;
		stickermedalinfo.visiblecount++;

		if (!gamedata->collected[(emblem-emblemlocations)] || Playing())
			gonnadrawtime = true;

bademblem:
		emblem = M_GetLevelEmblems(-1);
	}

	if (stickermedalinfo.visiblecount > 0)
	{
		if (emblem != NULL && emblem != stickermedalinfo.emblems[0])
		{
			// Regenerate the entire array if this is unlocked
			stickermedalinfo.regenemblem = emblem;
		}
		emblem = stickermedalinfo.emblems[0];

		if (gonnadrawtime)
		{
			if (emblem->tag > 0)
			{
				if (emblem->tag > mapheaderinfo[map]->ghostCount
				|| mapheaderinfo[map]->ghostBrief[emblem->tag-1] == NULL)
					snprintf(stickermedalinfo.targettext, 9, "Invalid");
				else if (mapheaderinfo[map]->ghostBrief[emblem->tag-1]->time == UINT32_MAX)
					snprintf(stickermedalinfo.targettext, 9, "DNF");
				else
					stickermedalinfo.timetoreach = mapheaderinfo[map]->ghostBrief[emblem->tag-1]->time;
			}
			else
				stickermedalinfo.timetoreach = emblem->var;
		}
	}

	if (!gonnadrawtime && showownrecord)
	{
		stickermedalinfo.timetoreach = G_GetBestTime(map);
	}

	if (stickermedalinfo.timetoreach != UINT32_MAX)
	{
		snprintf(stickermedalinfo.targettext, 9, "%i'%02i\"%02i",
			G_TicsToMinutes(stickermedalinfo.timetoreach, false),
			G_TicsToSeconds(stickermedalinfo.timetoreach),
			G_TicsToCentiseconds(stickermedalinfo.timetoreach));
	}
}

void G_TickTimeStickerMedals(void)
{
	if (stickermedalinfo.jitter)
		stickermedalinfo.jitter--;

	if (players[consoleplayer].realtime > stickermedalinfo.timetoreach)
	{
		if (stickermedalinfo.norecord == false)
		{
			S_StartSound(NULL, sfx_s3k72); //sfx_s26d); -- you STOLE fizzy lifting drinks
			stickermedalinfo.norecord = true;
			stickermedalinfo.jitter = 4;
		}
	}
	else
	{
		stickermedalinfo.norecord = false;
	}
}

//
// G_UpdateRecords
//
// Update time attack records
//
void G_UpdateRecords(void)
{
	UINT8 earnedEmblems;

	if (modeattacking & ATTACKING_TIME)
	{
		tic_t time = players[consoleplayer].realtime;
		if (players[consoleplayer].pflags & PF_NOCONTEST)
			time = UINT32_MAX;
		if (((mapheaderinfo[gamemap-1]->records.time == 0) || (time < mapheaderinfo[gamemap-1]->records.time))
		&& (time < UINT32_MAX)) // DNF
			mapheaderinfo[gamemap-1]->records.time = time;
	}
	else
	{
		mapheaderinfo[gamemap-1]->records.time = 0;
	}

	if (modeattacking & ATTACKING_LAP)
	{
		if ((mapheaderinfo[gamemap-1]->records.lap == 0) || (bestlap < mapheaderinfo[gamemap-1]->records.lap))
			mapheaderinfo[gamemap-1]->records.lap = bestlap;
	}
	else
	{
		mapheaderinfo[gamemap-1]->records.lap = 0;
	}

	// Check emblems when level data is updated
	if ((earnedEmblems = M_CheckLevelEmblems()))
	{
		if (stickermedalinfo.regenemblem != NULL
			&& gamedata->collected[(stickermedalinfo.regenemblem-emblemlocations)])
		{
			G_UpdateTimeStickerMedals(gamemap-1, false);
		}

		stickermedalinfo.jitter = 4*earnedEmblems;
		S_StartSound(NULL, sfx_ncitem);
	}

	M_UpdateUnlockablesAndExtraEmblems(true, true);
	gamedata->deferredsave = true;
}

//
// G_UpdateRecordReplays
//
// Update replay files/data, etc. for Record Attack
//
static void G_UpdateRecordReplays(void)
{
	char *gpath;
	char lastdemo[256], bestdemo[256];

	if (players[consoleplayer].pflags & PF_NOCONTEST)
	{
		players[consoleplayer].realtime = UINT32_MAX;
	}

	// Save demo!
	bestdemo[255] = '\0';
	lastdemo[255] = '\0';
	G_SetDemoTime(players[consoleplayer].realtime, bestlap);
	G_CheckDemoStatus();

	gpath = va("%s"PATHSEP"media"PATHSEP"replay"PATHSEP"%s",
			srb2home, timeattackfolder);
	M_MkdirEach(gpath, M_PathParts(gpath) - 3, 0755);

	strcat(gpath, PATHSEP);
	strcat(gpath, G_BuildMapName(gamemap));

	snprintf(lastdemo, 255, "%s-%s-last.lmp", gpath, cv_skin[0].string);

	if (modeattacking != ATTACKING_NONE && FIL_FileExists(lastdemo))
	{
		UINT8 *buf;
		size_t len;

		gpath = Z_StrDup(gpath);

		len = FIL_ReadFile(lastdemo, &buf);

		if (modeattacking & ATTACKING_TIME)
		{
			snprintf(bestdemo, 255, "%s-%s-time-best.lmp", gpath, cv_skin[0].string);
			if (!FIL_FileExists(bestdemo) || G_CmpDemoTime(bestdemo, lastdemo) & 1)
			{ // Better time, save this demo.
				if (FIL_FileExists(bestdemo))
					remove(bestdemo);
				FIL_WriteFile(bestdemo, buf, len);
				CONS_Printf("\x83%s\x80 %s '%s'\n", M_GetText("NEW RECORD TIME!"), M_GetText("Saved replay as"), bestdemo);
			}
		}

		if (modeattacking & ATTACKING_LAP)
		{
			snprintf(bestdemo, 255, "%s-%s-lap-best.lmp", gpath, cv_skin[0].string);
			if (!FIL_FileExists(bestdemo) || G_CmpDemoTime(bestdemo, lastdemo) & (1<<1))
			{ // Better lap time, save this demo.
				if (FIL_FileExists(bestdemo))
					remove(bestdemo);
				FIL_WriteFile(bestdemo, buf, len);
				CONS_Printf("\x83%s\x80 %s '%s'\n", M_GetText("NEW RECORD LAP!"), M_GetText("Saved replay as"), bestdemo);
			}
		}

		//CONS_Printf("%s '%s'\n", M_GetText("Saved replay as"), lastdemo);

		Z_Free(buf);

		Z_Free(gpath);
	}
}

// for consistency among messages: this modifies the game and removes savemoddata.
void G_SetGameModified(boolean silent, boolean major)
{
	if ((majormods && modifiedgame) || !mainwads || (refreshdirmenu & REFRESHDIR_GAMEDATA)) // new gamedata amnesty?
		return;

	modifiedgame = true;

	if (!major)
		return;

	//savemoddata = false; -- there is literally no reason to do this anymore.
	majormods = true;

	if (!silent)
		CONS_Alert(CONS_NOTICE, M_GetText("Game must be restarted to play Record Attack.\n"));

	// If in record attack recording, cancel it.
	if (modeattacking)
		M_EndModeAttackRun();
	else if (marathonmode)
		Command_ExitGame_f();
}

// for consistency among messages: this sets cheats as used.
void G_SetUsedCheats(void)
{
	if (usedCheats)
		return;

	usedCheats = true;
	CONS_Alert(CONS_NOTICE, M_GetText("Cheats activated -- game must be restarted to save progress.\n"));

	// If in record attack recording, cancel it.
	if (modeattacking)
		M_EndModeAttackRun();
	else if (marathonmode)
		Command_ExitGame_f();
}
/** Returns the map lump name for a map number.
  *
  * \param map Map number.
  * \return Map name.
  * \sa G_MapNumber
  */
const char *G_BuildMapName(INT32 map)
{
	if (map > 0 && map <= nummapheaders && mapheaderinfo[map - 1] != NULL)
	{
		return mapheaderinfo[map - 1]->lumpname;
	}
	else
	{
		return NULL;
	}
}

/** Returns the map number for map lump name.
  *
  * \param name Map name;
  * \return Map number.
  * \sa G_BuildMapName, nextmapspecial_t
  */
INT32 G_MapNumber(const char * name)
{
	if (strncasecmp("NEXTMAP_", name, 8) != 0)
	{
		INT32 map;
		UINT32 hash = quickncasehash(name, MAXMAPLUMPNAME);

		for (map = 0; map < nummapheaders; ++map)
		{
			if (hash != mapheaderinfo[map]->lumpnamehash)
				continue;

			if (strcasecmp(mapheaderinfo[map]->lumpname, name) != 0)
				continue;

			return map;
		}

		return NEXTMAP_INVALID;
	}

	name += 8;

	if (strcasecmp("EVALUATION", name) == 0)
		return NEXTMAP_EVALUATION;
	if (strcasecmp("CREDITS", name) == 0)
		return NEXTMAP_CREDITS;
	if (strcasecmp("CEREMONY", name) == 0)
		return NEXTMAP_CEREMONY;
	if (strcasecmp("TITLE", name) == 0)
		return NEXTMAP_TITLE;

	return NEXTMAP_INVALID;
}

/** Clips the console player's mouse aiming to the current view.
  * Used whenever the player view is changed manually.
  *
  * \param aiming Pointer to the vertical angle to clip.
  * \return The clipped angle.
  */
INT32 G_ClipAimingPitch(INT32 *aiming)
{
	INT32 limitangle;

	limitangle = ANGLE_90 - 1;

	if (*aiming > limitangle)
		*aiming = limitangle;
	else if (*aiming < -limitangle)
		*aiming = -limitangle;

	return (*aiming);
}

INT16 G_SoftwareClipAimingPitch(INT32 *aiming)
{
	INT32 limitangle;

	// note: the current software mode implementation doesn't have true perspective
	limitangle = ANGLE_45; // Some viewing fun, but not too far down...

	if (*aiming > limitangle)
		*aiming = limitangle;
	else if (*aiming < -limitangle)
		*aiming = -limitangle;

	return (INT16)((*aiming)>>16);
}

void G_FinalClipAimingPitch(INT32 *aiming, player_t *player, boolean skybox)
{
#ifndef HWRENDER
	(void)player;
	(void)skybox;
#endif

	// clip it in the case we are looking a hardware 90 degrees full aiming
	// (lmps, network and use F12...)
	if (rendermode == render_soft
#ifdef HWRENDER
		|| (rendermode == render_opengl
			&& (cv_glshearing.value == 1
			|| (cv_glshearing.value == 2 && R_IsViewpointThirdPerson(player, skybox))))
#endif
		)
	{
		G_SoftwareClipAimingPitch(aiming);
	}
	else
	{
		G_ClipAimingPitch(aiming);
	}
}

static INT32 G_GetValueFromControlTable(INT32 deviceID, INT32 deadzone, INT32 *controltable)
{
	INT32 i, failret = NO_BINDS_REACHABLE;

	if (deviceID <= UNASSIGNED_DEVICE)
	{
		// An invalid device can't have any binds!
		return failret;
	}

	for (i = 0; i < MAXINPUTMAPPING; i++)
	{
		INT32 key = controltable[i];
		INT32 value = 0;

		// Invalid key number.
		if (G_KeyIsAvailable(key, deviceID) == false)
		{
			continue;
		}

		value = G_GetDeviceGameKeyDownArray(deviceID)[key];

		if (value >= deadzone)
		{
			return value;
		}

		failret = 0;
	}

	// Not pressed.
	return failret;
}

INT32 G_PlayerInputAnalog(UINT8 p, INT32 gc, UINT8 menuPlayers)
{
	const INT32 deadzone = (JOYAXISRANGE * cv_deadzone[p].value) / FRACUNIT;
	const INT32 keyboard_player = G_GetPlayerForDevice(KEYBOARD_MOUSE_DEVICE);
	const boolean in_menu = (menuPlayers > 0);
	const boolean main_player = (p == 0);
	INT32 deviceID = UNASSIGNED_DEVICE;
	INT32 value = -1;
	INT32 avail_gamepad_id = 0;
	INT32 i;
	boolean bind_was_reachable = false;

	if (p >= MAXSPLITSCREENPLAYERS)
	{
#ifdef PARANOIA
		CONS_Debug(DBG_GAMELOGIC, "G_PlayerInputAnalog: Invalid player ID %d\n", p);
#endif
		return 0;
	}

	deviceID = G_GetDeviceForPlayer(p);

	if ((in_menu == true && G_KeyBindIsNecessary(gc) == true) // In menu: check for all unoverrideable menu default controls.
		|| (in_menu == false && gc == gc_start)) // In gameplay: check for the unoverrideable start button to be able to bring up the menu.
	{
		value = G_GetValueFromControlTable(KEYBOARD_MOUSE_DEVICE, JOYAXISRANGE/4, &(menucontrolreserved[gc][0]));
		if (value > 0) // Check for press instead of bound.
		{
			// This is only intended for P1.
			if (main_player == true)
			{
				return value;
			}
			else
			{
				return 0;
			}
		}
	}

	// Player 1 is always allowed to use the keyboard in 1P, even if they got disconnected.
	if (main_player == true && keyboard_player == -1 && deviceID == UNASSIGNED_DEVICE)
	{
		deviceID = KEYBOARD_MOUSE_DEVICE;
	}

	// First, try our actual binds.
	value = G_GetValueFromControlTable(deviceID, deadzone, &(gamecontrol[p][gc][0]));
	if (value > 0)
	{
		return value;
	}
	if (value != NO_BINDS_REACHABLE)
	{
		bind_was_reachable = true;
	}

	// If you're on gamepad in 1P, and you didn't have a gamepad bind for this, then try your keyboard binds.
	if (main_player == true && keyboard_player == -1 && deviceID > KEYBOARD_MOUSE_DEVICE)
	{
		value = G_GetValueFromControlTable(KEYBOARD_MOUSE_DEVICE, deadzone, &(gamecontrol[p][gc][0]));
		if (value > 0)
		{
			return value;
		}
		if (value != NO_BINDS_REACHABLE)
		{
			bind_was_reachable = true;
		}
	}

	if (in_menu == true)
	{
		if (main_player == true)
		{
			// We are P1 controlling menus. We should be able to
			// control the menu with any unused gamepads, so
			// that gamepads are able to navigate to the player
			// setup menu in the first place.
			for (avail_gamepad_id = 0; avail_gamepad_id < G_GetNumAvailableGamepads(); avail_gamepad_id++)
			{
				INT32 tryDevice = G_GetAvailableGamepadDevice(avail_gamepad_id);
				if (tryDevice <= KEYBOARD_MOUSE_DEVICE)
				{
					continue;
				}

				for (i = 0; i < menuPlayers; i++)
				{
					if (tryDevice == G_GetDeviceForPlayer(i))
					{
						// Don't do this for already taken devices.
						break;
					}
				}

				if (i == menuPlayers)
				{
					// This gamepad isn't being used, so we can
					// use it for P1 menu navigation.
					value = G_GetValueFromControlTable(tryDevice, deadzone, &(gamecontrol[p][gc][0]));
					if (value > 0)
					{
						return value;
					}
					if (value != NO_BINDS_REACHABLE)
					{
						bind_was_reachable = true;
					}
				}
			}
		}

		if (bind_was_reachable == false)
		{
			// Still nothing bound after everything. Try default gamepad controls.
			value = G_GetValueFromControlTable(deviceID, deadzone, &(gamecontroldefault[gc][0]));
			if (value > 0)
			{
				return value;
			}
		}
	}

	// Literally not bound at all, so it can't be pressed at all.
	return 0;
}

#undef KEYBOARDDEFAULTSSPLIT

boolean G_PlayerInputDown(UINT8 p, INT32 gc, UINT8 menuPlayers)
{
	return (G_PlayerInputAnalog(p, gc, menuPlayers) != 0);
}

//
// G_BuildTiccmd
// Builds a ticcmd from all of the available inputs
// or reads it from the demo buffer.
// If recording a demo, write it out
//
// set secondaryplayer true to build player 2's ticcmd in splitscreen mode
//
INT32 localaiming[MAXSPLITSCREENPLAYERS];
angle_t localangle[MAXSPLITSCREENPLAYERS];

INT32 localsteering[MAXSPLITSCREENPLAYERS];

ticcmd_t *G_CopyTiccmd(ticcmd_t* dest, const ticcmd_t* src, const size_t n)
{
	return M_Memcpy(dest, src, n*sizeof(*src));
}

ticcmd_t *G_MoveTiccmd(ticcmd_t* dest, const ticcmd_t* src, const size_t n)
{
	size_t i;
	for (i = 0; i < n; i++)
	{
		dest[i].forwardmove = src[i].forwardmove;
		dest[i].turning = (INT16)SHORT(src[i].turning);
		dest[i].angle = (INT16)SHORT(src[i].angle);
		dest[i].throwdir = (INT16)SHORT(src[i].throwdir);
		dest[i].aiming = (INT16)SHORT(src[i].aiming);
		dest[i].buttons = (UINT16)SHORT(src[i].buttons);
		dest[i].latency = src[i].latency;
		dest[i].flags = src[i].flags;
	}
	return dest;
}

void weaponPrefChange(void);
void weaponPrefChange(void)
{
	if (Playing())
		WeaponPref_Send(0);
}

void weaponPrefChange2(void);
void weaponPrefChange2(void)
{
	if (Playing())
		WeaponPref_Send(1);
}

void weaponPrefChange3(void);
void weaponPrefChange3(void)
{
	if (Playing())
		WeaponPref_Send(2);
}

void weaponPrefChange4(void);
void weaponPrefChange4(void)
{
	if (Playing())
		WeaponPref_Send(3);
}

void rumble_off_handle(void);
void rumble_off_handle(void)
{
	if (cv_rumble[0].value == 0)
		G_ResetPlayerDeviceRumble(0);
}

void rumble_off_handle2(void);
void rumble_off_handle2(void)
{
	if (cv_rumble[1].value == 0)
		G_ResetPlayerDeviceRumble(1);
}

void rumble_off_handle3(void);
void rumble_off_handle3(void)
{
	if (cv_rumble[2].value == 0)
		G_ResetPlayerDeviceRumble(2);
}

void rumble_off_handle4(void);
void rumble_off_handle4(void)
{
	if (cv_rumble[3].value == 0)
		G_ResetPlayerDeviceRumble(3);
}

//
// G_DoLoadLevelEx
//
void G_DoLoadLevelEx(boolean resetplayer, gamestate_t newstate)
{
	boolean doAutomate = false;
	INT32 i;

	// Make sure objectplace is OFF when you first start the level!
	OP_ResetObjectplace();

	levelstarttic = gametic; // for time calculation

	if (wipegamestate == newstate)
		wipegamestate = -1; // force a wipe

	if (cv_currprofile.value == -1 && !demo.playback)
	{
		PR_ApplyProfilePretend(cv_ttlprofilen.value, 0);
		for (i = 1; i < cv_splitplayers.value; i++)
		{
			PR_ApplyProfile(cv_lastprofile[i].value, i);
		}
	}
	if (gamestate == GS_INTERMISSION)
		Y_EndIntermission();
	if (gamestate == GS_VOTING)
		Y_EndVote();

	// cleanup
	// Is this actually necessary? Doesn't F_StartTitleScreen already do a significantly more comprehensive check?
	if (newstate == GS_TITLESCREEN)
	{
		if (gamemap < 1 || gamemap > nummapheaders)
		{
			G_SetGamestate(GS_TITLESCREEN);
			titlemapinaction = false;

			Z_Free(titlemap);
			titlemap = NULL; // let's not infinite recursion ok

			Command_ExitGame_f();
			return;
		}
	}

	// Doing this matches HOSTMOD behavior.
	// Is that desired? IDK
	doAutomate = (gamestate != GS_LEVEL && newstate == GS_LEVEL);

	G_SetGamestate(newstate);
	if (wipegamestate == GS_MENU)
		M_ClearMenus(true);
	I_UpdateMouseGrab();

	K_ResetCeremony();

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (resetplayer || (playeringame[i] && players[i].playerstate == PST_DEAD))
			players[i].playerstate = PST_REBORN;
	}

	// Setup the level.
	if (!P_LoadLevel(false, false)) // this never returns false?
	{
		// fail so reset game stuff
		Command_ExitGame_f();
		return;
	}

	gameaction = ga_nothing;
#ifdef PARANOIA
	Z_CheckHeap(-2);
#endif

	if (roundqueue.writetextmap == true)
	{
		if (roundqueue.size > 0)
		{
			G_GetNextMap();

			// roundqueue is wiped after the last round, but
			// preserve this to track state into the Podium!
			roundqueue.writetextmap = true;

			G_NextLevel();
			return;
		}
		else
		{
			// Podium: writetextmap is finished. Yay!
			HU_DoTitlecardCEcho(NULL, va("Congratulations,\\%s!\\Check the console!", cv_playername[0].string), true);

			livestudioaudience_timer = 0;
			LiveStudioAudience();

			CONS_Printf("\n\n\x83""writetextmap: Find your TEXTMAPs in %s\n", srb2home);

			roundqueue.writetextmap = false;
		}
	}

	for (i = 0; i <= r_splitscreen; i++)
	{
		if (camera[i].chase)
			P_ResetCamera(&players[displayplayers[i]], &camera[i]);
	}

	// clear cmd building stuff
	memset(gamekeydown, 0, sizeof (gamekeydown));
	G_ResetAllDeviceResponding();

	// clear hud messages remains (usually from game startup)
	CON_ClearHUD();

	SV_UpdateStats();

	server_lagless = cv_lagless.value;

	if (doAutomate == true)
	{
		if (roundqueue.size > 0 && roundqueue.position == 1)
		{
			Automate_Run(AEV_QUEUESTART);
		}

		Automate_Run(AEV_ROUNDSTART);
	}

	G_ResetAllDeviceRumbles();
}

void G_DoLoadLevel(boolean resetplayer)
{
	G_DoLoadLevelEx(resetplayer, GS_LEVEL);
}

//
// Start the title card.
//
void G_StartTitleCard(void)
{
	// The title card has been disabled for this map.
	// Oh well.
	if (demo.rewinding || !G_IsTitleCardAvailable())
	{
		WipeStageTitle = false;
		return;
	}

	// clear the hud
	CON_ClearHUD();

	// prepare status bar
	ST_startTitleCard();

	// play the sound
	if (gamestate != GS_CEREMONY)
	{
		sfxenum_t kstart = sfx_kstart;
		if (K_CheckBossIntro() == true)
			kstart = sfx_ssa021;
		else if (encoremode == true)
			kstart = sfx_ruby2;
		S_StartSound(NULL, kstart);
	}

	// start the title card
	WipeStageTitle = (gamestate == GS_LEVEL);
}

//
// Run the title card before fading in to the level.
//
void G_PreLevelTitleCard(void)
{
#ifndef NOWIPE
	tic_t strtime = I_GetTime();
	tic_t endtime = strtime + (PRELEVELTIME*NEWTICRATERATIO);
	tic_t nowtime = strtime;
	tic_t lasttime = strtime;
	while (nowtime < endtime)
	{
		// draw loop
		ST_runTitleCard();
		ST_preLevelTitleCardDrawer();
		I_FinishUpdate(); 	// page flip or blit buffer
		NetKeepAlive();		// Prevent timeouts

#ifdef HWRENDER
		if (moviemode && rendermode == render_opengl)
			M_LegacySaveFrame();
#endif

		while (!((nowtime = I_GetTime()) - lasttime))
		{
			I_Sleep(cv_sleep.value);
			I_UpdateTime(cv_timescale.value);
		}
		lasttime = nowtime;
	}
#endif
}

//
// Returns true if the current level has a title card.
//
boolean G_IsTitleCardAvailable(void)
{
	// Overwrites all other title card exceptions.
	if (K_CheckBossIntro() == true)
		return true;

	// The current level has no name.
	if (!mapheaderinfo[gamemap-1]->lvlttl[0])
		return false;

	// Instant white fade.
	if (gametyperules & GTR_SPECIALSTART)
		return false;

	// ALso.
	if (K_PodiumSequence() == true)
		return false;

	// The title card is available.
	return true;
}

INT32 pausedelay = 0;
boolean pausebreakkey = false;
static INT32 camtoggledelay[MAXSPLITSCREENPLAYERS];

//
// G_Responder
// Get info needed to make ticcmd_ts for the players.
//
boolean G_Responder(event_t *ev)
{
	//INT32 i;

	// any other key pops up menu if in demos
	if (gameaction == ga_nothing && !demo.quitafterplaying &&
		((demo.playback && !modeattacking && !demo.title && !multiplayer) || gamestate == GS_TITLESCREEN))
	{
		if (ev->type == ev_keydown
		|| (ev->type == ev_gamepad_axis && ev->data1 >= JOYANALOGS
			&& ((abs(ev->data2) > JOYAXISRANGE/2
			|| abs(ev->data3) > JOYAXISRANGE/2))
		))
		{
			M_StartControlPanel();
			return true;
		}
		return false;
	}
	else if (demo.playback && demo.title)
	{
		// Title demo uses intro responder
		if (F_IntroResponder(ev))
		{
			// stop the title demo
			G_CheckDemoStatus();
			return true;
		}
		return false;
	}

	if (gamestate == GS_LEVEL)
	{
		if (HU_Responder(ev))
		{
			hu_keystrokes = true;
			return true; // chat ate the event
		}
		if (AM_Responder(ev))
			return true; // automap ate it
		// map the event (key/mouse/joy) to a gamecontrol
	}
	// Intro
	else if (gamestate == GS_INTRO)
	{
		if (F_IntroResponder(ev))
		{
			D_StartTitle();
			return true;
		}
	}
	else if (gamestate == GS_CUTSCENE)
	{
		if (HU_Responder(ev))
		{
			hu_keystrokes = true;
			return true; // chat ate the event
		}

		if (F_CutsceneResponder(ev))
		{
			D_StartTitle();
			return true;
		}
	}
	else if (gamestate == GS_CREDITS)
	{
		if (HU_Responder(ev))
		{
			hu_keystrokes = true;
			return true; // chat ate the event
		}

		if (F_CreditResponder(ev))
		{
			// Skip credits for everyone
			if (! netgame)
				F_StartGameEvaluation();
			else if (server || IsPlayerAdmin(consoleplayer))
				SendNetXCmd(XD_EXITLEVEL, NULL, 0);
			return true;
		}
	}
	else if (gamestate == GS_CEREMONY)
	{
		if (HU_Responder(ev))
		{
			hu_keystrokes = true;
			return true; // chat ate the event
		}

		if (K_CeremonyResponder(ev))
		{
			if (grandprixinfo.gp == true
				&& grandprixinfo.cup != NULL
				&& grandprixinfo.cup->playcredits == true)
			{
				nextmap = NEXTMAP_CREDITS;
			}
			else
			{
				nextmap = NEXTMAP_TITLE;
			}

			G_EndGame();
			return true;
		}
	}
	else if (gamestate == GS_CONTINUING)
	{
		return true;
	}
	else if (gamestate == GS_INTERMISSION || gamestate == GS_VOTING || gamestate == GS_EVALUATION)
	{
		if (HU_Responder(ev))
		{
			hu_keystrokes = true;
			return true; // chat ate the event
		}
	}

	if (gamestate == GS_LEVEL && ev->type == ev_keydown && multiplayer && demo.playback && !demo.freecam)
	{
		// Allow pausing
		if (
			//ev->data1 == gamecontrol[0][gc_pause][0]
			//|| ev->data1 == gamecontrol[0][gc_pause][1]
			ev->data1 == KEY_PAUSE
		)
		{
			paused = !paused;

			if (demo.rewinding)
			{
				G_ConfirmRewind(leveltime);
				paused = true;
				S_PauseAudio();
			}
			else if (paused)
				S_PauseAudio();
			else
				S_ResumeAudio();

			return true;
		}

		// open menu but only w/ esc
		if (ev->data1 == 32)
		{
			M_StartControlPanel();

			return true;
		}
	}

	switch (ev->type)
	{
		case ev_keydown:
			if (//ev->data1 == gamecontrol[0][gc_pause][0]
				//|| ev->data1 == gamecontrol[0][gc_pause][1]
				ev->data1 == KEY_PAUSE)
			{
				if (modeattacking && !demo.playback && (gamestate == GS_LEVEL))
				{
					pausebreakkey = (ev->data1 == KEY_PAUSE);
					if (menuactive || pausedelay < 0 || leveltime < 2)
						return true;

					if (pausedelay < 1+(NEWTICRATE/2))
						pausedelay = 1+(NEWTICRATE/2);
					else if (++pausedelay > 1+(NEWTICRATE/2)+(NEWTICRATE/3))
					{
						G_SetModeAttackRetryFlag();
						return true;
					}
					pausedelay++; // counteract subsequent subtraction this frame
				}
				else
				{
					INT32 oldpausedelay = pausedelay;
					pausedelay = (NEWTICRATE/7);
					if (!oldpausedelay)
					{
						// command will handle all the checks for us
						COM_ImmedExecute("pause");
						return true;
					}
				}
			}

			/*
			for (i = 0; i < MAXSPLITSCREENPLAYERS; i++)
			{
				if (ev->data1 == gamecontrol[i][gc_camtoggle][0]
					|| ev->data1 == gamecontrol[i][gc_camtoggle][1])
				{
					if (!camtoggledelay[i])
					{
						camtoggledelay[i] = NEWTICRATE / 7;
						CV_SetValue(&cv_chasecam[i], cv_chasecam[i].value ? 0 : 1);
					}
				}
			}
			*/

			return true;

		case ev_keyup:
			return false; // always let key up events filter down

		case ev_mouse:
			return true; // eat events

		case ev_gamepad_axis:
			return true; // eat events

		default:
			break;
	}

	return false;
}

//
// G_CouldView
// Return whether a player could be viewed by any means.
//
boolean G_CouldView(INT32 playernum)
{
	player_t *player;

	if (playernum < 0 || playernum > MAXPLAYERS-1)
		return false;

	if (!playeringame[playernum])
		return false;

	player = &players[playernum];

	if (player->spectator)
		return false;

	// SRB2Kart: Only go through players who are actually playing
	if (player->exiting)
		return false;
	if (( player->pflags & PF_NOCONTEST ))
		return false;

	// SRB2Kart: we have no team-based modes, YET...
	if (G_GametypeHasTeams())
	{
		if (players[consoleplayer].ctfteam && player->ctfteam != players[consoleplayer].ctfteam)
			return false;
	}

	return true;
}

//
// G_CanView
// Return whether a player can be viewed on a particular view (splitscreen).
//
boolean G_CanView(INT32 playernum, UINT8 viewnum, boolean onlyactive)
{
	UINT8 splits;
	UINT8 viewd;
	INT32 *displayplayerp;

	if (!(onlyactive ? G_CouldView(playernum) : (playeringame[playernum] && !players[playernum].spectator)))
		return false;

	splits = r_splitscreen+1;
	if (viewnum > splits)
		viewnum = splits;

	for (viewd = 1; viewd < viewnum; ++viewd)
	{
		displayplayerp = (&displayplayers[viewd-1]);
		if ((*displayplayerp) == playernum)
			return false;
	}
	for (viewd = viewnum + 1; viewd <= splits; ++viewd)
	{
		displayplayerp = (&displayplayers[viewd-1]);
		if ((*displayplayerp) == playernum)
			return false;
	}

	return true;
}

//
// G_FindView
// Return the next player that can be viewed on a view, wraps forward.
// An out of range startview is corrected.
//
INT32 G_FindView(INT32 startview, UINT8 viewnum, boolean onlyactive, boolean reverse)
{
	INT32 i, dir = reverse ? -1 : 1;
	startview = min(max(startview, -1), MAXPLAYERS);
	for (i = startview; i < MAXPLAYERS && i >= 0; i += dir)
	{
		if (G_CanView(i, viewnum, onlyactive))
			return i;
	}
	for (i = (reverse ? MAXPLAYERS-1 : 0); i != startview; i += dir)
	{
		if (G_CanView(i, viewnum, onlyactive))
			return i;
	}
	return -1;
}

INT32 G_CountPlayersPotentiallyViewable(boolean active)
{
	INT32 total = 0;
	INT32 i;
	for (i = 0; i < MAXPLAYERS; ++i)
	{
		if (active ? G_CouldView(i) : (playeringame[i] && !players[i].spectator))
			total++;
	}
	return total;
}

//
// G_ResetView
// Correct a viewpoint to playernum or the next available, wraps forward.
// Also promotes splitscreen up to available viewable players.
// An out of range playernum is corrected.
//
void G_ResetView(UINT8 viewnum, INT32 playernum, boolean onlyactive)
{
	UINT8 splits;
	UINT8 viewd;

	INT32    *displayplayerp;
	camera_t *camerap;

	INT32 olddisplayplayer;
	INT32 playersviewable;

	splits = r_splitscreen+1;

	/* Promote splits */
	if (viewnum > splits)
	{
		playersviewable = G_CountPlayersPotentiallyViewable(onlyactive);
		if (playersviewable < splits)/* do not demote */
			return;

		if (viewnum > playersviewable)
			viewnum = playersviewable;
		r_splitscreen = viewnum-1;

		/* Prepare extra views for G_FindView to pass. */
		for (viewd = splits+1; viewd < viewnum; ++viewd)
		{
			displayplayerp = (&displayplayers[viewd-1]);
			(*displayplayerp) = INT32_MAX;
		}

		R_ExecuteSetViewSize();
	}

	displayplayerp = (&displayplayers[viewnum-1]);
	olddisplayplayer = (*displayplayerp);

	/* Check if anyone is available to view. */
	if (( playernum = G_FindView(playernum, viewnum, onlyactive, playernum < olddisplayplayer) ) == -1)
	{
		if (G_PartySize(consoleplayer) < viewnum)
		{
			return;
		}

		/* Fall back on true self */
		playernum = G_PartyMember(consoleplayer, viewnum - 1);
	}

	// Call ViewpointSwitch hooks here.
	// The viewpoint was forcibly changed.
	LUA_HookViewpointSwitch(&players[g_localplayers[viewnum - 1]], &players[playernum], true);

	/* Focus our target view first so that we don't take its player. */
	(*displayplayerp) = playernum;
	if ((*displayplayerp) != olddisplayplayer)
	{
		camerap = &camera[viewnum-1];
		P_ResetCamera(&players[(*displayplayerp)], camerap);

		R_ResetViewInterpolation(viewnum);
	}

	if (viewnum > splits)
	{
		for (viewd = splits+1; viewd < viewnum; ++viewd)
		{
			displayplayerp = (&displayplayers[viewd-1]);
			camerap = &camera[viewd];

			(*displayplayerp) = G_FindView(0, viewd, onlyactive, false);

			P_ResetCamera(&players[(*displayplayerp)], camerap);
		}
	}

	if (viewnum == 1 && demo.playback)
		consoleplayer = displayplayers[0];

	// change statusbar also if playing back demo
	if (demo.quitafterplaying)
		ST_changeDemoView();
}

//
// G_AdjustView
// Increment a viewpoint by offset from the current player. A negative value
// decrements.
//
void G_AdjustView(UINT8 viewnum, INT32 offset, boolean onlyactive)
{
	INT32 *displayplayerp, oldview;
	displayplayerp = &displayplayers[viewnum-1];
	oldview = (*displayplayerp);
	G_ResetView(viewnum, ( (*displayplayerp) + offset ), onlyactive);

	// If no other view could be found, go back to what we had.
	if ((*displayplayerp) == -1)
		(*displayplayerp) = oldview;
}

//
// G_ResetViews
// Ensures all viewpoints are valid
// Also demotes splitscreen down to one player.
//
void G_ResetViews(void)
{
	UINT8 splits;
	UINT8 viewd;

	INT32 playersviewable;

	splits = r_splitscreen+1;

	playersviewable = G_CountPlayersPotentiallyViewable(false);
	/* Demote splits */
	if (playersviewable < splits)
	{
		splits = max(playersviewable, G_PartySize(consoleplayer)); // don't delete local players
		r_splitscreen = splits - 1;
		R_ExecuteSetViewSize();
	}

	/*
	Consider installing a method to focus the last
	view elsewhere if all players spectate?
	*/
	for (viewd = 1; viewd <= splits; ++viewd)
	{
		G_AdjustView(viewd, 0, false);
	}
}

//
// G_Ticker
// Make ticcmd_ts for the players.
//
void G_Ticker(boolean run)
{
	UINT32 i;
	INT32 buf;
	ticcmd_t *cmd;

	// see also SCR_DisplayMarathonInfo
	if ((marathonmode & (MA_INIT|MA_INGAME)) == MA_INGAME && gamestate == GS_LEVEL)
		marathontime++;

	P_MapStart();

	if (gamestate == GS_LEVEL)
	{
		// Or, alternatively, retry.
		if (G_GetRetryFlag())
		{
			G_ClearRetryFlag();

			for (i = 0; i < MAXPLAYERS; i++)
			{
				if (playeringame[i])
				{
					if (players[i].bot == true && grandprixinfo.gp == true && grandprixinfo.masterbots == false)
					{
						players[i].botvars.difficulty--;

						if (players[i].botvars.difficulty < 1)
						{
							players[i].botvars.difficulty = 1;
						}
					}
					else
					{
						K_PlayerLoseLife(&players[i]);
					}
				}
			}

			D_MapChange(gamemap, gametype, (cv_kartencore.value == 1), false, 1, false, false);
		}
	}

	// do player reborns if needed
	if (G_GamestateUsesLevel() == true)
	{
		boolean changed = false;

		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (playeringame[i] && players[i].playerstate == PST_REBORN)
			{
				G_DoReborn(i);
				changed = true;
			}
		}

		if (changed == true)
		{
			K_UpdateAllPlayerPositions();
		}
	}

	P_MapEnd();

	// do things to change the game state
	while (gameaction != ga_nothing)
		switch (gameaction)
		{
			case ga_completed: G_DoCompleted(); break;
			case ga_startcont: G_DoStartContinue(); break;
			case ga_continued: G_DoContinued(); break;
			case ga_worlddone: G_DoWorldDone(); break;
			case ga_startvote: G_DoStartVote(); break;
			case ga_nothing: break;
			default: I_Error("gameaction = %d\n", gameaction);
		}

	buf = gametic % BACKUPTICS;

	if (!demo.playback)
	{
		for (i = 0; i < MAXPLAYERS; i++)
		{
			cmd = &players[i].cmd;

			if (playeringame[i])
			{
				G_CopyTiccmd(cmd, &netcmds[buf][i], 1);

				// Use the leveltime sent in the player's ticcmd to determine control lag
				if (K_PlayerUsesBotMovement(&players[i]))
				{
					// Never has lag
					cmd->latency = 0;
				}
				else
				{
					//@TODO add a cvar to allow setting this max
					cmd->latency = min(((leveltime & TICCMD_LATENCYMASK) - cmd->latency) & TICCMD_LATENCYMASK, MAXPREDICTTICS-1);
				}
			}
		}
	}

	// do main actions
	switch (gamestate)
	{
		case GS_LEVEL:
			if (demo.title)
				F_TitleDemoTicker();
			P_Ticker(run); // tic the game
			ST_Ticker(run);
			F_TextPromptTicker();
			AM_Ticker();
			HU_Ticker();
			break;

		case GS_INTERMISSION:
			if (run)
				Y_Ticker();
			HU_Ticker();
			break;

		case GS_VOTING:
			if (run)
				Y_VoteTicker();
			HU_Ticker();
			break;

		case GS_MENU:
			break;

		case GS_INTRO:
			if (run)
				F_IntroTicker();
			break;

		case GS_CUTSCENE:
			if (run)
				F_CutsceneTicker();
			HU_Ticker();
			break;

		case GS_EVALUATION:
			if (run)
				F_GameEvaluationTicker();
			HU_Ticker();
			break;

		case GS_CONTINUING:
			break;

		case GS_CREDITS:
			if (run)
				F_CreditTicker();
			HU_Ticker();
			break;

		case GS_TITLESCREEN:
			if (titlemapinaction)
				P_Ticker(run);

			F_TitleScreenTicker(run);
			break;

		case GS_CEREMONY:
			P_Ticker(run);
			K_CeremonyTicker(run);
			HU_Ticker();
			break;

		case GS_WAITINGPLAYERS:
			if (netgame)
				F_WaitingPlayersTicker();
			HU_Ticker();
			break;

		case GS_DEDICATEDSERVER:
		case GS_NULL:
			break; // do nothing
	}

	if (run)
	{
		if (G_GametypeHasSpectators()
			&& (gamestate == GS_LEVEL || gamestate == GS_INTERMISSION || gamestate == GS_VOTING // definitely good
			|| gamestate == GS_WAITINGPLAYERS)) // definitely a problem if we don't do it at all in this gamestate, but might need more protection?
		{
			K_CheckSpectateStatus(true);
		}

		if (pausedelay && pausedelay != INT32_MIN)
		{
			if (pausedelay > 0)
				pausedelay--;
			else
				pausedelay++;
		}

		for (i = 0; i < MAXSPLITSCREENPLAYERS; i++)
		{
			if (camtoggledelay[i])
				camtoggledelay[i]--;
		}

		if (gametic % NAMECHANGERATE == 0)
		{
			memset(player_name_changes, 0, sizeof player_name_changes);
		}

		if (Playing() == true)
		{
			if (musiccountdown > 1)
			{
				musiccountdown--;
				if (musiccountdown == 1)
				{
					Music_Play("intermission");
				}
				else if (musiccountdown == (MUSICCOUNTDOWNMAX - (3*TICRATE)/2))
				{
					P_EndingMusic();
				}
			}

			P_InvincGrowMusic();

			K_TickMidVote();
		}
	}
}

//
// PLAYER STRUCTURE FUNCTIONS
// also see P_SpawnPlayer in P_Things
//

//
// G_PlayerFinishLevel
// Called when a player completes a level.
//
static inline void G_PlayerFinishLevel(INT32 player)
{
	player_t *p;

	p = &players[player];

	p->mo->renderflags &= ~(RF_TRANSMASK|RF_BRIGHTMASK); // cancel invisibility
	P_FlashPal(p, 0, 0); // Resets

	p->starpostnum = 0;
	memset(&p->respawn, 0, sizeof (p->respawn));

	p->spectatorReentry = 0; // Clean up any pending re-entry forbiddings
}

//
// G_PlayerReborn
// Called after a player dies. Almost everything is cleared and initialized.
//
void G_PlayerReborn(INT32 player, boolean betweenmaps)
{
	player_t *p;
	INT32 score, roundscore;
	INT32 lives;

	UINT8 kartspeed;
	UINT8 kartweight;

	boolean followerready;
	INT32 followerskin;
	UINT16 followercolor;

	mobj_t *ringShooter, *hoverhyudoro;
	mobj_t *skyboxviewpoint, *skyboxcenterpoint;

	INT32 charflags;
	UINT32 followitem;

	INT32 pflags;

	UINT8 ctfteam;

	INT32 starpostnum;
	INT32 exiting;
	INT32 khudfinish;
	INT32 khudcardanimation;
	INT16 totalring;
	UINT8 laps;
	UINT8 latestlap;
	UINT32 lapPoints;
	UINT16 skincolor;
	INT32 skin;
	UINT8 availabilities[MAXAVAILABILITY];
	UINT8 fakeskin;
	UINT8 lastfakeskin;

	tic_t jointime;

	tic_t spectatorReentry;

	UINT32 griefValue;
	UINT8 griefStrikes;

	UINT8 splitscreenindex;
	boolean spectator;
	boolean bot;
	UINT8 botdifficulty;

	INT16 rings;
	INT16 spheres;
	INT16 steering;
	angle_t playerangleturn;

	UINT8 botdiffincrease;
	boolean botrival;

	SINT8 xtralife;

	uint8_t public_key[PUBKEYLENGTH];

	// SRB2kart
	itemroulette_t itemRoulette;
	respawnvars_t respawn;
	INT32 itemtype;
	INT32 itemamount;
	INT32 growshrinktimer;
	UINT16 nocontrol;
	INT32 khudfault;
	INT32 kickstartaccel;
	boolean enteredGame;

	roundconditions_t roundconditions;
	boolean saveroundconditions;

	score = players[player].score;
	lives = players[player].lives;
	ctfteam = players[player].ctfteam;

	jointime = players[player].jointime;

	splitscreenindex = players[player].splitscreenindex;
	spectator = players[player].spectator;

	steering = players[player].steering;
	playerangleturn = players[player].angleturn;

	skincolor = players[player].skincolor;
	skin = players[player].skin;

	if (betweenmaps)
	{
		fakeskin = MAXSKINS;
		kartspeed = skins[players[player].skin].kartspeed;
		kartweight = skins[players[player].skin].kartweight;
		charflags = skins[players[player].skin].flags;
	}
	else
	{
		UINT32 skinflags = (demo.playback)
			? demo.skinlist[demo.currentskinid[player]].flags
			: skins[players[player].skin].flags;

		fakeskin = players[player].fakeskin;
		kartspeed = players[player].kartspeed;
		kartweight = players[player].kartweight;

		charflags = (skinflags & SF_IRONMAN) ? skinflags : players[player].charflags;
	}
	lastfakeskin = players[player].lastfakeskin;

	followerready = players[player].followerready;
	followercolor = players[player].followercolor;
	followerskin = players[player].followerskin;

	memcpy(availabilities, players[player].availabilities, sizeof(availabilities));

	followitem = players[player].followitem;

	bot = players[player].bot;
	botdifficulty = players[player].botvars.difficulty;

	botdiffincrease = players[player].botvars.diffincrease;
	botrival = players[player].botvars.rival;

	totalring = players[player].totalring;
	xtralife = players[player].xtralife;

	pflags = (players[player].pflags & (PF_WANTSTOJOIN|PF_KICKSTARTACCEL|PF_SHRINKME|PF_SHRINKACTIVE|PF_AUTOROULETTE));

	// SRB2kart
	memcpy(&itemRoulette, &players[player].itemRoulette, sizeof (itemRoulette));
	memcpy(&respawn, &players[player].respawn, sizeof (respawn));
	memcpy(&public_key, &players[player].public_key, sizeof(public_key));

	if (betweenmaps || leveltime < introtime)
	{
		K_StopRoulette(&itemRoulette);

		itemtype = 0;
		itemamount = 0;
		growshrinktimer = 0;
		if (gametyperules & GTR_SPHERES)
		{
			rings = 0;
		}
		else if (modeattacking & ATTACKING_SPB)
		{
			rings = 20;
		}
		else if (gametyperules & GTR_CATCHER)
		{
			rings = 20;
		}
		else
		{
			rings = 5;
		}
		spheres = 0;
		kickstartaccel = 0;
		khudfault = 0;
		nocontrol = 0;
		laps = 0;
		latestlap = 0;
		lapPoints = 0;
		roundscore = 0;
		exiting = 0;
		khudfinish = 0;
		khudcardanimation = 0;
		starpostnum = 0;

		saveroundconditions = false;
	}
	else
	{
		if (players[player].pflags & PF_ITEMOUT)
		{
			itemtype = 0;
			itemamount = 0;
		}
		else
		{
			itemtype = players[player].itemtype;
			itemamount = players[player].itemamount;
		}

		// Keep Shrink status, remove Grow status
		if (players[player].growshrinktimer < 0)
			growshrinktimer = players[player].growshrinktimer;
		else
			growshrinktimer = 0;

		rings = players[player].rings;
		spheres = players[player].spheres;
		kickstartaccel = players[player].kickstartaccel;

		khudfault = players[player].karthud[khud_fault];
		nocontrol = players[player].nocontrol;

		laps = players[player].laps;
		latestlap = players[player].latestlap;
		lapPoints = players[player].lapPoints;

		roundscore = players[player].roundscore;

		exiting = players[player].exiting;
		if (exiting > 0)
		{
			khudfinish = players[player].karthud[khud_finish];
			khudcardanimation = players[player].karthud[khud_cardanimation];
		}
		else
		{
			khudfinish = 0;
			khudcardanimation = 0;
		}

		starpostnum = players[player].starpostnum;

		pflags |= (players[player].pflags & (PF_STASIS|PF_ELIMINATED|PF_NOCONTEST|PF_FAULT|PF_LOSTLIFE));

		memcpy(&roundconditions, &players[player].roundconditions, sizeof (roundconditions));
		saveroundconditions = true;
	}

	spectatorReentry = (betweenmaps ? 0 : players[player].spectatorReentry);

	griefValue = players[player].griefValue;
	griefStrikes = players[player].griefStrikes;

	if (!betweenmaps)
	{
		K_RemoveFollower(&players[player]);

#define PlayerPointerRemove(field) \
		if (field) \
		{ \
			P_RemoveMobj(field); \
			P_SetTarget(&field, NULL); \
		}

		// These are mostly subservient to the player, and may not clean themselves up.
		PlayerPointerRemove(players[player].followmobj);
		PlayerPointerRemove(players[player].stumbleIndicator);
		PlayerPointerRemove(players[player].sliptideZipIndicator);

#undef PlayerPointerRemove

		// These will erase themselves.
		P_SetTarget(&players[player].whip, NULL);
		P_SetTarget(&players[player].hand, NULL);

		// TODO: Any better handling in store?
		P_SetTarget(&players[player].awayview.mobj, NULL);
		P_SetTarget(&players[player].flickyAttacker, NULL);
		P_SetTarget(&players[player].powerup.flickyController, NULL);

		// The following pointers are safe to set directly, because the end goal should be refcount consistency before and after remanifestation.
		ringShooter = players[player].ringShooter;
		hoverhyudoro = players[player].hoverhyudoro;
		skyboxviewpoint = players[player].skybox.viewpoint;
		skyboxcenterpoint = players[player].skybox.centerpoint;
	}
	else
	{
		ringShooter = hoverhyudoro = NULL;
		skyboxviewpoint = skyboxcenterpoint = NULL;
	}

	enteredGame = players[player].enteredGame;

	p = &players[player];
	memset(p, 0, sizeof (*p));

	p->score = score;
	p->roundscore = roundscore;
	p->lives = lives;
	p->pflags = pflags;
	p->ctfteam = ctfteam;
	p->jointime = jointime;
	p->splitscreenindex = splitscreenindex;
	p->spectator = spectator;
	p->steering = steering;
	p->angleturn = playerangleturn;

	// save player config truth reborn
	p->skincolor = skincolor;
	p->skin = skin;

	p->fakeskin = fakeskin;
	p->kartspeed = kartspeed;
	p->kartweight = kartweight;
	p->charflags = charflags;
	p->lastfakeskin = lastfakeskin;

	memcpy(players[player].availabilities, availabilities, sizeof(availabilities));
	p->followitem = followitem;

	p->starpostnum = starpostnum;
	p->exiting = exiting;
	p->karthud[khud_finish] = khudfinish;
	p->karthud[khud_cardanimation] = khudcardanimation;

	p->laps = laps;
	p->latestlap = latestlap;
	p->lapPoints = lapPoints;
	p->totalring = totalring;

	p->bot = bot;
	p->botvars.difficulty = botdifficulty;
	p->rings = rings;
	p->spheres = spheres;
	p->botvars.diffincrease = botdiffincrease;
	p->botvars.rival = botrival;
	p->xtralife = xtralife;

	// SRB2kart
	p->itemtype = itemtype;
	p->itemamount = itemamount;
	p->growshrinktimer = growshrinktimer;
	p->karmadelay = 0;
	p->eggmanblame = -1;
	p->lastdraft = -1;
	p->karthud[khud_fault] = khudfault;
	p->nocontrol = nocontrol;
	p->kickstartaccel = kickstartaccel;

	p->ringvolume = 255;

	p->botvars.rubberband = FRACUNIT;
	p->botvars.controller = UINT16_MAX;

	p->spectatorReentry = spectatorReentry;
	p->griefValue = griefValue;
	p->griefStrikes = griefStrikes;

	memcpy(&p->itemRoulette, &itemRoulette, sizeof (p->itemRoulette));
	memcpy(&p->respawn, &respawn, sizeof (p->respawn));

	memcpy(&p->public_key, &public_key, sizeof(p->public_key));

	if (saveroundconditions)
		memcpy(&p->roundconditions, &roundconditions, sizeof (p->roundconditions));

	// See above comment about refcount consistency.
	p->ringShooter = ringShooter;
	p->hoverhyudoro = hoverhyudoro;
	p->skybox.viewpoint = skyboxviewpoint;
	p->skybox.centerpoint = skyboxcenterpoint;

	p->followerready = followerready;
	p->followerskin = followerskin;
	p->followercolor = followercolor;
	//p->follower = NULL;	// respawn a new one with you, it looks better.
	// ^ Not necessary anyway since it will be respawned regardless considering it doesn't exist anymore.

	p->playerstate = PST_LIVE;
	p->panim = PA_STILL; // standing animation

	// Check to make sure their color didn't change somehow...
	if (G_GametypeHasTeams())
	{
		UINT8 i;

		if (p->ctfteam == 1 && p->skincolor != skincolor_redteam)
		{
			for (i = 0; i <= splitscreen; i++)
			{
				if (p == &players[g_localplayers[i]])
				{
					CV_SetValue(&cv_playercolor[i], skincolor_redteam);
					break;
				}
			}
		}
		else if (p->ctfteam == 2 && p->skincolor != skincolor_blueteam)
		{
			for (i = 0; i <= splitscreen; i++)
			{
				if (p == &players[g_localplayers[i]])
				{
					CV_SetValue(&cv_playercolor[i], skincolor_blueteam);
					break;
				}
			}
		}
	}

	if (p->spectator == false)
	{
		if (betweenmaps || enteredGame == true)
		{
			ACS_RunPlayerEnterScript(p);
		}
		else
		{
			ACS_RunPlayerRespawnScript(p);
		}
	}

	if (betweenmaps)
		return;

	if (leveltime < starttime)
		return;

	if (exiting)
		return;
}

//
// G_CheckSpot
// Returns false if the player cannot be respawned
// at the given mapthing_t spot
// because something is occupying it
//
static boolean G_CheckSpot(INT32 playernum, mapthing_t *mthing)
{
	fixed_t x;
	fixed_t y;
	INT32 i;

	// maybe there is no player start
	if (!mthing)
		return false;

	if (!players[playernum].mo)
	{
		// first spawn of level
		for (i = 0; i < playernum; i++)
			if (playeringame[i] && players[i].mo
				&& players[i].mo->x == mthing->x << FRACBITS
				&& players[i].mo->y == mthing->y << FRACBITS)
			{
				return false;
			}
		return true;
	}

	x = mthing->x << FRACBITS;
	y = mthing->y << FRACBITS;

	if (!K_CheckPlayersRespawnColliding(playernum, x, y))
		return false;

	if (!P_CheckPosition(players[playernum].mo, x, y, NULL))
		return false;

	return true;
}

//
// G_SpawnPlayer
// Spawn a player in a spot appropriate for the gametype --
// or a not-so-appropriate spot, if it initially fails
// due to a lack of starts open or something.
//
void G_SpawnPlayer(INT32 playernum)
{
	if (!playeringame[playernum])
		return;

	P_SpawnPlayer(playernum);
	G_MovePlayerToSpawnOrStarpost(playernum);
	LUA_HookPlayer(&players[playernum], HOOK(PlayerSpawn)); // Lua hook for player spawning :)
}

void G_MovePlayerToSpawnOrStarpost(INT32 playernum)
{
#if 0
	if (leveltime <= introtime && !players[playernum].spectator)
		P_MovePlayerToSpawn(playernum, G_FindMapStart(playernum));
	else
		P_MovePlayerToStarpost(playernum);
#else
	// Player's first spawn should be at the "map start".
	// I.e. level load or join mid game.
	if (leveltime > starttime && players[playernum].jointime > 1 && K_PodiumSequence() == false)
		P_MovePlayerToStarpost(playernum);
	else
		P_MovePlayerToSpawn(playernum, G_FindMapStart(playernum));
#endif
}

mapthing_t *G_FindTeamStart(INT32 playernum)
{
	const boolean doprints = P_IsLocalPlayer(&players[playernum]);
	INT32 i,j;

	if (!numredctfstarts && !numbluectfstarts) //why even bother, eh?
	{
		if ((gametyperules & GTR_TEAMSTARTS) && doprints)
			CONS_Alert(CONS_WARNING, M_GetText("No CTF starts in this map!\n"));
		return NULL;
	}

	if ((!players[playernum].ctfteam && numredctfstarts && (!numbluectfstarts || P_RandomChance(PR_PLAYERSTARTS, FRACUNIT/2))) || players[playernum].ctfteam == 1) //red
	{
		if (!numredctfstarts)
		{
			if (doprints)
				CONS_Alert(CONS_WARNING, M_GetText("No Red Team starts in this map!\n"));
			return NULL;
		}

		for (j = 0; j < 32; j++)
		{
			i = P_RandomKey(PR_PLAYERSTARTS, numredctfstarts);
			if (G_CheckSpot(playernum, redctfstarts[i]))
				return redctfstarts[i];
		}

		if (doprints)
			CONS_Alert(CONS_WARNING, M_GetText("Could not spawn at any Red Team starts!\n"));
		return NULL;
	}
	else if (!players[playernum].ctfteam || players[playernum].ctfteam == 2) //blue
	{
		if (!numbluectfstarts)
		{
			if (doprints)
				CONS_Alert(CONS_WARNING, M_GetText("No Blue Team starts in this map!\n"));
			return NULL;
		}

		for (j = 0; j < 32; j++)
		{
			i = P_RandomKey(PR_PLAYERSTARTS, numbluectfstarts);
			if (G_CheckSpot(playernum, bluectfstarts[i]))
				return bluectfstarts[i];
		}
		if (doprints)
			CONS_Alert(CONS_WARNING, M_GetText("Could not spawn at any Blue Team starts!\n"));
		return NULL;
	}
	//should never be reached but it gets stuff to shut up
	return NULL;
}

mapthing_t *G_FindBattleStart(INT32 playernum)
{
	const boolean doprints = P_IsLocalPlayer(&players[playernum]);
	INT32 i, j;

	if (numdmstarts)
	{
		for (j = 0; j < 64; j++)
		{
			i = P_RandomKey(PR_PLAYERSTARTS, numdmstarts);
			if (G_CheckSpot(playernum, deathmatchstarts[i]))
				return deathmatchstarts[i];
		}
		if (doprints)
			CONS_Alert(CONS_WARNING, M_GetText("Could not spawn at any Deathmatch starts!\n"));
		return NULL;
	}

	if ((gametyperules & GTR_BATTLESTARTS) && doprints)
		CONS_Alert(CONS_WARNING, M_GetText("No Deathmatch starts in this map!\n"));
	return NULL;
}

mapthing_t *G_FindRaceStart(INT32 playernum)
{
	const boolean doprints = P_IsLocalPlayer(&players[playernum]);

	if (numcoopstarts)
	{
		UINT8 i;
		UINT8 pos = 0;

		// SRB2Kart: figure out player spawn pos from points
		if (!playeringame[playernum] || players[playernum].spectator)
		{
			return playerstarts[0]; // go to first spot if you're a spectator
		}

		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (!playeringame[i] || players[i].spectator)
				continue;
			if (i == playernum)
				continue;

			if (players[i].score < players[playernum].score)
			{
				UINT8 j;
				UINT8 num = 0;

				for (j = 0; j < MAXPLAYERS; j++) // I hate similar loops inside loops... :<
				{
					if (!playeringame[j] || players[j].spectator)
						continue;
					if (j == playernum)
						continue;
					if (j == i)
						continue;

					if ((netgame || (demo.playback && demo.netgame)) && cv_kartusepwrlv.value)
					{
						if (clientpowerlevels[j][PWRLV_RACE] == clientpowerlevels[i][PWRLV_RACE])
							num++;
					}
					else
					{
						if (players[j].score == players[i].score)
							num++;
					}
				}

				if (num > 1) // found dupes
					pos++;
			}
			else
			{
				if (i < playernum)
					pos++;
				else
				{
					if ((netgame || (demo.playback && demo.netgame)) && cv_kartusepwrlv.value)
					{
						if (clientpowerlevels[i][PWRLV_RACE] > clientpowerlevels[playernum][PWRLV_RACE])
							pos++;
					}
					else
					{
						if (players[i].score > players[playernum].score)
							pos++;
					}
				}
			}
		}

		if (G_CheckSpot(playernum, playerstarts[pos % numcoopstarts]))
			return playerstarts[pos % numcoopstarts];

		// Your spot isn't available? Find whatever you can get first.
		for (i = 0; i < numcoopstarts; i++)
		{
			if (G_CheckSpot(playernum, playerstarts[i]))
				return playerstarts[i];
		}

		// SRB2Kart: We have solid players, so this behavior is less ideal.
		// Don't bother checking to see if the player 1 start is open.
		// Just spawn there.
		//return playerstarts[0];

		if (doprints)
			CONS_Alert(CONS_WARNING, M_GetText("Could not spawn at any Race starts!\n"));
		return NULL;
	}

	if (doprints)
		CONS_Alert(CONS_WARNING, M_GetText("No Race starts in this map!\n"));
	return NULL;
}

mapthing_t *G_FindPodiumStart(INT32 playernum)
{
	const boolean doprints = P_IsLocalPlayer(&players[playernum]);

	if (numcoopstarts)
	{
		UINT8 pos = K_GetPodiumPosition(&players[playernum]) - 1;
		UINT8 i;

		if (G_CheckSpot(playernum, playerstarts[pos % numcoopstarts]))
		{
			return playerstarts[pos % numcoopstarts];
		}

		// Your spot isn't available? Find whatever you can get first.
		for (i = 0; i < numcoopstarts; i++)
		{
			if (G_CheckSpot(playernum, playerstarts[(pos + i) % numcoopstarts]))
			{
				return playerstarts[(pos + i) % numcoopstarts];
			}
		}

		if (doprints)
		{
			CONS_Alert(CONS_WARNING, M_GetText("Could not spawn at any Podium starts!\n"));
		}

		return NULL;
	}

	if (doprints)
		CONS_Alert(CONS_WARNING, M_GetText("No Podium starts in this map!\n"));
	return NULL;
}

// Find a Co-op start, or fallback into other types of starts.
static inline mapthing_t *G_FindRaceStartOrFallback(INT32 playernum)
{
	mapthing_t *spawnpoint = NULL;
	if (!(spawnpoint = G_FindRaceStart(playernum)) // find a Race start
	&& !(spawnpoint = G_FindBattleStart(playernum))) // find a DM start
		spawnpoint = G_FindTeamStart(playernum); // fallback
	return spawnpoint;
}

// Find a Match start, or fallback into other types of starts.
static inline mapthing_t *G_FindBattleStartOrFallback(INT32 playernum)
{
	mapthing_t *spawnpoint = NULL;
	if (!(spawnpoint = G_FindBattleStart(playernum)) // find a DM start
	&& !(spawnpoint = G_FindTeamStart(playernum))) // find a CTF start
		spawnpoint = G_FindRaceStart(playernum); // fallback
	return spawnpoint;
}

static inline mapthing_t *G_FindTeamStartOrFallback(INT32 playernum)
{
	mapthing_t *spawnpoint = NULL;
	if (!(spawnpoint = G_FindTeamStart(playernum)) // find a CTF start
	&& !(spawnpoint = G_FindBattleStart(playernum))) // find a DM start
		spawnpoint = G_FindRaceStart(playernum); // fallback
	return spawnpoint;
}

mapthing_t *G_FindMapStart(INT32 playernum)
{
	mapthing_t *spawnpoint;

	if (!playeringame[playernum])
		return NULL;

	// -- Podium -- 
	// Single special behavior
	if (K_PodiumSequence() == true)
		spawnpoint = G_FindPodiumStart(playernum);

	// -- Time Attack --
	// Order: Race->DM->CTF
	else if (K_TimeAttackRules() == true)
		spawnpoint = G_FindRaceStartOrFallback(playernum);

	// -- CTF --
	// Order: CTF->DM->Race
	else if ((gametyperules & GTR_TEAMSTARTS) && players[playernum].ctfteam)
		spawnpoint = G_FindTeamStartOrFallback(playernum);

	// -- DM/Tag/CTF-spectator/etc --
	// Order: DM->CTF->Race
	else if (gametyperules & GTR_BATTLESTARTS)
		spawnpoint = G_FindBattleStartOrFallback(playernum);

	// -- Other game modes --
	// Order: Race->DM->CTF
	else
		spawnpoint = G_FindRaceStartOrFallback(playernum);

	//No spawns found. ANYWHERE.
	if (!spawnpoint)
	{
		if (nummapthings)
		{
			if (P_IsLocalPlayer(&players[playernum]))
				CONS_Alert(CONS_ERROR, M_GetText("No player spawns found, spawning at the first mapthing!\n"));
			spawnpoint = &mapthings[0];
		}
		else
		{
			if (P_IsLocalPlayer(&players[playernum]))
				CONS_Alert(CONS_ERROR, M_GetText("No player spawns found, spawning at the origin!\n"));
		}
	}

	return spawnpoint;
}

// Go back through all the projectiles and remove all references to the old
// player mobj, replacing them with the new one.
void G_ChangePlayerReferences(mobj_t *oldmo, mobj_t *newmo)
{
	thinker_t *th;
	mobj_t *mo2;

	I_Assert((oldmo != NULL) && (newmo != NULL));

	// scan all thinkers
	for (th = thlist[THINK_MOBJ].next; th != &thlist[THINK_MOBJ]; th = th->next)
	{
		if (th->function.acp1 == (actionf_p1)P_RemoveThinkerDelayed)
			continue;

		mo2 = (mobj_t *)th;

		if (!(mo2->flags & MF_MISSILE))
			continue;

		if (mo2->target == oldmo)
		{
			P_SetTarget(&mo2->target, newmo);
			mo2->flags2 |= MF2_BEYONDTHEGRAVE; // this mobj belongs to a player who has reborn
		}
	}
}

//
// G_DoReborn
//
void G_DoReborn(INT32 playernum)
{
	player_t *player = &players[playernum];

	// Make sure objectplace is OFF when you first start the level!
	OP_ResetObjectplace();

	{
		// respawn at the start
		mobj_t *oldmo = NULL;

		// first dissasociate the corpse
		if (player->mo)
		{
			oldmo = player->mo;

			// Don't leave your carcass stuck 10-billion feet in the ground!
			P_RemoveMobj(player->mo);
			P_SetTarget(&player->mo, NULL);
		}

		G_SpawnPlayer(playernum);
		if (oldmo)
			G_ChangePlayerReferences(oldmo, players[playernum].mo);
	}
}

void G_AddPlayer(INT32 playernum)
{
	player_t *p = &players[playernum];
	p->playerstate = PST_REBORN;
	demo_extradata[playernum] |= DXD_JOINDATA|DXD_PLAYSTATE|DXD_COLOR|DXD_NAME|DXD_SKIN|DXD_FOLLOWER; // Set everything
}

void G_BeginLevelExit(void)
{
	g_exit.losing = true;
	g_exit.retry = false;

	if (grandprixinfo.gp == true)
	{
		UINT8 i;

		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (playeringame[i] && !players[i].spectator)
			{
				K_PlayerFinishGrandPrix(&players[i]);
			}
		}
	}

	if (!G_GametypeUsesLives() || skipstats != 0)
	{
		g_exit.losing = false; // never force a retry
	}
	else if (specialstageinfo.valid == true || (gametyperules & GTR_BOSS))
	{
		UINT8 i;

		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (playeringame[i] && !players[i].spectator && !players[i].bot)
			{
				if (!K_IsPlayerLosing(&players[i]))
				{
					g_exit.losing = false;
					break;
				}
			}
		}
	}
	else if (grandprixinfo.gp == true && grandprixinfo.eventmode == GPEVENT_NONE)
	{
		g_exit.losing = (grandprixinfo.wonround != true);
	}

	if (g_exit.losing)
	{
		// You didn't win...

		UINT8 i;

		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (playeringame[i] && !players[i].spectator && !players[i].bot)
			{
				if (players[i].lives > 0)
				{
					g_exit.retry = true;
					break;
				}
			}
		}
	}

	if (g_exit.losing && specialstageinfo.valid)
	{
		exitcountdown = TICRATE;
	}
	else
	{
		exitcountdown = raceexittime+1;
	}

	if (g_exit.losing)
	{
		if (!g_exit.retry)
		{
			ACS_RunGameOverScript();
		}
	}
}

void G_FinishExitLevel(void)
{
	G_ResetAllDeviceRumbles();

	if (gamestate == GS_LEVEL)
	{
		if (g_exit.retry)
		{
			// Restart cup here whenever we do Online GP
			if (!netgame)
			{
				// We have lives, just redo this one course.
				G_SetRetryFlag();
				return;
			}
		}
		else if (g_exit.losing)
		{
			// We were in a Special Stage.
			// We can still progress to the podium when we game over here.
			const boolean special = grandprixinfo.gp == true && grandprixinfo.eventmode == GPEVENT_SPECIAL;

			if (!netgame && !special)
			{
				// Back to the menu with you.
				G_HandleSaveLevel(true);
				D_QuitNetGame();
				CL_Reset();
				D_ClearState();
				M_StartControlPanel();
				return;
			}
		}

		gameaction = ga_completed;
		lastdraw = true;

		// If you want your teams scrambled on map change, start the process now.
		// The teams will scramble at the start of the next round.
		if (cv_scrambleonchange.value && G_GametypeHasTeams())
		{
			if (server)
				CV_SetValue(&cv_teamscramble, cv_scrambleonchange.value);
		}

		CON_LogMessage(M_GetText("The round has ended.\n"));

		// Remove CEcho text on round end.
		HU_ClearCEcho();
		HU_ClearTitlecardCEcho();

		// Don't save demos immediately here! Let standings write first
	}
	else if (gamestate == GS_CREDITS)
	{
		F_StartGameEvaluation();
	}
}

static gametype_t defaultgametypes[] =
{
	// GT_RACE
	{
		"Race",
		"GT_RACE",
		GTR_CIRCUIT|GTR_BOTS|GTR_ENCORE,
		TOL_RACE,
		int_time,
		0,
		0,
	},

	// GT_BATTLE
	{
		"Battle",
		"GT_BATTLE",
		GTR_SPHERES|GTR_BUMPERS|GTR_PAPERITEMS|GTR_POWERSTONES|GTR_KARMA|GTR_ITEMARROWS|GTR_PRISONS|GTR_BATTLESTARTS|GTR_POINTLIMIT|GTR_TIMELIMIT|GTR_OVERTIME|GTR_CLOSERPLAYERS,
		TOL_BATTLE,
		int_scoreortimeattack,
		0,
		3,
	},

	// GT_SPECIAL
	{
		"Special",
		"GT_SPECIAL",
		GTR_CATCHER|GTR_SPECIALSTART|GTR_ROLLINGSTART|GTR_CIRCUIT,
		TOL_SPECIAL,
		int_time,
		0,
		0,
	},

	// GT_VERSUS
	{
		"Versus",
		"GT_VERSUS",
		GTR_BOSS|GTR_SPHERES|GTR_BUMPERS|GTR_POINTLIMIT|GTR_CLOSERPLAYERS|GTR_NOCUPSELECT|GTR_ENCORE,
		TOL_VERSUS,
		int_scoreortimeattack,
		0,
		0,
	},

	// GT_TUTORIAL
	{
		"Tutorial",
		"GT_TUTORIAL",
		GTR_NOMP|GTR_NOCUPSELECT|GTR_NOPOSITION,
		TOL_TUTORIAL,
		int_none,
		0,
		0,
	},
};

gametype_t *gametypes[MAXGAMETYPES+1] =
{
	&defaultgametypes[GT_RACE],
	&defaultgametypes[GT_BATTLE],
	&defaultgametypes[GT_SPECIAL],
	&defaultgametypes[GT_VERSUS],
	&defaultgametypes[GT_TUTORIAL],
};

//
// G_GetGametypeByName
//
// Returns the number for the given gametype name string, or -1 if not valid.
//
INT32 G_GetGametypeByName(const char *gametypestr)
{
	INT32 i = 0;

	while (gametypes[i] != NULL)
	{
		if (!stricmp(gametypestr, gametypes[i]->name))
			return i;
		i++;
	}

	return -1; // unknown gametype
}

//
// G_GuessGametypeByTOL
//
// Returns the first valid number for the given typeoflevel, or -1 if not valid.
//
INT32 G_GuessGametypeByTOL(UINT32 tol)
{
	INT32 i = 0;

	while (gametypes[i] != NULL)
	{
		if (tol & gametypes[i]->tol)
			return i;
		i++;
	}

	return -1; // unknown gametype
}

//
// G_SetGametype
//
// Set a new gametype, also setting gametype rules accordingly. Yay!
//
void G_SetGametype(INT16 gtype)
{
	if (gtype < 0 || gtype > numgametypes)
	{
		I_Error("G_SetGametype: Bad gametype change %d (was %d/\"%s\")", gtype, gametype, gametypes[gametype]->name);
	}

	gametype = gtype;
}

//
// G_PrepareGametypeConstant
//
// Self-explanatory. Filters out "bad" characters.
//
char *G_PrepareGametypeConstant(const char *newgtconst)
{
	size_t r = 0; // read
	size_t w = 0; // write
	size_t len = strlen(newgtconst);
	char *gtconst = Z_Calloc(len + 4, PU_STATIC, NULL);
	char *tmpconst = Z_Calloc(len + 1, PU_STATIC, NULL);

	// Copy the gametype name.
	strcpy(tmpconst, newgtconst);

	// Make uppercase.
	strupr(tmpconst);

	// Prepare to write the new constant string now.
	strcpy(gtconst, "GT_");

	// Remove characters that will not be allowed in the constant string.
	for (; r < strlen(tmpconst); r++)
	{
		boolean writechar = true;
		char rc = tmpconst[r];
		switch (rc)
		{
			// Space, at sign and question mark
			case ' ':
			case '@':
			case '?':
			// Used for operations
			case '+':
			case '-':
			case '*':
			case '/':
			case '%':
			case '^':
			case '&':
			case '!':
			// Part of Lua's syntax
			case '#':
			case '=':
			case '~':
			case '<':
			case '>':
			case '(':
			case ')':
			case '{':
			case '}':
			case '[':
			case ']':
			case ':':
			case ';':
			case ',':
			case '.':
				writechar = false;
				break;
		}
		if (writechar)
		{
			gtconst[3 + w] = rc;
			w++;
		}
	}

	// Free the temporary string.
	Z_Free(tmpconst);

	// Finally, return the constant string.
	return gtconst;
}

tolinfo_t TYPEOFLEVEL[NUMTOLNAMES] = {
	{"RACE",TOL_RACE},
	{"BATTLE",TOL_BATTLE},
	{"SPECIAL",TOL_SPECIAL},
	{"VERSUS",TOL_VERSUS},
	{"TUTORIAL",TOL_TUTORIAL},
	{"TV",TOL_TV},
	{NULL, 0}
};

UINT32 lastcustomtol = (TOL_TV<<1);

//
// G_AddTOL
//
// Adds a type of level.
//
void G_AddTOL(UINT32 newtol, const char *tolname)
{
	INT32 i;
	for (i = 0; TYPEOFLEVEL[i].name; i++)
		;

	TYPEOFLEVEL[i].name = Z_StrDup(tolname);
	TYPEOFLEVEL[i].flag = newtol;
}

//
// G_GametypeUsesLives
//
// Returns true if the current gametype uses
// the lives system.  False otherwise.
//
boolean G_GametypeUsesLives(void)
{
	if (modeattacking || metalrecording) // NOT in Record Attack
		return false;

	if ((grandprixinfo.gp == true) // In Grand Prix
		&& grandprixinfo.eventmode != GPEVENT_BONUS) // NOT in bonus round
	{
		return true;
	}

	return false;
}

//
// G_GametypeHasTeams
//
// Returns true if the current gametype uses
// Red/Blue teams.  False otherwise.
//
boolean G_GametypeHasTeams(void)
{
	if (gametyperules & GTR_TEAMS)
	{
		// Teams forced on by this gametype
		return true;
	}
	else if (gametyperules & GTR_NOTEAMS)
	{
		// Teams forced off by this gametype
		return false;
	}

	// Teams are determined by the "teamplay" modifier!
	return false; // teamplay
}

//
// G_GametypeHasSpectators
//
// Returns true if the current gametype supports
// spectators.  False otherwise.
//
boolean G_GametypeHasSpectators(void)
{
	// TODO: this would make a great debug feature for release
#ifdef DEVELOP
	return true;
#endif
	return (netgame || (multiplayer && demo.netgame));
}

//
// G_SometimesGetDifferentEncore
//
// Because gametypes are no longer on the vote screen, all this does is sometimes flip encore mode.
// However, it remains a seperate function for long-term possibility.
//
INT16 G_SometimesGetDifferentEncore(void)
{
	boolean encorepossible = ((M_SecretUnlocked(SECRET_ENCORE, false) || encorescramble == 1)
		&& (gametyperules & GTR_ENCORE));
	UINT8 encoremodifier = 0;

	// FORCE to what was scrambled on intermission?
	if (encorepossible && encorescramble != -1)
	{
		// FORCE to what was scrambled on intermission
		if ((encorescramble != 0) != (cv_kartencore.value == 1))
		{
			encoremodifier = VOTE_MOD_ENCORE;
		}
	}

	return encoremodifier;
}

/** Get the typeoflevel flag needed to indicate support of a gametype.
  * \param gametype The gametype for which support is desired.
  * \return The typeoflevel flag to check for that gametype.
  * \author Graue <graue@oceanbase.org>
  */
UINT32 G_TOLFlag(INT32 pgametype)
{
	if (pgametype >= 0 && pgametype < numgametypes)
		return gametypes[pgametype]->tol;
	return 0;
}

UINT16 G_GetFirstMapOfGametype(UINT8 pgametype)
{
	UINT8 i = 0;
	UINT16 mapnum = NEXTMAP_INVALID;
	levelsearch_t templevelsearch;

	templevelsearch.cup = NULL;
	templevelsearch.typeoflevel = G_TOLFlag(pgametype);
	templevelsearch.cupmode = (!(gametypes[pgametype]->rules & GTR_NOCUPSELECT));
	templevelsearch.timeattack = false;
	templevelsearch.tutorial = false;
	templevelsearch.checklocked = true;

	if (templevelsearch.cupmode)
	{
		templevelsearch.cup = kartcupheaders;
		while (templevelsearch.cup && mapnum >= nummapheaders)
		{
			mapnum = M_GetFirstLevelInList(&i, &templevelsearch);
			i = 0;
			templevelsearch.cup = templevelsearch.cup->next;
		}
	}
	else
	{
		mapnum = M_GetFirstLevelInList(&i, &templevelsearch);
	}

	return mapnum;
}

static INT32 TOLMaps(UINT8 pgametype)
{
	INT32 num = 0;
	INT32 i;

	UINT32 tolflag = G_TOLFlag(pgametype);

	// Find all the maps that are ok
	for (i = 0; i < nummapheaders; i++)
	{
		if (mapheaderinfo[i] == NULL)
		{
			continue;
		}

		if (mapheaderinfo[i]->lumpnum == LUMPERROR)
		{
			continue;
		}

		if ((mapheaderinfo[i]->typeoflevel & tolflag) == 0)
		{
			continue;
		}

		if (mapheaderinfo[i]->menuflags & LF2_HIDEINMENU)
		{
			// Don't include hidden
			continue;
		}

		if (M_MapLocked(i + 1))
		{
			// Don't include locked
			continue;
		}

		num++;
	}

	return num;
}

/** Select a random map with the given typeoflevel flags.
  * If no map has those flags, this arbitrarily gives you map 1.
  * \param tolflags The typeoflevel flags to insist on. Other bits may
  *                 be on too, but all of these must be on.
  * \return A random map with those flags, 1-based, or 1 if no map
  *         has those flags.
  * \author Graue <graue@oceanbase.org>
  */
static UINT16 *g_allowedMaps = NULL;

#ifdef PARANOIA
static size_t g_randMapStack = 0;
#endif

UINT16 G_RandMap(UINT32 tolflags, UINT16 pprevmap, boolean ignoreBuffers, boolean callAgainSoon, UINT16 *extBuffer)
{
	INT32 allowedMapsCount = 0;
	INT32 extBufferCount = 0;
	UINT16 ret = 0;
	INT32 i, j;

#ifdef PARANOIA
	g_randMapStack++;
#endif

	if (g_allowedMaps == NULL)
	{
		g_allowedMaps = Z_Malloc(nummapheaders * sizeof(UINT16), PU_STATIC, NULL);
	}

	if (extBuffer != NULL)
	{
		for (i = 0; extBuffer[i] != UINT16_MAX; i++)
		{
			extBufferCount++;
		}
	}

tryAgain:

	for (i = 0; i < nummapheaders; i++)
	{
		if (mapheaderinfo[i] == NULL || mapheaderinfo[i]->lumpnum == LUMPERROR)
		{
			// Doesn't exist?
			continue;
		}

		if (i == pprevmap)
		{
			// We were just here.
			continue;
		}

		if ((mapheaderinfo[i]->typeoflevel & tolflags) == 0)
		{
			// Doesn't match our gametype.
			continue;
		}

		if (pprevmap == UINT16_MAX-1 // title demo hack (FUCK YOU, MAKE IT A BOOL)
			&& mapheaderinfo[i]->ghostCount == 0)
		{
			// Doesn't have any ghosts, so it's not suitable for title demos.
			continue;
		}

		if ((mapheaderinfo[i]->menuflags & LF2_HIDEINMENU) == LF2_HIDEINMENU)
		{
			// Not intended to be accessed in multiplayer.
			continue;
		}

		if (M_MapLocked(i + 1) == true)
		{
			// We haven't earned this one.
			continue;
		}

		if (ignoreBuffers == false)
		{
			if (mapheaderinfo[i]->justPlayed > 0)
			{
				// We just played this map, don't play it again.
				continue;
			}

			if (extBufferCount > 0)
			{
				boolean inExt = false;

				// An optional additional buffer,
				// to avoid duplicates on the voting screen.
				for (j = 0; j < extBufferCount; j++)
				{
					if (extBuffer[j] >= nummapheaders)
					{
						// Rest of buffer SHOULD be empty.
						break;
					}

					if (i == extBuffer[j])
					{
						// Map is in this other buffer, don't duplicate.
						inExt = true;
						break;
					}
				}

				if (inExt == true)
				{
					// Didn't make it out of this buffer, so don't add this map.
					continue;
				}
			}
		}

		// Got past the gauntlet, so we can allow this one.
		g_allowedMaps[ allowedMapsCount++ ] = i;
	}

	if (allowedMapsCount == 0)
	{
		// No maps are available.
		if (ignoreBuffers == false)
		{
			// Try again with ignoring the buffer before giving up.
			ignoreBuffers = true;
			goto tryAgain;
		}

		// Nothing else actually worked. Welp!
		// You just get whatever was added first.
		ret = 0;
	}
	else
	{
		ret = g_allowedMaps[ M_RandomKey(allowedMapsCount) ];
	}

	if (callAgainSoon == false)
	{
		Z_Free(g_allowedMaps);
		g_allowedMaps = NULL;

#ifdef PARANOIA
		// Crash if callAgainSoon was mishandled.
		I_Assert(g_randMapStack == 1);
#endif
	}

#ifdef PARANOIA
	g_randMapStack--;
#endif

	return ret;
}

void G_AddMapToBuffer(UINT16 map)
{
	if (mapheaderinfo[map]->justPlayed == 0) // Started playing a new map.
	{
		// Decrement every maps' justPlayed value.
		INT32 i;
		for (i = 0; i < nummapheaders; i++)
		{
			if (mapheaderinfo[i]->justPlayed > 0)
			{
				mapheaderinfo[i]->justPlayed--;
			}
		}
	}

	// Set our map's justPlayed value.
	mapheaderinfo[map]->justPlayed = TOLMaps(gametype) - VOTE_NUM_LEVELS;
	mapheaderinfo[map]->anger = 0; // Reset voting anger now that we're playing it
}

//
// G_UpdateVisited
//
void G_UpdateVisited(void)
{
	UINT8 i;
	UINT8 earnedEmblems;

	// No demos.
	if (demo.playback)
		return;

	// Check if every local player wiped out.
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i]) // Not here.
			continue;

		if (!P_IsLocalPlayer(&players[i])) // Not local.
			continue;

		if (players[i].spectator) // Not playing.
			continue;

		if (players[i].pflags & PF_NOCONTEST) // Sonic after not surviving.
			continue;
		break;
	}

	if (i == MAXPLAYERS) // Not a single living local soul?
		return;

	// Update visitation flags
	mapheaderinfo[prevmap]->records.mapvisited |= MV_BEATEN;

	if (encoremode == true)
	{
		mapheaderinfo[prevmap]->records.mapvisited |= MV_ENCORE;
	}

	if (modeattacking & ATTACKING_SPB)
	{
		mapheaderinfo[prevmap]->records.mapvisited |= MV_SPBATTACK;
	}

	if (modeattacking)
		G_UpdateRecordReplays();

	if ((earnedEmblems = M_CompletionEmblems()))
		CONS_Printf(M_GetText("\x82" "Earned %hu emblem%s for level completion.\n"), (UINT16)earnedEmblems, earnedEmblems > 1 ? "s" : "");

	M_UpdateUnlockablesAndExtraEmblems(true, true);
	G_SaveGameData();
}

void G_HandleSaveLevel(boolean removecondition)
{
	if (!grandprixinfo.gp || !grandprixinfo.cup
	|| splitscreen || netgame)
		return;

	if (removecondition)
		goto doremove;

	if (gamestate != GS_LEVEL
	|| roundqueue.size == 0)
		return;

	if (roundqueue.position == 1
	|| players[consoleplayer].lives <= 1) // because a life is lost on reload
		goto doremove;

	G_SaveGame();
	return;

doremove:
	if (FIL_FileExists(gpbackup))
		remove(gpbackup);
}

// Next map apparatus
struct roundqueue roundqueue;

void G_MapSlipIntoRoundQueue(UINT8 position, UINT16 map, UINT8 setgametype, boolean setencore, boolean rankrestricted)
{
	I_Assert(position < ROUNDQUEUE_MAX);

	roundqueue.entries[position].mapnum = map;
	roundqueue.entries[position].gametype = setgametype;
	roundqueue.entries[position].encore = setencore;
	roundqueue.entries[position].rankrestricted = rankrestricted;
}

void G_MapIntoRoundQueue(UINT16 map, UINT8 setgametype, boolean setencore, boolean rankrestricted)
{
	if (roundqueue.size >= ROUNDQUEUE_MAX)
	{
		CONS_Alert(CONS_ERROR, "G_MapIntoRoundQueue: Unable to add map beyond %u\n", roundqueue.size);
		return;
	}

	G_MapSlipIntoRoundQueue(roundqueue.size, map, setgametype, setencore, rankrestricted);
	roundqueue.size++;
}

void G_GPCupIntoRoundQueue(cupheader_t *cup, UINT8 setgametype, boolean setencore)
{
	UINT8 i, levelindex = 0, bonusindex = 0;
	UINT8 bonusmodulo = max(1, (cup->numlevels+1)/(cup->numbonus+1));
	UINT16 cupLevelNum;

	// Levels are added to the queue in the following pattern.
	// For 5 Race rounds and 2 Bonus rounds, the most common case:
	//    race - race - BONUS - race - race - BONUS - race
	// The system is flexible enough to permit other arrangements.
	// However, we just want to keep the pacing even & consistent.
	while (levelindex < cup->numlevels)
	{
		// Fill like two or three Race maps.
		for (i = 0; i < bonusmodulo; i++)
		{
			cupLevelNum = cup->cachedlevels[levelindex];

			if (cupLevelNum >= nummapheaders)
			{
				// For invalid Race maps, we keep the pacing by going to TEST RUN.
				// It transparently lets the user know something is wrong.
				cupLevelNum = 0;
			}

			G_MapIntoRoundQueue(
				cupLevelNum,
				setgametype,
				setencore, // *probably* correct
				false
			);

			levelindex++;
			if (levelindex >= cup->numlevels)
				break;
		}

		// Attempt to add an interstitial Bonus round.
		if (levelindex < cup->numlevels
			&& bonusindex < cup->numbonus)
		{
			cupLevelNum = cup->cachedlevels[CUPCACHE_BONUS + bonusindex];

			if (cupLevelNum < nummapheaders)
			{
				// In the case of Bonus rounds, we simply skip invalid maps.
				G_MapIntoRoundQueue(
					cupLevelNum,
					G_GuessGametypeByTOL(mapheaderinfo[cupLevelNum]->typeoflevel),
					setencore, // if this isn't correct, Got_Mapcmd will fix it
					false
				);
			}

			bonusindex++;
		}
	}

	// ...but there's one last trick up our sleeves.
	// At the end of the Cup is a Rank-restricted treat.
	// So we append it to the end of the roundqueue.
	// (as long as it exists, of course!)
	cupLevelNum = cup->cachedlevels[CUPCACHE_SPECIAL];
	if (cupLevelNum < nummapheaders)
	{
		G_MapIntoRoundQueue(
			cupLevelNum,
			G_GuessGametypeByTOL(mapheaderinfo[cupLevelNum]->typeoflevel),
			setencore, // if this isn't correct, Got_Mapcmd will fix it
			true // Rank-restricted!
		);
	}

	if (roundqueue.size == 0)
	{
		I_Error("G_CupToRoundQueue: roundqueue size is 0 after population!?");
	}
}

void G_GetNextMap(void)
{
	INT32 i;
	boolean setalready = false;

	if (!server)
	{
		// Server is authoriative, not you
		return;
	}

	deferencoremode = (cv_kartencore.value == 1);
	forceresetplayers = forcespecialstage = false;

	// go to next level
	// nextmap is 0-based, unlike gamemap
	if (nextmapoverride != 0)
	{
		nextmap = (nextmapoverride-1);
		setalready = true;
	}
	else if (roundqueue.size > 0)
	{
		// See also Y_CalculateMatchData, M_DrawPause
		boolean permitrank = false;
		if (grandprixinfo.gp == true
			&& grandprixinfo.gamespeed >= KARTSPEED_NORMAL)
		{
			// On A rank pace? Then you get a chance for S rank!
			permitrank = (K_CalculateGPGrade(&grandprixinfo.rank) >= GRADE_A);
		}

		while (roundqueue.position < roundqueue.size
			&& (roundqueue.entries[roundqueue.position].mapnum >= nummapheaders
			|| mapheaderinfo[roundqueue.entries[roundqueue.position].mapnum] == NULL
			|| (netgame && (gametypes[roundqueue.entries[roundqueue.position].gametype]->rules & GTR_FORBIDMP))
			|| (permitrank == false && roundqueue.entries[roundqueue.position].rankrestricted == true)))
		{
			// Skip all restricted queue entries.
			roundqueue.position++;
		}

		if (roundqueue.position < roundqueue.size)
		{
			// The next entry in the queue is valid; set it as nextmap!
			nextmap = roundqueue.entries[roundqueue.position].mapnum;
			deferencoremode = roundqueue.entries[roundqueue.position].encore;

			// And we handle gametype changes, too.
			if (roundqueue.entries[roundqueue.position].gametype != gametype)
			{
				INT32 lastgametype = gametype;
				G_SetGametype(roundqueue.entries[roundqueue.position].gametype);
				D_GameTypeChanged(lastgametype);
			}

			// Is this special..?
			forcespecialstage = roundqueue.entries[roundqueue.position].rankrestricted;

			// On entering roundqueue mode, kill the non-PWR between-round scores.
			// This makes it viable as a future tournament mode base.
			if (roundqueue.position == 0)
			{
				forceresetplayers = true;
			}

			// Handle primary queue position update.
			roundqueue.position++;
			if (grandprixinfo.gp == false || gametype == roundqueue.entries[0].gametype)
			{
				roundqueue.roundnum++;
			}

			setalready = true;
		}
		else
		{
			// Wipe the queue info.
			memset(&roundqueue, 0, sizeof(struct roundqueue));

			if (grandprixinfo.gp == true)
			{
				// In GP, we're now ready to go to the ceremony.
				nextmap = NEXTMAP_CEREMONY;
				setalready = true;
			}
			else
			{
				// On exiting roundqueue mode, kill the non-PWR between-round scores.
				// This prevents future tournament winners from carrying their wins out.
				forceresetplayers = true;
			}
		}

		// Make sure the next D_MapChange sends updated roundqueue state.
		roundqueue.netcommunicate = true;
	}
	else if (grandprixinfo.gp == true)
	{
		// Fast And Rapid Testing
		// this codepath is exclusively accessible through console/command line
		nextmap = prevmap;
		setalready = true;
	}

	if (setalready == false)
	{
		UINT32 tolflag = G_TOLFlag(gametype);
		register INT16 cm;

		if (!(gametyperules & GTR_NOCUPSELECT))
		{
			cupheader_t *cup = mapheaderinfo[gamemap-1]->cup;
			UINT8 gettingresult = 0;

			while (cup)
			{
				// Not unlocked? Grab the next result afterwards
				if (!marathonmode && M_CupLocked(cup))
				{
					cup = cup->next;
					gettingresult = 1;
					continue;
				}

				for (i = 0; i < cup->numlevels; i++)
				{
					cm = cup->cachedlevels[i];

					// Not valid?
					if (cm >= nummapheaders
						|| !mapheaderinfo[cm]
						|| mapheaderinfo[cm]->lumpnum == LUMPERROR
						|| !(mapheaderinfo[cm]->typeoflevel & tolflag)
						|| (!marathonmode && M_MapLocked(cm+1)))
						continue;

					// If the map is in multiple cups, only consider the first one valid.
					if (mapheaderinfo[cm]->cup != cup)
					{
						continue;
					}

					// Grab the first valid after the map you're on
					if (gettingresult)
					{
						nextmap = cm;
						gettingresult = 2;
						break;
					}

					// Not the map you're on?
					if (cm != prevmap)
					{
						continue;
					}

					// Ok, this is the current map, time to get the next
					gettingresult = 1;
				}

				// We have a good nextmap?
				if (gettingresult == 2)
				{
					break;
				}

				// Ok, iterate to the next
				cup = cup->next;
			}

			// Didn't get a nextmap before reaching the end?
			if (gettingresult != 2)
			{
				nextmap = NEXTMAP_CEREMONY; // ceremonymap
			}
		}
		else
		{
			cm = prevmap;
			if (++cm >= nummapheaders)
				cm = 0;

			while (cm != prevmap)
			{
				if (!mapheaderinfo[cm]
					|| mapheaderinfo[cm]->lumpnum == LUMPERROR
					|| !(mapheaderinfo[cm]->typeoflevel & tolflag)
					|| (mapheaderinfo[cm]->menuflags & LF2_HIDEINMENU)
					|| M_MapLocked(cm+1))
				{
					if (++cm >= nummapheaders)
						cm = 0;
					continue;
				}

				break;
			}

			nextmap = cm;
		}

		if (K_CanChangeRules(true))
		{
			if (!netgame) // Match Race.
				nextmap = NEXTMAP_TITLE;
			else switch (cv_advancemap.value)
			{
				case 0: // Stay on same map.
					nextmap = prevmap;
					break;
				case 3: // Voting screen.
					{
						for (i = 0; i < MAXPLAYERS; i++)
						{
							if (!playeringame[i])
								continue;
							if (players[i].spectator)
								continue;
							break;
						}
						if (i != MAXPLAYERS)
						{
							nextmap = NEXTMAP_VOTING;
							break;
						}
					}
					/* FALLTHRU */
				case 2: // Go to random map.
					nextmap = G_RandMap(G_TOLFlag(gametype), prevmap, false, false, NULL);
					break;
				default:
					if (nextmap >= NEXTMAP_SPECIAL) // Loop back around
					{
						nextmap = G_GetFirstMapOfGametype(gametype);
					}
					break;
			}
		}
	}

	// We are committed to this map now.
	if (nextmap == NEXTMAP_INVALID || (nextmap < NEXTMAP_SPECIAL && (nextmap >= nummapheaders || !mapheaderinfo[nextmap] || mapheaderinfo[nextmap]->lumpnum == LUMPERROR)))
		I_Error("G_GetNextMap: Internal map ID %d not found (nummapheaders = %d)\n", nextmap, nummapheaders);

#if 0 // This is a surprise tool that will help us later.
	if (!spec)
#endif //#if 0
		lastmap = nextmap;
}

//
// G_DoCompleted
//
static void G_DoCompleted(void)
{
	INT32 i, j = 0;

	if (modeattacking && pausedelay)
		pausedelay = 0;

	// We do this here so Challenges-related sounds aren't insta-killed
	S_StopSounds();

	if (legitimateexit && !demo.playback && !mapreset) // (yes you're allowed to unlock stuff this way when the game is modified)
	{
		if (gametype != GT_TUTORIAL)
		{
			UINT8 roundtype = GDGT_CUSTOM;

			if (gametype == GT_RACE)
				roundtype = GDGT_RACE;
			else if (gametype == GT_BATTLE)
				roundtype = (battleprisons ? GDGT_PRISONS : GDGT_BATTLE);
			else if (gametype == GT_SPECIAL || gametype == GT_VERSUS)
				roundtype = GDGT_SPECIAL;

			gamedata->roundsplayed[roundtype]++;
		}
		gamedata->pendingkeyrounds++;

		// Done before forced addition of PF_NOCONTEST to make UCRP_NOCONTEST harder to achieve
		M_UpdateUnlockablesAndExtraEmblems(true, true);
		gamedata->deferredsave = true;
	}

	if (gamedata->deferredsave)
		G_SaveGameData();

	legitimateexit = false;

	gameaction = ga_nothing;

	if (metalplayback)
		G_StopMetalDemo();
	if (metalrecording)
		G_StopMetalRecording(false);

	G_SetGamestate(GS_NULL);
	wipegamestate = GS_NULL;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i])
		{
			// Exitlevel shouldn't get you the points
			if (!players[i].exiting && !(players[i].pflags & PF_NOCONTEST))
			{
				clientPowerAdd[i] = 0;

				if (players[i].bot)
				{
					K_FakeBotResults(&players[i]);
				}
				else
				{
					players[i].pflags |= PF_NOCONTEST;

					if (P_IsLocalPlayer(&players[i]))
					{
						j++;
					}
				}
			}

			G_PlayerFinishLevel(i); // take away cards and stuff
		}
	}

	if (automapactive)
		AM_Stop();

	prevmap = (INT16)(gamemap-1);

	if (!demo.playback)
	{
		// Set up power level gametype scrambles
		K_SetPowerLevelScrambles(K_UsingPowerLevels());
	}

	// If the current gametype has no intermission screen set, then don't start it.
	Y_DetermineIntermissionType();

	if (intertype == int_none)
	{
		G_UpdateVisited();
		K_UpdateGPRank();
		G_AfterIntermission();
	}
	else
	{
		G_SetGamestate(GS_INTERMISSION);
		Y_StartIntermission();
		G_UpdateVisited();
	}
}

// See also F_EndCutscene, the only other place which handles intra-map/ending transitions
void G_AfterIntermission(void)
{
	if (gamecomplete == 2) // special temporary mode to prevent using SP level select in pause menu until the intermission is over without restricting it in every intermission
		gamecomplete = 1;

	HU_ClearCEcho();
	HU_ClearTitlecardCEcho();

	if (demo.playback)
	{
		M_PlaybackQuit(0);
		return;
	}
	else if (demo.recording && (modeattacking || demo.savemode != DSM_NOTSAVING))
		G_SaveDemo();

	if (modeattacking) // End the run.
	{
		M_EndModeAttackRun();
		return;
	}

	if (gamestate != GS_VOTING)
	{
		G_GetNextMap();
	}

	if ((grandprixinfo.gp == true) && mapheaderinfo[prevmap]->cutscenenum && !modeattacking && skipstats <= 1 && (gamecomplete || !(marathonmode & MA_NOCUTSCENES))) // Start a custom cutscene.
		F_StartCustomCutscene(mapheaderinfo[prevmap]->cutscenenum-1, false, false);
	else
	{
		G_NextLevel();
	}
}

//
// G_NextLevel (WorldDone)
//
// init next level or go to the final scene
// called by end of intermission screen (y_inter)
//
void G_NextLevel(void)
{
	if (nextmap >= NEXTMAP_SPECIAL)
	{
		G_EndGame();
		return;
	}

	gameaction = ga_worlddone;
}

static void G_DoWorldDone(void)
{
	if (server)
	{
		// SRB2Kart
		D_MapChange(nextmap+1,
			gametype,
			deferencoremode,
			forceresetplayers,
			0,
			false,
			forcespecialstage);
	}

	gameaction = ga_nothing;
}

//
// G_DoStartVote
//
static void G_DoStartVote(void)
{
	if (server)
	{
		if (gamestate == GS_VOTING)
			I_Error("G_DoStartVote: NEXTMAP_VOTING causes recursive vote!");
		D_SetupVote();
	}
	gameaction = ga_nothing;
}

//
// G_UseContinue
//
void G_UseContinue(void)
{
	if (gamestate == GS_LEVEL && !netgame && !multiplayer)
	{
		gameaction = ga_startcont;
		lastdraw = true;
	}
}

static void G_DoStartContinue(void)
{
	I_Assert(!netgame && !multiplayer);

	legitimateexit = false;
	G_PlayerFinishLevel(consoleplayer); // take away cards and stuff

	//F_StartContinue();
	gameaction = ga_nothing;
}

//
// G_Continue
//
// re-init level, used by continue and possibly countdowntimeup
//
void G_Continue(void)
{
	if (!netgame && !multiplayer)
		gameaction = ga_continued;
}

static void G_DoContinued(void)
{
	player_t *pl = &players[consoleplayer];
	I_Assert(!netgame && !multiplayer);
	//I_Assert(pl->continues > 0);

	/*if (pl->continues)
		pl->continues--;*/

	// Reset score
	pl->score = 0;

	// Reset # of lives
	pl->lives = 3;

	D_MapChange(gamemap, gametype, false, false, 0, false, false);

	gameaction = ga_nothing;
}

//
// G_EndGame (formerly Y_EndGame)
// Frankly this function fits better in g_game.c than it does in y_inter.c
//
// ...Gee, (why) end the game?
// Because G_AfterIntermission and F_EndCutscene would
// both do this exact same thing *in different ways* otherwise,
// which made it so that you could only unlock Ultimate mode
// if you had a cutscene after the final level and crap like that.
// This function simplifies it so only one place has to be updated
// when something new is added.
void G_EndGame(void)
{
	// Handle voting
	if (nextmap == NEXTMAP_VOTING)
	{
		gameaction = ga_startvote;
		return;
	}

	// Only do evaluation and credits in singleplayer contexts
	if (!netgame && grandprixinfo.gp == true)
	{
		G_HandleSaveLevel(true);

		if (nextmap == NEXTMAP_CEREMONY) // end game with ceremony
		{
			if (K_StartCeremony() == true)
			{
				return;
			}
		}
		if (nextmap == NEXTMAP_CREDITS) // end game with credits
		{
			F_StartCredits();
			return;
		}
		if (nextmap == NEXTMAP_EVALUATION) // end game with evaluation
		{
			F_StartGameEvaluation();
			return;
		}
	}

	// In a netgame, don't unwittingly boot everyone.
	if (netgame)
	{
		Music_StopAll();
		G_SetGamestate(GS_WAITINGPLAYERS); // hack to prevent a command repeat

		if (server)
		{
			UINT16 map = G_GetFirstMapOfGametype(gametype)+1;

			if (map > nummapheaders)
				I_Error("G_EndGame: No valid map ID found!?");

			COM_BufAddText(va("map %s\n", G_BuildMapName(map)));
		}

		return;
	}

	// Time to return to the menu.
	D_ClearState();
	M_StartControlPanel();
}

//
// G_LoadGameSettings
//
// Sets a tad of default info we need.
void G_LoadGameSettings(void)
{
	INT32 i;

	// initialize free sfx slots for skin sounds
	S_InitRuntimeSounds();

	// Prepare skincolor material.
	for (i = 0; i < MAXSKINCOLORS; i++)
	{
		Color_cons_t[i].value = Followercolor_cons_t[i+2].value = i;
		Color_cons_t[i].strvalue = Followercolor_cons_t[i+2].strvalue = skincolors[i].name;
	}

	Followercolor_cons_t[1].value = FOLLOWERCOLOR_MATCH;
	Followercolor_cons_t[1].strvalue = "Match"; // Add "Match" option, which will make the follower color match the player's

	Followercolor_cons_t[0].value = FOLLOWERCOLOR_OPPOSITE;
	Followercolor_cons_t[0].strvalue = "Opposite"; // Add "Opposite" option, ...which is like "Match", but for coloropposite.

	Color_cons_t[MAXSKINCOLORS].value = Followercolor_cons_t[MAXSKINCOLORS+2].value = 0;
	Color_cons_t[MAXSKINCOLORS].strvalue = Followercolor_cons_t[MAXSKINCOLORS+2].strvalue = NULL;
}

#define GD_VERSIONCHECK 0xBA5ED123 // Change every major version, as usual
#define GD_VERSIONMINOR 6 // Change every format update

typedef enum
{
	GDEVER_ADDON = 1,
	GDEVER_CREDITS = 1<<1,
	GDEVER_REPLAY = 1<<2,
	GDEVER_SPECIAL = 1<<3,
} gdeverdone_t;

static const char *G_GameDataFolder(void)
{
	if (strcmp(srb2home,"."))
		return srb2home;
	else
		return "the Ring Racers folder";
}

// G_LoadGameData
// Loads the main data file, which stores information such as emblems found, etc.
void G_LoadGameData(void)
{
	UINT32 i, j;
	UINT32 versionID;
	UINT8 versionMinor;
	UINT8 rtemp;
	boolean gridunusable = false;
	savebuffer_t save = {0};

	UINT16 emblemreadcount = MAXEMBLEMS;
	UINT16 unlockreadcount = MAXUNLOCKABLES;
	UINT16 conditionreadcount = MAXCONDITIONSETS;
	size_t unlockreadsize = sizeof(UINT16);

	//For records
	UINT32 numgamedataskins;
	UINT32 numgamedatamapheaders;
	UINT32 numgamedatacups;

	// Stop saving, until we successfully load it again.
	gamedata->loaded = false;

	// Clear things so previously read gamedata doesn't transfer
	// to new gamedata
	// see also M_EraseDataResponse
	G_ClearRecords(); // records
	M_ClearStats(); // statistics
	M_ClearSecrets(); // emblems, unlocks, maps visited, etc

	if (M_CheckParm("-nodata"))
	{
		// Don't load at all.
		// The following used to be in M_ClearSecrets, but that was silly.
		M_UpdateUnlockablesAndExtraEmblems(false, true);
		return;
	}

	if (M_CheckParm("-resetdata"))
	{
		// Don't load, but do save. (essentially, reset)
		goto finalisegamedata;
	}

	if (P_SaveBufferFromFile(&save, va(pandf, srb2home, gamedatafilename)) == false)
	{
		// No gamedata. We can save a new one.
		goto finalisegamedata;
	}

	// Version check
	versionID = READUINT32(save.p);
	if (versionID != GD_VERSIONCHECK)
	{
		const char *gdfolder = G_GameDataFolder();

		P_SaveBufferFree(&save);
		I_Error("Game data is not for Ring Racers v2.0.\nDelete %s (maybe in %s) and try again.", gamedatafilename, gdfolder);
	}

	versionMinor = READUINT8(save.p);
	if (versionMinor > GD_VERSIONMINOR)
	{
		const char *gdfolder = G_GameDataFolder();

		P_SaveBufferFree(&save);
		I_Error("Game data is from the future! (expected %d, got %d)\nRename or delete %s (maybe in %s) and try again.", GD_VERSIONMINOR, versionMinor, gamedatafilename, gdfolder);
	}
	else if (versionMinor < GD_VERSIONMINOR)
	{
		// We're converting - let'd create a backup.
		FIL_WriteFile(va("%s" PATHSEP "%s.bak", srb2home, gamedatafilename), save.buffer, save.size);
	}

	if ((versionMinor == 0 || versionMinor == 1)
#ifdef DEVELOP
		|| M_CheckParm("-resetchallengegrid")
#endif
		)
	{
		gridunusable = true;
	}

	if (versionMinor > 1)
	{
		gamedata->evercrashed = (boolean)READUINT8(save.p);
	}

	gamedata->totalplaytime = READUINT32(save.p);

	if (versionMinor > 1)
	{
		gamedata->totalrings = READUINT32(save.p);

		for (i = 0; i < GDGT_MAX; i++)
		{
			gamedata->roundsplayed[i] = READUINT32(save.p);
		}

		gamedata->pendingkeyrounds = READUINT32(save.p);
		gamedata->pendingkeyroundoffset = READUINT8(save.p);

		if (versionMinor < 3)
		{
			gamedata->keyspending = READUINT8(save.p);
		}
		else
		{
			gamedata->keyspending = READUINT16(save.p);
		}

		gamedata->chaokeys = READUINT16(save.p);

		if (versionMinor >= 4)
		{
			UINT32 everflags = READUINT32(save.p);

			gamedata->everloadedaddon = !!(everflags & GDEVER_ADDON);
			gamedata->everfinishedcredits = !!(everflags & GDEVER_CREDITS);
			gamedata->eversavedreplay = !!(everflags & GDEVER_REPLAY);
			gamedata->everseenspecial = !!(everflags & GDEVER_SPECIAL);
		}
		else
		{
			gamedata->everloadedaddon = (boolean)READUINT8(save.p);
			gamedata->eversavedreplay = (boolean)READUINT8(save.p);
			gamedata->everseenspecial = (boolean)READUINT8(save.p);
		}
	}
	else
	{
		save.p += 4; // no direct equivalent to matchesplayed
	}

	{
		// Quick & dirty hash for what mod this save file is for.
		UINT32 modID = READUINT32(save.p);
		UINT32 expectedID = quickncasehash(timeattackfolder, 64);

		if (modID != expectedID)
		{
			// Aha! Someone's been screwing with the save file!
			goto datacorrupt;
		}
	}

	if (versionMinor < 3)
	{
		emblemreadcount = 512;
		unlockreadcount = conditionreadcount = UINT8_MAX;
		unlockreadsize = sizeof(UINT8);
	}

	// To save space, use one bit per collected/achieved/unlocked flag
	for (i = 0; i < emblemreadcount;)
	{
		rtemp = READUINT8(save.p);
		for (j = 0; j < 8 && j+i < emblemreadcount; ++j)
			gamedata->collected[j+i] = ((rtemp >> j) & 1);
		i += j;
	}
	for (i = 0; i < unlockreadcount;)
	{
		rtemp = READUINT8(save.p);
		for (j = 0; j < 8 && j+i < unlockreadcount; ++j)
			gamedata->unlocked[j+i] = ((rtemp >> j) & 1);
		i += j;
	}
	for (i = 0; i < unlockreadcount;)
	{
		rtemp = READUINT8(save.p);
		for (j = 0; j < 8 && j+i < unlockreadcount; ++j)
			gamedata->unlockpending[j+i] = ((rtemp >> j) & 1);
		i += j;
	}
	for (i = 0; i < conditionreadcount;)
	{
		rtemp = READUINT8(save.p);
		for (j = 0; j < 8 && j+i < conditionreadcount; ++j)
			gamedata->achieved[j+i] = ((rtemp >> j) & 1);
		i += j;
	}

	if (gridunusable)
	{
		UINT16 burn = READUINT16(save.p); // Previous challengegridwidth
		UINT8 height = (versionMinor > 0) ? CHALLENGEGRIDHEIGHT : 5;
		save.p += (burn * height * unlockreadsize); // Step over previous grid data

		gamedata->challengegridwidth = 0;
		Z_Free(gamedata->challengegrid);
		gamedata->challengegrid = NULL;
	}
	else
	{
		gamedata->challengegridwidth = READUINT16(save.p);
		Z_Free(gamedata->challengegrid);
		if (gamedata->challengegridwidth)
		{
			gamedata->challengegrid = Z_Malloc(
				(gamedata->challengegridwidth * CHALLENGEGRIDHEIGHT * sizeof(UINT16)),
				PU_STATIC, NULL);
			if (unlockreadsize == sizeof(UINT8))
			{
				for (i = 0; i < (gamedata->challengegridwidth * CHALLENGEGRIDHEIGHT); i++)
				{
					gamedata->challengegrid[i] = READUINT8(save.p);
					if (gamedata->challengegrid[i] == unlockreadcount)
						gamedata->challengegrid[i] = MAXUNLOCKABLES;
				}
			}
			else
			{
				for (i = 0; i < (gamedata->challengegridwidth * CHALLENGEGRIDHEIGHT); i++)
				{
					gamedata->challengegrid[i] = READUINT16(save.p);
				}
			}

			M_SanitiseChallengeGrid();
		}
		else
		{
			gamedata->challengegrid = NULL;
		}
	}

	gamedata->timesBeaten = READUINT32(save.p);

	// Main records

	skinreference_t *tempskinreferences = NULL;

	if (versionMinor < 3)
	{
		gamedata->importprofilewins = true;
		numgamedataskins = 0;
	}
	else
	{
		numgamedataskins = READUINT32(save.p);

		if (numgamedataskins)
		{
			tempskinreferences = Z_Malloc(
				numgamedataskins * sizeof (skinreference_t),
				PU_STATIC,
				NULL
			);

			for (i = 0; i < numgamedataskins; i++)
			{
				char skinname[SKINNAMESIZE+1];
				INT32 skin;

				READSTRINGN(save.p, skinname, SKINNAMESIZE);
				skin = R_SkinAvailable(skinname);

				skinrecord_t dummyrecord;

				dummyrecord.wins = READUINT32(save.p);
				dummyrecord._saveid = i;

				tempskinreferences[i].id = MAXSKINS;

				if (skin != -1)
				{
					// We found a skin, so assign the win.

					M_Memcpy(&skins[skin].records, &dummyrecord, sizeof(skinrecord_t));

					tempskinreferences[i].id = skin;
					tempskinreferences[i].unloaded = NULL;
				}
				else if (dummyrecord.wins)
				{
					// Invalid, but we don't want to lose all the juicy statistics.
					// Instead, update a FILO linked list of "unloaded skins".

					unloaded_skin_t *unloadedskin =
						Z_Malloc(
							sizeof(unloaded_skin_t),
							PU_STATIC, NULL
						);

					// Establish properties, for later retrieval on file add.
					strlcpy(unloadedskin->name, skinname, sizeof(unloadedskin->name));
					unloadedskin->namehash = quickncasehash(unloadedskin->name, SKINNAMESIZE);

					// Insert at the head, just because it's convenient.
					unloadedskin->next = unloadedskins;
					unloadedskins = unloadedskin;

					// Finally, copy into.
					M_Memcpy(&unloadedskin->records, &dummyrecord, sizeof(skinrecord_t));

					tempskinreferences[i].unloaded = unloadedskin;
				}
			}
		}
	}

	UINT16 *tempmapidreferences = NULL;

	numgamedatamapheaders = READUINT32(save.p);

	if (numgamedatamapheaders)
	{
		tempmapidreferences = Z_Malloc(
			numgamedatamapheaders * sizeof (UINT16),
			PU_STATIC,
			NULL
		);

		for (i = 0; i < numgamedatamapheaders; i++)
		{
			char mapname[MAXMAPLUMPNAME];
			UINT16 mapnum;

			READSTRINGL(save.p, mapname, MAXMAPLUMPNAME);
			mapnum = G_MapNumber(mapname);

			tempmapidreferences[i] = (UINT16)mapnum;

			recorddata_t dummyrecord;

			dummyrecord.mapvisited = READUINT8(save.p);
			dummyrecord.time = (tic_t)READUINT32(save.p);
			dummyrecord.lap  = (tic_t)READUINT32(save.p);

			if (mapnum < nummapheaders && mapheaderinfo[mapnum])
			{
				// Valid mapheader, time to populate with record data.

				dummyrecord.mapvisited &= MV_MAX;
				M_Memcpy(&mapheaderinfo[mapnum]->records, &dummyrecord, sizeof(recorddata_t));
			}
			else if (
				((dummyrecord.mapvisited & MV_PERSISTUNLOADED) != 0
					&& (dummyrecord.mapvisited & MV_BEATEN) != 0)
				|| dummyrecord.time != 0
				|| dummyrecord.lap != 0
			)
			{
				// Invalid, but we don't want to lose all the juicy statistics.
				// Instead, update a FILO linked list of "unloaded mapheaders".

				unloaded_mapheader_t *unloadedmap =
					Z_Malloc(
						sizeof(unloaded_mapheader_t),
						PU_STATIC, NULL
					);

				// Establish properties, for later retrieval on file add.
				unloadedmap->lumpname = Z_StrDup(mapname);
				unloadedmap->lumpnamehash = quickncasehash(unloadedmap->lumpname, MAXMAPLUMPNAME);

				// Insert at the head, just because it's convenient.
				unloadedmap->next = unloadedmapheaders;
				unloadedmapheaders = unloadedmap;

				// Finally, copy into.
				M_Memcpy(&unloadedmap->records, &dummyrecord, sizeof(recorddata_t));
			}
		}
	}

	if (versionMinor > 5)
	{
		gamedata->gotspraycans = 0;
		gamedata->numspraycans = READUINT16(save.p);
		Z_Free(gamedata->spraycans);

		if (gamedata->numspraycans)
		{
			gamedata->spraycans = Z_Malloc(
				(gamedata->numspraycans * sizeof(candata_t)),
				PU_STATIC, NULL);

			for (i = 0; i < gamedata->numspraycans; i++)
			{
				gamedata->spraycans[i].col = SKINCOLOR_NONE;
				gamedata->spraycans[i].map = NEXTMAP_INVALID;

				UINT16 col = READUINT16(save.p);
				UINT32 _saveid = READUINT32(save.p);

				if (col < SKINCOLOR_FIRSTFREESLOT)
				{
					gamedata->spraycans[i].col = col;
					skincolors[col].cache_spraycan = i;
				}

				if (_saveid >= numgamedatamapheaders)
				{
					// Can has not been grabbed on any map, this is intentional.
					continue;
				}

				UINT16 map = tempmapidreferences[_saveid];
				if (map >= nummapheaders || !mapheaderinfo[map])
				{
					//CONS_Printf("LOAD - Can %u, color %s - id %u (unloaded header)\n", i, skincolors[col].name, _saveid);
					continue;
				}

				//CONS_Printf("LOAD - Can %u, color %s - id %u, map %d\n", i, skincolors[col].name, _saveid, map);

				gamedata->spraycans[i].map = map;

				if (gamedata->gotspraycans != i)
				{
					//CONS_Printf("LOAD - Swapping gotten can %u, color %s with prior ungotten can %u\n", i, skincolors[col].name, gamedata->gotspraycans);

					// All grabbed cans should be at the head of the list.
					// Let's swap with the can the disjoint occoured at.
					// This will prevent a gap from occouring on reload.
					candata_t copycan = gamedata->spraycans[gamedata->gotspraycans];
					gamedata->spraycans[gamedata->gotspraycans] = gamedata->spraycans[i];
					gamedata->spraycans[i] = copycan;

					mapheaderinfo[copycan.map]->cache_spraycan = i;
				}

				mapheaderinfo[map]->cache_spraycan = gamedata->gotspraycans;

				gamedata->gotspraycans++;
			}
		}
		else
		{
			gamedata->spraycans = NULL;
		}
	}

	if (versionMinor > 1)
	{
		numgamedatacups = READUINT32(save.p);

		for (i = 0; i < numgamedatacups; i++)
		{
			char cupname[MAXCUPNAME];
			cupheader_t *cup;

			cupwindata_t dummywindata[4];

			// Find the relevant cup.
			if (versionMinor < 5)
			{
				// Before this version cups were called things like RING.
				// Now that example cup would be called RR_RING instead.
				cupname[0] = cupname[1] = 'R';
				cupname[2] = '_';
				READSTRINGL(save.p, (cupname + 3), sizeof(cupname) - 3);
			}
			else
			{
				READSTRINGL(save.p, cupname, sizeof(cupname));
			}

			UINT32 hash = quickncasehash(cupname, MAXCUPNAME);
			for (cup = kartcupheaders; cup; cup = cup->next)
			{
				if (cup->namehash != hash)
					continue;

				if (strcmp(cup->name, cupname))
					continue;

				break;
			}

			// Digest its data...
			for (j = 0; j < KARTGP_MAX; j++)
			{
				rtemp = READUINT8(save.p);

				dummywindata[j].best_placement = (rtemp & 0x0F);
				dummywindata[j].best_grade = (rtemp & 0x70)>>4;
				dummywindata[j].got_emerald = !!(rtemp & 0x80);

				dummywindata[j].best_skin.id = MAXSKINS;
				dummywindata[j].best_skin.unloaded = NULL;
				if (versionMinor >= 3)
				{
					UINT32 _saveid = READUINT32(save.p);
					if (_saveid < numgamedataskins)
					{
						M_Memcpy(&dummywindata[j].best_skin, &tempskinreferences[_saveid], sizeof(dummywindata[j].best_skin));
					}
				}
			}

			if (versionMinor < 3)
			{
				// We now require backfilling of placement information.	

				cupwindata_t bestwindata;
				bestwindata.best_placement = 0;

				j = KARTGP_MAX;
				while (j > 0)
				{
					j--;

					if (bestwindata.best_placement == 0)
					{
						if (dummywindata[j].best_placement != 0)
						{
							M_Memcpy(&bestwindata, &dummywindata[j], sizeof(bestwindata));
						}
						continue;
					}

					if (dummywindata[j].best_placement != 0)
					{
						if (dummywindata[j].best_placement < bestwindata.best_placement)
							bestwindata.best_placement = dummywindata[j].best_placement;

						if (dummywindata[j].best_grade > bestwindata.best_grade)
							bestwindata.best_grade = dummywindata[j].best_grade;

						bestwindata.got_emerald |= dummywindata[j].got_emerald;
					}

					M_Memcpy(&dummywindata[j], &bestwindata, sizeof(dummywindata[j]));
				}
			}

			if (cup)
			{
				// We found a cup, so assign the windata.

				M_Memcpy(&cup->windata, &dummywindata, sizeof(cup->windata));
			}
			else if (dummywindata[0].best_placement != 0)
			{
				// Invalid, but we don't want to lose all the juicy statistics.
				// Instead, update a FILO linked list of "unloaded cupheaders".

				unloaded_cupheader_t *unloadedcup =
					Z_Malloc(
						sizeof(unloaded_cupheader_t),
						PU_STATIC, NULL
					);

				// Establish properties, for later retrieval on file add.
				strlcpy(unloadedcup->name, cupname, sizeof(unloadedcup->name));
				unloadedcup->namehash = quickncasehash(unloadedcup->name, MAXCUPNAME);

				// Insert at the head, just because it's convenient.
				unloadedcup->next = unloadedcupheaders;
				unloadedcupheaders = unloadedcup;

				// Finally, copy into.
				M_Memcpy(&unloadedcup->windata, &dummywindata, sizeof(unloadedcup->windata));
			}
		}
	}

	if (tempskinreferences)
		Z_Free(tempskinreferences);
	if (tempmapidreferences)
		Z_Free(tempmapidreferences);

	// done
	P_SaveBufferFree(&save);

	finalisegamedata:
	{
		M_FinaliseGameData();

		return;
	}

	// Landing point for corrupt gamedata
	datacorrupt:
	{
		const char *gdfolder = "the Ring Racers folder";
		if (strcmp(srb2home,"."))
			gdfolder = srb2home;

		P_SaveBufferFree(&save);

		I_Error("Corrupt game data file.\nDelete %s(maybe in %s) and try again.", gamedatafilename, gdfolder);
	}
}

// G_DirtyGameData
// Modifies the gamedata as little as possible to maintain safety in a crash event, while still recording it.
void G_DirtyGameData(void)
{
	FILE *handle = NULL;
	const UINT8 writebytesource = true;

	if (gamedata)
		gamedata->evercrashed = true;

	//if (FIL_WriteFileOK(name))
		handle = fopen(va(pandf, srb2home, gamedatafilename), "r+");

	if (!handle)
		return;

	// Write a dirty byte immediately after the gamedata check + minor version.
	if (fseek(handle, 5, SEEK_SET) != -1)
		fwrite(&writebytesource, 1, 1, handle);

	fclose(handle);

	return;
}

// G_SaveGameData
// Saves the main data file, which stores information such as emblems found, etc.
void G_SaveGameData(void)
{
	size_t length;
	INT32 i, j;
	cupheader_t *cup;
	UINT8 btemp;
	savebuffer_t save = {0};

	if (gamedata == NULL || !gamedata->loaded)
		return; // If never loaded (-nodata), don't save

	gamedata->deferredsave = false;

	if (usedCheats)
	{
#ifdef DEVELOP
		CONS_Alert(CONS_WARNING, M_GetText("Cheats used - Gamedata will not be saved.\n"));
#endif
		return;
	}

	length = (4+1+1+
		4+4+
		(4*GDGT_MAX)+
		4+1+2+2+
		4+
		4+
		(MAXEMBLEMS+(MAXUNLOCKABLES*2)+MAXCONDITIONSETS)+
		4+2);

	if (gamedata->challengegrid)
	{
		length += (gamedata->challengegridwidth * CHALLENGEGRIDHEIGHT) * 2;
	}


	UINT32 numgamedataskins = 0;
	unloaded_skin_t *unloadedskin;

	for (i = 0; i < numskins; i++)
	{
		// It's safe to assume a skin with no wins will have no other data worth keeping
		if (skins[i].records.wins == 0)
		{
			continue;
		}

		numgamedataskins++;
	}

	for (unloadedskin = unloadedskins; unloadedskin; unloadedskin = unloadedskin->next)
	{
		// Ditto, with the exception that we should warn about it.
		if (unloadedskin->records.wins == 0)
		{
			CONS_Alert(CONS_WARNING, "Unloaded skin \"%s\" has no wins!\n", unloadedskin->name);
			continue;
		}

		numgamedataskins++;
	}

	length += 4 + (numgamedataskins * (SKINNAMESIZE+4));


	UINT32 numgamedatamapheaders = 0;
	unloaded_mapheader_t *unloadedmap;

	for (i = 0; i < nummapheaders; i++)
	{
		// No spraycan attached.
		if (mapheaderinfo[i]->cache_spraycan >= gamedata->numspraycans
		// It's safe to assume a level with no mapvisited will have no other data worth keeping, since you get MV_VISITED just for opening it.
		&& !(mapheaderinfo[i]->records.mapvisited & MV_MAX))
		{
			mapheaderinfo[i]->_saveid = UINT32_MAX;
			continue;
		}

		mapheaderinfo[i]->_saveid = numgamedatamapheaders;
		numgamedatamapheaders++;
	}

	for (unloadedmap = unloadedmapheaders; unloadedmap; unloadedmap = unloadedmap->next)
	{
		// Ditto, with the exception that we should warn about it.
		if (!(unloadedmap->records.mapvisited & MV_MAX))
		{
			CONS_Alert(CONS_WARNING, "Unloaded map \"%s\" has no mapvisited!\n", unloadedmap->lumpname);
			continue;
		}

		// It's far off on the horizon, beyond many memory limits, but prevent a potential misery moment of losing ALL your data.
		if (++numgamedatamapheaders == UINT32_MAX)
		{
			CONS_Alert(CONS_WARNING, "Some unloaded map record data has been dropped due to datatype limitations.\n");
			break;
		}
	}

	length += 4 + (numgamedatamapheaders * (MAXMAPLUMPNAME+1+4+4));

	length += 2;

	if (gamedata->numspraycans)
	{
		length += (gamedata->numspraycans * (2 + 4));
	}


	UINT32 numgamedatacups = 0;
	unloaded_cupheader_t *unloadedcup;

	for (cup = kartcupheaders; cup; cup = cup->next)
	{
		// Results are populated downwards, so no Easy win
		// means there's no important player data to save.
		if (cup->windata[0].best_placement == 0)
			continue;

		numgamedatacups++;
	}

	for (unloadedcup = unloadedcupheaders; unloadedcup; unloadedcup = unloadedcup->next)
	{
		// Ditto, with the exception that we should warn about it.
		if (unloadedcup->windata[0].best_placement == 0)
		{
			CONS_Alert(CONS_WARNING, "Unloaded cup \"%s\" has no windata!\n", unloadedcup->name);
			continue;
		}

		// It's far off on the horizon, beyond many memory limits, but prevent a potential misery moment of losing ALL your data.
		if (++numgamedatacups == UINT32_MAX)
		{
			CONS_Alert(CONS_WARNING, "Some unloaded cup standings data has been dropped due to datatype limitations.\n");
			break;
		}
	}

	length += 4 + (numgamedatacups * (MAXCUPNAME + 4*(1+4)));


	if (P_SaveBufferAlloc(&save, length) == false)
	{
		CONS_Alert(CONS_ERROR, M_GetText("No more free memory for saving game data\n"));
		return;
	}

	// Version test

	WRITEUINT32(save.p, GD_VERSIONCHECK); // 4
	WRITEUINT8(save.p, GD_VERSIONMINOR); // 1

	// Crash dirtiness
	// cannot move, see G_DirtyGameData
	WRITEUINT8(save.p, gamedata->evercrashed); // 1

	// Statistics

	WRITEUINT32(save.p, gamedata->totalplaytime); // 4
	WRITEUINT32(save.p, gamedata->totalrings); // 4

	for (i = 0; i < GDGT_MAX; i++) // 4 * GDGT_MAX
	{
		WRITEUINT32(save.p, gamedata->roundsplayed[i]);
	}

	WRITEUINT32(save.p, gamedata->pendingkeyrounds); // 4
	WRITEUINT8(save.p, gamedata->pendingkeyroundoffset); // 1
	WRITEUINT16(save.p, gamedata->keyspending); // 2
	WRITEUINT16(save.p, gamedata->chaokeys); // 2

	{
		UINT32 everflags = 0;

		if (gamedata->everloadedaddon)
			everflags |= GDEVER_ADDON;
		if (gamedata->everfinishedcredits)
			everflags |= GDEVER_CREDITS;
		if (gamedata->eversavedreplay)
			everflags |= GDEVER_REPLAY;
		if (gamedata->everseenspecial)
			everflags |= GDEVER_SPECIAL;

		WRITEUINT32(save.p, everflags); // 4
	}

	WRITEUINT32(save.p, quickncasehash(timeattackfolder, 64)); // 4

	// To save space, use one bit per collected/achieved/unlocked flag
	for (i = 0; i < MAXEMBLEMS;) // MAXEMBLEMS * 1;
	{
		btemp = 0;
		for (j = 0; j < 8 && j+i < MAXEMBLEMS; ++j)
			btemp |= (gamedata->collected[j+i] << j);
		WRITEUINT8(save.p, btemp);
		i += j;
	}

	// MAXUNLOCKABLES * 2;
	for (i = 0; i < MAXUNLOCKABLES;)
	{
		btemp = 0;
		for (j = 0; j < 8 && j+i < MAXUNLOCKABLES; ++j)
			btemp |= (gamedata->unlocked[j+i] << j);
		WRITEUINT8(save.p, btemp);
		i += j;
	}
	for (i = 0; i < MAXUNLOCKABLES;)
	{
		btemp = 0;
		for (j = 0; j < 8 && j+i < MAXUNLOCKABLES; ++j)
			btemp |= (gamedata->unlockpending[j+i] << j);
		WRITEUINT8(save.p, btemp);
		i += j;
	}

	for (i = 0; i < MAXCONDITIONSETS;) // MAXCONDITIONSETS * 1;
	{
		btemp = 0;
		for (j = 0; j < 8 && j+i < MAXCONDITIONSETS; ++j)
			btemp |= (gamedata->achieved[j+i] << j);
		WRITEUINT8(save.p, btemp);
		i += j;
	}

	if (gamedata->challengegrid) // 2 + (gamedata->challengegridwidth * CHALLENGEGRIDHEIGHT) * 2
	{
		WRITEUINT16(save.p, gamedata->challengegridwidth);
		for (i = 0; i < (gamedata->challengegridwidth * CHALLENGEGRIDHEIGHT); i++)
		{
			WRITEUINT16(save.p, gamedata->challengegrid[i]);
		}
	}
	else // 2
	{
		WRITEUINT16(save.p, 0);
	}

	WRITEUINT32(save.p, gamedata->timesBeaten); // 4

	// Main records

	WRITEUINT32(save.p, numgamedataskins); // 4

	{
		// numgamedataskins * (SKINNAMESIZE+4)

		UINT32 maxid = 0;

		for (i = 0; i < numskins; i++)
		{
			if (skins[i].records.wins == 0)
			{
				skins[i].records._saveid = UINT32_MAX;
				continue;
			}

			WRITESTRINGN(save.p, skins[i].name, SKINNAMESIZE);

			WRITEUINT32(save.p, skins[i].records.wins);

			skins[i].records._saveid = maxid;
			if (++maxid == numgamedataskins)
				break;
		}

		if (maxid < numgamedataskins)
		{
			for (unloadedskin = unloadedskins; unloadedskin; unloadedskin = unloadedskin->next)
			{
				if (unloadedskin->records.wins == 0)
					continue;

				WRITESTRINGN(save.p, unloadedskin->name, SKINNAMESIZE);

				WRITEUINT32(save.p, unloadedskin->records.wins);

				unloadedskin->records._saveid = maxid;
				if (++maxid == numgamedataskins)
					break;
			}
		}
	}

#define GETSKINREFSAVEID(ref, var) \
	{ \
		if (ref.unloaded != NULL) \
			var = ref.unloaded->records._saveid;\
		else if (ref.id < numskins)\
			var = skins[ref.id].records._saveid; \
		else \
			var = UINT32_MAX; \
	}

	WRITEUINT32(save.p, numgamedatamapheaders); // 4

	if (numgamedatamapheaders)
	{
		// numgamedatamapheaders * (MAXMAPLUMPNAME+1+4+4)

		for (i = 0; i < nummapheaders; i++)
		{
			if (mapheaderinfo[i]->cache_spraycan >= gamedata->numspraycans
			&& !(mapheaderinfo[i]->records.mapvisited & MV_MAX))
				continue;

			WRITESTRINGL(save.p, mapheaderinfo[i]->lumpname, MAXMAPLUMPNAME);

			UINT8 mapvisitedtemp = (mapheaderinfo[i]->records.mapvisited & MV_MAX);

			if ((mapheaderinfo[i]->menuflags & (LF2_FINISHNEEDED|LF2_HIDEINMENU)))
			{
				mapvisitedtemp |= MV_FINISHNEEDED;
			}

			WRITEUINT8(save.p, mapvisitedtemp);

			WRITEUINT32(save.p, mapheaderinfo[i]->records.time);
			WRITEUINT32(save.p, mapheaderinfo[i]->records.lap);

			if (--numgamedatamapheaders == 0)
				break;
		}

		if (numgamedatamapheaders)
		{
			for (unloadedmap = unloadedmapheaders; unloadedmap; unloadedmap = unloadedmap->next)
			{
				if (!(unloadedmap->records.mapvisited & MV_MAX))
					continue;

				WRITESTRINGL(save.p, unloadedmap->lumpname, MAXMAPLUMPNAME);

				WRITEUINT8(save.p, unloadedmap->records.mapvisited);

				WRITEUINT32(save.p, unloadedmap->records.time);
				WRITEUINT32(save.p, unloadedmap->records.lap);

				if (--numgamedatamapheaders == 0)
					break;
			}
		}
	}
	
	WRITEUINT16(save.p, gamedata->numspraycans); // 2

	// gamedata->numspraycans * (2 + 4)

	for (i = 0; i < gamedata->numspraycans; i++)
	{
		WRITEUINT16(save.p, gamedata->spraycans[i].col);

		UINT32 _saveid = UINT32_MAX;

		UINT16 map = gamedata->spraycans[i].map;

		if (map < nummapheaders && mapheaderinfo[map])
		{
			_saveid = mapheaderinfo[map]->_saveid;
		}

		//CONS_Printf("SAVE - Can %u, color %s - id %u, map %d\n", i, skincolors[gamedata->spraycans[i].col].name, _saveid, map);

		WRITEUINT32(save.p, _saveid);
	}

	WRITEUINT32(save.p, numgamedatacups); // 4

	if (numgamedatacups)
	{
		// numgamedatacups * (MAXCUPNAME + 4*(1+4))

#define WRITECUPWINDATA(maybeunloadedcup) \
		for (i = 0; i < KARTGP_MAX; i++) \
		{ \
			btemp = min(maybeunloadedcup->windata[i].best_placement, 0x0F); \
			btemp |= (maybeunloadedcup->windata[i].best_grade<<4); \
			if (maybeunloadedcup->windata[i].got_emerald == true) \
				btemp |= 0x80; \
			\
			WRITEUINT8(save.p, btemp); \
			\
			GETSKINREFSAVEID(maybeunloadedcup->windata[i].best_skin, j); \
			\
			WRITEUINT32(save.p, j); \
		}

		for (cup = kartcupheaders; cup; cup = cup->next)
		{
			if (cup->windata[0].best_placement == 0)
				continue;

			WRITESTRINGL(save.p, cup->name, MAXCUPNAME);

			WRITECUPWINDATA(cup);

			if (--numgamedatacups == 0)
				break;
		}

		if (numgamedatacups)
		{
			for (unloadedcup = unloadedcupheaders; unloadedcup; unloadedcup = unloadedcup->next)
			{
				if (unloadedcup->windata[0].best_placement == 0)
					continue;

				WRITESTRINGL(save.p, unloadedcup->name, MAXCUPNAME);

				WRITECUPWINDATA(unloadedcup);

				if (--numgamedatacups == 0)
					break;
			}
		}

#undef WRITECUPWINDATA
	}

#undef GETSKINREFSAVEID

	length = save.p - save.buffer;

	FIL_WriteFile(va(pandf, srb2home, gamedatafilename), save.buffer, length);
	P_SaveBufferFree(&save);

	// Also save profiles here.
	PR_SaveProfiles();
}

#define VERSIONSIZE 16

//
// G_InitFromSavegame
// Can be called by the startup code or the menu task.
//

#define SAV_VERSIONMINOR 2

void G_LoadGame(void)
{
	char vcheck[VERSIONSIZE+1];
	char savename[255];
	UINT8 versionMinor;
	savebuffer_t save = {0};

	// memset savedata to all 0, fixes calling perfectly valid saves corrupt because of bots
	memset(&savedata, 0, sizeof(savedata));

	//if (makelivebackup)
		strcpy(savename, gpbackup);
	//else
		//sprintf(savename, savegamename, cursaveslot);

	if (P_SaveBufferFromFile(&save, savename) == false)
	{
		CONS_Printf(M_GetText("Couldn't read file %s\n"), savename);
		return;
	}

	versionMinor = READUINT8(save.p);

	memset(vcheck, 0, sizeof (vcheck));
	sprintf(vcheck, "version %d", VERSION);

	if (versionMinor != SAV_VERSIONMINOR
	|| memcmp(save.p, vcheck, VERSIONSIZE))
	{
		M_StartMessage("Savegame Load", va(M_GetText("Savegame %s is from\na different version."), savename), NULL, MM_NOTHING, NULL, NULL);
		P_SaveBufferFree(&save);
		return; // bad version
	}
	save.p += VERSIONSIZE;

	if (demo.playback) // reset game engine
		G_StopDemo();

//	paused = false;
//	automapactive = false;

	// dearchive all the modifications
	if (!P_LoadGame(&save))
	{
		M_StartMessage("Savegame Load", va(M_GetText("Savegame %s could not be loaded.\n"
		"Check the console log for more info.\n"), savename), NULL, MM_NOTHING, NULL, NULL);
		Z_Free(save.buffer);
		save.p = save.buffer = NULL;

		return;
	}

	// done
	P_SaveBufferFree(&save);

	CON_ToggleOff();
}

void G_GetBackupCupData(boolean actuallygetdata)
{
	if (actuallygetdata == false)
	{
		cupsavedata.cup = NULL;
		return;
	}

	char vcheck[VERSIONSIZE+1];
	char savename[255];
	UINT8 versionMinor;
	savebuffer_t save = {0};

	//if (makelivebackup)
		strcpy(savename, gpbackup);
	//else
		//sprintf(savename, savegamename, cursaveslot);

	if (P_SaveBufferFromFile(&save, savename) == false)
	{
		cupsavedata.cup = NULL;
		return;
	}

	versionMinor = READUINT8(save.p);

	memset(vcheck, 0, sizeof (vcheck));
	sprintf(vcheck, "version %d", VERSION);

	if (versionMinor != SAV_VERSIONMINOR
	|| memcmp(save.p, vcheck, VERSIONSIZE))
	{
		cupsavedata.cup = NULL;
		P_SaveBufferFree(&save);
		return; // bad version
	}
	save.p += VERSIONSIZE;

	P_GetBackupCupData(&save);

	if (cv_dummygpdifficulty.value != cupsavedata.difficulty
	|| !!cv_dummygpencore.value != cupsavedata.encore)
	{
		// Still not compatible.
		cupsavedata.cup = NULL;
	}

	// done
	P_SaveBufferFree(&save);
}

//
// G_SaveGame
// Saves your game.
//
void G_SaveGame(void)
{
	boolean saved;
	char savename[256] = "";
	savebuffer_t save = {0};

	//if (makelivebackup)
		strcpy(savename, gpbackup);
	//else
		//sprintf(savename, savegamename, cursaveslot);

	gameaction = ga_nothing;
	{
		char name[VERSIONSIZE+1];
		size_t length;

		if (P_SaveBufferAlloc(&save, SAVEGAMESIZE) == false)
		{
			CONS_Alert(CONS_ERROR, M_GetText("No more free memory for saving game data\n"));
			return;
		}

		WRITEUINT8(save.p, SAV_VERSIONMINOR);

		memset(name, 0, sizeof (name));
		sprintf(name, "version %d", VERSION);
		WRITEMEM(save.p, name, VERSIONSIZE);

		P_SaveGame(&save);

		length = save.p - save.buffer;
		saved = FIL_WriteFile(savename, save.buffer, length);
		P_SaveBufferFree(&save);
	}

	gameaction = ga_nothing;

	if (cht_debug && saved)
		CONS_Printf(M_GetText("%s saved.\n"), savename);
	else if (!saved)
		CONS_Alert(CONS_ERROR, M_GetText("Error while writing to %s\n"), savename);
}

//
// G_DeferedInitNew
// Can be called by the startup code or the menu task,
// consoleplayer, displayplayers[], playeringame[] should be set.
//
void G_DeferedInitNew(boolean pencoremode, INT32 map, INT32 pickedchar, UINT8 ssplayers, boolean FLS)
{
	UINT16 color = SKINCOLOR_NONE;

	paused = false;

	if (demo.playback)
		COM_BufAddText("stopdemo\n");

	G_FreeGhosts(); // TODO: do we actually need to do this?

	// this leave the actual game if needed
	SV_StartSinglePlayerServer(gametype, false);

	if (splitscreen != ssplayers)
	{
		splitscreen = ssplayers;
		SplitScreen_OnChange();
	}

	SetPlayerSkinByNum(consoleplayer, pickedchar);
	CV_StealthSet(&cv_skin[0], skins[pickedchar].name);

	if (color != SKINCOLOR_NONE)
	{
		CV_StealthSetValue(&cv_playercolor[0], color);
	}

	D_MapChange(map, gametype, pencoremode, true, 1, false, FLS);
}

//
// This is the map command interpretation something like Command_Map_f
//
// called at: map cmd execution, doloadgame, doplaydemo
void G_InitNew(UINT8 pencoremode, INT32 map, boolean resetplayer, boolean skipprecutscene)
{
	const char * mapname = G_BuildMapName(map);

	INT32 i;

	if (paused)
	{
		paused = false;
		S_ResumeAudio();
	}

	prevencoremode = ((!Playing()) ? false : encoremode);
	encoremode = pencoremode;

	legitimateexit = false; // SRB2Kart
	comebackshowninfo = false;

	if (!demo.playback && !netgame) // Netgame sets random seed elsewhere, demo playback sets seed just before us!
		P_ClearRandom(M_RandomizedSeed()); // Use a more "Random" random seed

	// Clear a bunch of variables
	redscore = bluescore = lastmap = 0;
	racecountdown = exitcountdown = musiccountdown = mapreset = exitfadestarted = 0;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		players[i].playerstate = PST_REBORN;
		memset(&players[i].respawn, 0, sizeof (players[i].respawn));

		players[i].roundscore = 0;

		if (resetplayer && !demo.playback) // SRB2Kart
		{
			players[i].lives = 3;
			players[i].xtralife = 0;
			players[i].totalring = 0;
			players[i].score = 0;
		}
	}

	// clear itemfinder, just in case
	if (!dedicated)	// except in dedicated servers, where it is not registered and can actually I_Error debug builds
		CV_StealthSetValue(&cv_itemfinder, 0);

	// internal game map
	// well this check is useless because it is done before (d_netcmd.c::command_map_f)
	// but in case of for demos....
	if (!mapname)
	{
		I_Error("Internal game map with ID %d not found\n", map);
		Command_ExitGame_f();
		return;
	}
	if (mapheaderinfo[map-1]->lumpnum == LUMPERROR)
	{
		I_Error("Internal game map '%s' not found\n", mapname);
		Command_ExitGame_f();
		return;
	}

	if (map == G_MapNumber(podiummap)+1)
	{
		// Didn't want to do this, but it needs to be here
		// for it to work on startup.
		if (K_StartCeremony() == true)
		{
			return;
		}
	}

	gamemap = map;

	maptol = mapheaderinfo[gamemap-1]->typeoflevel;
	globalweather = mapheaderinfo[gamemap-1]->weather;

	// Don't carry over custom music change to another map.
	mapmusflags |= MUSIC_RELOADRESET;

	automapactive = false;
	imcontinuing = false;

	if ((grandprixinfo.gp == true) && !skipprecutscene && mapheaderinfo[gamemap-1]->precutscenenum && !modeattacking && !(marathonmode & MA_NOCUTSCENES)) // Start a custom cutscene.
		F_StartCustomCutscene(mapheaderinfo[gamemap-1]->precutscenenum-1, true, resetplayer);
	else
	{
		LUA_HookInt(gamemap, HOOK(MapChange));
		G_DoLoadLevel(resetplayer);
	}

	if (netgame)
	{
		char *title = G_BuildMapTitle(gamemap);

		CON_LogMessage(va(M_GetText("Map is now \"%s"), mapname));
		if (title)
		{
			CON_LogMessage(va(": %s", title));
			Z_Free(title);
		}
		CON_LogMessage("\"\n");
	}

	G_AddMapToBuffer(gamemap - 1);
}


char *G_BuildMapTitle(INT32 mapnum)
{
	char *title = NULL;

	if (!mapnum || mapnum > nummapheaders || !mapheaderinfo[mapnum-1])
		I_Error("G_BuildMapTitle: Internal map ID %d not found (nummapheaders = %d)", mapnum-1, nummapheaders);

	if (strcmp(mapheaderinfo[mapnum-1]->lvlttl, ""))
	{
		size_t len = 1;
		const char *zonetext = NULL;
		const UINT8 actnum = mapheaderinfo[mapnum-1]->actnum;

		len += strlen(mapheaderinfo[mapnum-1]->lvlttl);
		if (strlen(mapheaderinfo[mapnum-1]->zonttl) > 0)
		{
			zonetext = M_GetText(mapheaderinfo[mapnum-1]->zonttl);
			len += strlen(zonetext) + 1;	// ' ' + zonetext
		}
		else if (!(mapheaderinfo[mapnum-1]->levelflags & LF_NOZONE))
		{
			zonetext = M_GetText("Zone");
			len += strlen(zonetext) + 1;	// ' ' + zonetext
		}

		if (actnum > 0)
			len += 1 + 11;					// ' ' + INT32

		title = Z_Malloc(len, PU_STATIC, NULL);

		sprintf(title, "%s", mapheaderinfo[mapnum-1]->lvlttl);
		if (zonetext) sprintf(title + strlen(title), " %s", zonetext);
		if (actnum > 0) sprintf(title + strlen(title), " %d", actnum);
	}

	return title;
}

static void measurekeywords(mapsearchfreq_t *fr,
		struct searchdim **dimp, UINT8 *cuntp,
		const char *s, const char *q, boolean wanttable)
{
	char *qp;
	char *sp;
	if (wanttable)
		(*dimp) = Z_Realloc((*dimp), 255 * sizeof (struct searchdim),
				PU_STATIC, NULL);
	for (qp = strtok(va("%s", q), " ");
			qp && fr->total < 255;
			qp = strtok(0, " "))
	{
		if (( sp = strcasestr(s, qp) ))
		{
			if (wanttable)
			{
				(*dimp)[(*cuntp)].pos = sp - s;
				(*dimp)[(*cuntp)].siz = strlen(qp);
			}
			(*cuntp)++;
			fr->total++;
		}
	}
	if (wanttable)
		(*dimp) = Z_Realloc((*dimp), (*cuntp) * sizeof (struct searchdim),
				PU_STATIC, NULL);
}

static void writesimplefreq(mapsearchfreq_t *fr, INT32 *frc,
		INT32 mapnum, UINT8 pos, UINT8 siz)
{
	fr[(*frc)].mapnum = mapnum;
	fr[(*frc)].matchd = ZZ_Alloc(sizeof (struct searchdim));
	fr[(*frc)].matchd[0].pos = pos;
	fr[(*frc)].matchd[0].siz = siz;
	fr[(*frc)].matchc = 1;
	fr[(*frc)].total = 1;
	(*frc)++;
}

INT32 G_FindMap(const char *mapname, char **foundmapnamep,
		mapsearchfreq_t **freqp, INT32 *freqcp)
{
	INT32 newmapnum = 0;
	INT32 mapnum;
	INT32 apromapnum = 0;

	size_t      mapnamelen;
	char   *realmapname = NULL;
	char   *newmapname = NULL;
	char   *apromapname = NULL;
	char   *aprop = NULL;

	mapsearchfreq_t *freq;
	boolean wanttable;
	INT32 freqc;
	UINT8 frequ;

	INT32 i;

	mapnamelen = strlen(mapname);

	freq = ZZ_Calloc(nummapheaders * sizeof (mapsearchfreq_t));

	wanttable = !!( freqp );

	freqc = 0;
	for (i = 0, mapnum = 1; i < nummapheaders; ++i, ++mapnum)
	{
		if (!mapheaderinfo[i] || mapheaderinfo[i]->lumpnum == LUMPERROR)
			continue;

		if (!( realmapname = G_BuildMapTitle(mapnum) ))
			continue;

		aprop = realmapname;

		/* Now that we found a perfect match no need to fucking guess. */
		if (strnicmp(realmapname, mapname, mapnamelen) == 0)
		{
			if (wanttable)
			{
				writesimplefreq(freq, &freqc, mapnum, 0, mapnamelen);
			}
			if (newmapnum == 0)
			{
				newmapnum = mapnum;
				newmapname = realmapname;
				realmapname = 0;
				Z_Free(apromapname);
				if (!wanttable)
					break;
			}
		}
		else
		if (apromapnum == 0 || wanttable)
		{
			/* LEVEL 1--match keywords verbatim */
			if (( aprop = strcasestr(realmapname, mapname) ))
			{
				if (wanttable)
				{
					writesimplefreq(freq, &freqc,
							mapnum, aprop - realmapname, mapnamelen);
				}
				if (apromapnum == 0)
				{
					apromapnum = mapnum;
					apromapname = realmapname;
					realmapname = 0;
				}
			}
			else/* ...match individual keywords */
			{
				freq[freqc].mapnum = mapnum;
				measurekeywords(&freq[freqc],
						&freq[freqc].matchd, &freq[freqc].matchc,
						realmapname, mapname, wanttable);
				measurekeywords(&freq[freqc],
						&freq[freqc].keywhd, &freq[freqc].keywhc,
						mapheaderinfo[i]->keywords, mapname, wanttable);
				if (freq[freqc].total)
					freqc++;
			}
		}

		Z_Free(realmapname);/* leftover old name */
	}

	if (newmapnum == 0)/* no perfect match--try a substring */
	{
		newmapnum = apromapnum;
		newmapname = apromapname;
	}

	if (newmapnum == 0)/* calculate most queries met! */
	{
		frequ = 0;
		for (i = 0; i < freqc; ++i)
		{
			if (freq[i].total > frequ)
			{
				frequ = freq[i].total;
				newmapnum = freq[i].mapnum;
			}
		}
		if (newmapnum)
		{
			newmapname = G_BuildMapTitle(newmapnum);
		}
	}

	if (freqp)
		(*freqp) = freq;
	else
		Z_Free(freq);

	if (freqcp)
		(*freqcp) = freqc;

	if (foundmapnamep)
		(*foundmapnamep) = newmapname;
	else
		Z_Free(newmapname);

	return newmapnum;
}

void G_FreeMapSearch(mapsearchfreq_t *freq, INT32 freqc)
{
	INT32 i;
	for (i = 0; i < freqc; ++i)
	{
		Z_Free(freq[i].matchd);
	}
	Z_Free(freq);
}

INT32 G_FindMapByNameOrCode(const char *mapname, char **realmapnamep)
{
	INT32 newmapnum;

	char *p;

	/* Now detect map number in base 10, which no one asked for. */
	newmapnum = strtol(mapname, &p, 10);

	if (*p == '\0')/* we got it */
	{
		if (newmapnum < 1 || newmapnum > nummapheaders)
			return 0;
		if (!mapheaderinfo[newmapnum-1] || mapheaderinfo[newmapnum-1]->lumpnum == LUMPERROR)
			return 0;
	}
	else
	{
		newmapnum = G_MapNumber(mapname)+1;

		if (newmapnum > nummapheaders)
			return G_FindMap(mapname, realmapnamep, NULL, NULL);
	}

	if (realmapnamep)
		(*realmapnamep) = G_BuildMapTitle(newmapnum);

	return newmapnum;
}

//
// G_SetGamestate
//
// Use this to set the gamestate, please.
//
void G_SetGamestate(gamestate_t newstate)
{
	gamestate = newstate;
#ifdef HAVE_DISCORDRPC
	DRPC_UpdatePresence();
#endif
}

boolean G_GamestateUsesLevel(void)
{
	switch (gamestate)
	{
		case GS_TITLESCREEN:
			return titlemapinaction;

		case GS_LEVEL:
		case GS_CEREMONY:
			return true;

		default:
			return false;
	}
}

/* These functions handle the exitgame flag. Before, when the user
   chose to end a game, it happened immediately, which could cause
   crashes if the game was in the middle of something. Now, a flag
   is set, and the game can then be stopped when it's safe to do
   so.
*/

// Used as a callback function.
void G_SetExitGameFlag(void)
{
	exitgame = true;
}

void G_ClearExitGameFlag(void)
{
	exitgame = false;
}

boolean G_GetExitGameFlag(void)
{
	return exitgame;
}

// Same deal with retrying.
void G_SetRetryFlag(void)
{
	if (retrying == false)
	{
		grandprixinfo.rank.continuesUsed++;
	}

	retrying = true;
}

void G_ClearRetryFlag(void)
{
	retrying = false;
}

boolean G_GetRetryFlag(void)
{
	return retrying;
}

void G_SetModeAttackRetryFlag(void)
{
	retryingmodeattack = true;
	G_SetRetryFlag();
}

void G_ClearModeAttackRetryFlag(void)
{
	retryingmodeattack = false;
}

boolean G_GetModeAttackRetryFlag(void)
{
	return retryingmodeattack;
}

// Time utility functions
INT32 G_TicsToHours(tic_t tics)
{
	return tics/(3600*TICRATE);
}

INT32 G_TicsToMinutes(tic_t tics, boolean full)
{
	if (full)
		return tics/(60*TICRATE);
	else
		return tics/(60*TICRATE)%60;
}

INT32 G_TicsToSeconds(tic_t tics)
{
	return (tics/TICRATE)%60;
}

INT32 G_TicsToCentiseconds(tic_t tics)
{
	return (INT32)((tics%TICRATE) * (100.00f/TICRATE));
}

INT32 G_TicsToMilliseconds(tic_t tics)
{
	return (INT32)((tics%TICRATE) * (1000.00f/TICRATE));
}
